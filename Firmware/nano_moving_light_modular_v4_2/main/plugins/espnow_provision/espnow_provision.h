#pragma once

// ── ESP-NOW Provisioning Plugin ───────────────────────────────────
//
// SENDER  (device with WiFi): listens for SEEKER beacons → broadcasts
//         encrypted SSID+PW → seekers connect on next reboot.
// SEEKER  (no WiFi creds):    beacons every PROVISION_BEACON_MS →
//         receives payload → decrypts → saves /wifi_provision.json → reboot.
//
// Call espnow_provision_pre_wifi() in setup() BEFORE wifi_connectMulti().
// Add #ifdef block to wifi_connectMulti() to try /wifi_provision.json first.
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "../storage/storage.h"
#include "espnow_provision_config.h"
#include "nonce_cache.h"
#include "crypto.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_arduino_version.h>

// ── Key buffers ───────────────────────────────────────────────────
// PROVISION_KEY = 32 hex chars → 16 bytes AES key.
// HMAC key = AES key repeated twice (32 bytes total).
static uint8_t _prov_keyAes[16]  = {};
static uint8_t _prov_keyHmac[32] = {};

static bool _prov_keyParsed = false;

static bool _prov_parseKey() {
    if (_prov_keyParsed) return true;
    const char* hex = PROVISION_KEY;
    if (strlen(hex) != 32) return false;
    for (int i = 0; i < 16; i++) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(hex[i * 2]), lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        _prov_keyAes[i] = (uint8_t)((hi << 4) | lo);
    }
    memcpy(_prov_keyHmac,      _prov_keyAes, 16);
    memcpy(_prov_keyHmac + 16, _prov_keyAes, 16);
    _prov_keyParsed = true;
    return true;
}

// ── Shared SEEKER state ───────────────────────────────────────────
static volatile bool _prov_provisioned = false;
static char _prov_rxSSID[33] = {};
static char _prov_rxPass[65] = {};

// ── SENDER task resources (static — no heap after init) ───────────
enum _ProvMsgType : uint8_t { PROV_MSG_BEACON = 1, PROV_MSG_STOP = 2 };
struct _ProvMsg {
    _ProvMsgType type;
    uint8_t      srcMac[6];
    uint8_t      raw[16];
};

static StaticTask_t  _prov_taskBuf;
static StackType_t   _prov_taskStack[4096 / sizeof(StackType_t)];
static StaticQueue_t _prov_queueBuf;
static uint8_t       _prov_queueStorage[8 * sizeof(_ProvMsg)];
static QueueHandle_t _prov_queue    = nullptr;
static TaskHandle_t  _prov_task     = nullptr;
static NonceCache    _prov_nonces;
static unsigned long _prov_lastBeaconMs = 0;
static bool          _prov_isSender     = false;

// Credentials the SENDER broadcasts
static char _prov_txSSID[33] = {};
static char _prov_txPass[65] = {};

// ── Helpers ───────────────────────────────────────────────────────
static bool _prov_addBroadcastPeer(uint8_t channel) {
    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    if (esp_now_is_peer_exist(bcast)) return true;
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, bcast, 6);
    p.channel = channel;
    p.ifidx   = WIFI_IF_STA;
    p.encrypt  = false;
    return esp_now_add_peer(&p) == ESP_OK;
}

static void _prov_buildBeacon(uint8_t out[16]) {
    out[0] = 0xE5; out[1] = 0x50;
    out[2] = 0x01;  // version
    out[3] = 0x02;  // role = SEEKER
    uint32_t nonce = esp_random();
    out[4] = (nonce >> 24) & 0xFF;
    out[5] = (nonce >> 16) & 0xFF;
    out[6] = (nonce >>  8) & 0xFF;
    out[7] = (nonce      ) & 0xFF;
    uint8_t hmac[32];
    crypto_hmac_sha256(_prov_keyHmac, 32, out, 8, hmac);
    memcpy(out + 8, hmac, 8);
}

// Returns total payload length or 0 on error. buf must be ≥ 220 bytes.
static size_t _prov_buildPayload(uint8_t* buf, size_t bufLen,
                                  const char* ssid, const char* pass) {
    size_t ssidLen  = strlen(ssid);
    size_t passLen  = strlen(pass);
    size_t plainLen = ssidLen + 1 + passLen;  // ssid NUL pass

    // Max cipher: ((plainLen/16)+1)*16 ≤ 128, total ≤ 28+128+32 = 188
    if (plainLen > 97 || 28 + 128 + 32 > (int)bufLen) return 0;

    buf[0] = 0xE5; buf[1] = 0x50;
    buf[2] = 0x01;  // version
    buf[3] = 0x01;  // role = SENDER

    // 8-byte nonce at [4..11]
    uint32_t n1 = esp_random(), n2 = esp_random();
    buf[4]  = (n1 >> 24) & 0xFF; buf[5]  = (n1 >> 16) & 0xFF;
    buf[6]  = (n1 >>  8) & 0xFF; buf[7]  = (n1      ) & 0xFF;
    buf[8]  = (n2 >> 24) & 0xFF; buf[9]  = (n2 >> 16) & 0xFF;
    buf[10] = (n2 >>  8) & 0xFF; buf[11] = (n2      ) & 0xFF;

    // 16-byte IV at [12..27]
    uint8_t* iv = buf + 12;
    uint32_t iv1 = esp_random(), iv2 = esp_random();
    uint32_t iv3 = esp_random(), iv4 = esp_random();
    iv[0]  = (iv1>>24)&0xFF; iv[1]  = (iv1>>16)&0xFF;
    iv[2]  = (iv1>> 8)&0xFF; iv[3]  = (iv1    )&0xFF;
    iv[4]  = (iv2>>24)&0xFF; iv[5]  = (iv2>>16)&0xFF;
    iv[6]  = (iv2>> 8)&0xFF; iv[7]  = (iv2    )&0xFF;
    iv[8]  = (iv3>>24)&0xFF; iv[9]  = (iv3>>16)&0xFF;
    iv[10] = (iv3>> 8)&0xFF; iv[11] = (iv3    )&0xFF;
    iv[12] = (iv4>>24)&0xFF; iv[13] = (iv4>>16)&0xFF;
    iv[14] = (iv4>> 8)&0xFF; iv[15] = (iv4    )&0xFF;

    // Plaintext = ssid \0 pass
    static uint8_t plain[128];
    memcpy(plain, ssid, ssidLen);
    plain[ssidLen] = 0;
    memcpy(plain + ssidLen + 1, pass, passLen);

    uint8_t* cipher = buf + 28;
    size_t cipherLen = crypto_aes128_cbc_encrypt(_prov_keyAes, iv, plain, plainLen, cipher);
    if (!cipherLen) return 0;

    uint8_t hmac[32];
    crypto_hmac_sha256(_prov_keyHmac, 32, buf, 28 + cipherLen, hmac);
    memcpy(buf + 28 + cipherLen, hmac, 32);

    return 28 + cipherLen + 32;
}

// ── SEEKER receive callback ────────────────────────────────────────
// Runs in WiFi task context — minimal work, no blocking calls.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void _prov_seekerRecvCb(const esp_now_recv_info_t*, const uint8_t* data, int len)
#else
static void _prov_seekerRecvCb(const uint8_t*, const uint8_t* data, int len)
#endif
{
    if (_prov_provisioned) return;

    // Min valid payload: 28 header + 16 cipher + 32 HMAC = 76 bytes
    if (len < 76) return;
    if (data[0] != 0xE5 || data[1] != 0x50) return;
    if (data[2] != 0x01) return;  // version
    if (data[3] != 0x01) return;  // role = SENDER

    // Verify HMAC (covers everything except the trailing 32-byte HMAC itself)
    size_t covered = (size_t)len - 32;
    uint8_t expected[32];
    if (!crypto_hmac_sha256(_prov_keyHmac, 32, data, covered, expected)) return;
    if (memcmp(data + covered, expected, 32) != 0) return;

    // Decrypt
    const uint8_t* iv     = data + 12;
    const uint8_t* cipher = data + 28;
    size_t cipherLen = covered - 28;

    static uint8_t plain[160];
    size_t plainLen = crypto_aes128_cbc_decrypt(_prov_keyAes, iv, cipher, cipherLen, plain);
    if (plainLen == 0 || plainLen >= sizeof(plain)) return;
    plain[plainLen] = 0;

    // Parse: ssid \0 pass
    const char* ssid = (const char*)plain;
    size_t ssidLen   = strnlen(ssid, plainLen);
    if (ssidLen == 0 || ssidLen >= plainLen) return;
    const char* pass = ssid + ssidLen + 1;
    size_t passLen   = strnlen(pass, plainLen - ssidLen - 1);
    if (ssidLen > 32 || passLen > 64) return;

    memcpy(_prov_rxSSID, ssid, ssidLen); _prov_rxSSID[ssidLen] = 0;
    memcpy(_prov_rxPass, pass, passLen); _prov_rxPass[passLen] = 0;
    _prov_provisioned = true;
}

// ── SENDER receive callback ────────────────────────────────────────
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void _prov_senderRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len)
#else
static void _prov_senderRecvCb(const uint8_t* mac, const uint8_t* data, int len)
#endif
{
    if (len != 16) return;
    if (data[0] != 0xE5 || data[1] != 0x50) return;
    if (data[2] != 0x01) return;
    if (data[3] != 0x02) return;  // role = SEEKER

    // HMAC over bytes [0..7]
    uint8_t expected[32];
    if (!crypto_hmac_sha256(_prov_keyHmac, 32, data, 8, expected)) return;
    if (memcmp(data + 8, expected, 8) != 0) return;

    // Replay check
    uint32_t nonce = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16)
                   | ((uint32_t)data[6] <<  8) |  (uint32_t)data[7];
    if (_prov_nonces.contains(nonce)) return;
    _prov_nonces.insert(nonce);

    if (!_prov_queue) return;
    _ProvMsg msg = {};
    msg.type = PROV_MSG_BEACON;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    memcpy(msg.srcMac, info->src_addr, 6);
#else
    memcpy(msg.srcMac, mac, 6);
#endif
    memcpy(msg.raw, data, 16);
    xQueueSend(_prov_queue, &msg, pdMS_TO_TICKS(50));
}

// ── SENDER FreeRTOS task ──────────────────────────────────────────
static void _prov_senderTask(void*) {
    static uint8_t payloadBuf[220];
    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    _prov_lastBeaconMs = millis();

    while (true) {
        _ProvMsg msg;
        if (xQueueReceive(_prov_queue, &msg, pdMS_TO_TICKS(1000))) {
            if (msg.type == PROV_MSG_STOP) break;
            _prov_lastBeaconMs = millis();
            size_t len = _prov_buildPayload(payloadBuf, sizeof(payloadBuf),
                                             _prov_txSSID, _prov_txPass);
            if (len > 0)
                esp_now_send(bcast, payloadBuf, len);
            Serial.println("[PROVISION] SENDER payload sent");
        } else {
            if (millis() - _prov_lastBeaconMs > PROVISION_TIMEOUT_MS) {
                Serial.println("[PROVISION] SENDER timeout — deactivating");
                break;
            }
        }
    }

    esp_now_deinit();
    _prov_task  = nullptr;
    _prov_queue = nullptr;
    vTaskDelete(nullptr);
}

// ── Public API ────────────────────────────────────────────────────

// Call in setup() BEFORE wifi_connectMulti().
// If device has no credentials, enters SEEKER mode (blocking up to PROVISION_TIMEOUT_MS).
// On successful provisioning: saves /wifi_provision.json and reboots.
// On timeout: returns, wifi_connectMulti() then starts AP mode normally.
static void espnow_provision_pre_wifi() {
    // Decide if we are a SEEKER: no compile-time networks AND no provisioned file
    bool hasProvFile = false;
    {
        JsonDocument doc;
        if (storage_readJson("/wifi_provision.json", doc)) {
            const char* s = doc["ssid"] | "";
            const char* p = doc["pass"] | "";
            hasProvFile = (strlen(s) > 0 && strlen(p) > 0);
        }
    }
    if (WIFI_NETWORK_COUNT > 0 || hasProvFile) return;

    if (!_prov_parseKey()) {
        Serial.println("[PROVISION] Invalid PROVISION_KEY — SEEKER skipped");
        return;
    }

    Serial.println("[PROVISION] No credentials — SEEKER mode");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    esp_wifi_set_channel(PROVISION_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[PROVISION] esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(_prov_seekerRecvCb);
    _prov_addBroadcastPeer(PROVISION_CHANNEL);

    Serial.printf("[PROVISION] ESP-NOW OK on channel %d — beaconing every %d ms\n",
                  PROVISION_CHANNEL, PROVISION_BEACON_MS);

    unsigned long startMs    = millis();
    unsigned long lastBeacon = 0;

    while (millis() - startMs < PROVISION_TIMEOUT_MS) {
        unsigned long now = millis();

        if (now - lastBeacon >= (unsigned long)PROVISION_BEACON_MS) {
            uint8_t beacon[16];
            _prov_buildBeacon(beacon);
            static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            esp_now_send(bcast, beacon, 16);
            Serial.println("[PROVISION] SEEKER beacon sent");
            lastBeacon = now;
        }

        if (_prov_provisioned) {
            Serial.printf("[PROVISION] Credentials received — SSID: %s\n", _prov_rxSSID);
            JsonDocument doc;
            doc["ssid"] = _prov_rxSSID;
            doc["pass"] = _prov_rxPass;
            esp_now_deinit();
            if (storage_writeJson("/wifi_provision.json", doc)) {
                Serial.println("[PROVISION] Saved — rebooting");
                delay(100);
                ESP.restart();
            } else {
                Serial.println("[PROVISION] Save failed — continuing without credentials");
                return;
            }
        }

        delay(10);
    }

    Serial.println("[PROVISION] SEEKER timeout — falling back to AP mode");
    esp_now_deinit();
}

static void espnow_provision_setup() {
    if (WiFi.status() != WL_CONNECTED) return;  // AP mode: no SENDER needed

    if (!_prov_parseKey()) {
        Serial.println("[PROVISION] Invalid PROVISION_KEY — SENDER skipped");
        return;
    }

    // Find the password for the currently-connected SSID.
    // 1. Check compile-time WIFI_NETWORKS[]
    String connSSID = WiFi.SSID();
    bool found = false;
    for (int i = 0; i < WIFI_NETWORK_COUNT && !found; i++) {
        if (connSSID == WIFI_NETWORKS[i].ssid) {
            strncpy(_prov_txSSID, WIFI_NETWORKS[i].ssid,     sizeof(_prov_txSSID) - 1);
            strncpy(_prov_txPass, WIFI_NETWORKS[i].password, sizeof(_prov_txPass)  - 1);
            found = true;
        }
    }
    // 2. Fall back to /wifi_provision.json (this device was itself provisioned via ESP-NOW)
    if (!found) {
        JsonDocument doc;
        if (storage_readJson("/wifi_provision.json", doc)) {
            const char* s = doc["ssid"] | "";
            const char* p = doc["pass"] | "";
            if (strlen(s) > 0 && strlen(p) > 0 && connSSID == s) {
                strncpy(_prov_txSSID, s, sizeof(_prov_txSSID) - 1);
                strncpy(_prov_txPass, p, sizeof(_prov_txPass)  - 1);
                found = true;
            }
        }
    }
    if (!found) {
        Serial.println("[PROVISION] No credential match for SENDER — disabled");
        return;
    }

    // Use the AP's actual WiFi channel so ESP-NOW packets reach the SEEKER.
    // SEEKER must send on PROVISION_CHANNEL which must match this channel.
    uint8_t ch = (uint8_t)WiFi.channel();

    // Disable WiFi power saving so broadcast ESP-NOW frames from SEEKER are not missed.
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[PROVISION] esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(_prov_senderRecvCb);
    _prov_nonces.init();
    _prov_addBroadcastPeer(ch);

    _prov_queue = xQueueCreateStatic(8, sizeof(_ProvMsg), _prov_queueStorage, &_prov_queueBuf);
    _prov_task  = xTaskCreateStatic(_prov_senderTask, "prov_tx",
                                    4096 / sizeof(StackType_t), nullptr, 5,
                                    _prov_taskStack, &_prov_taskBuf);
    _prov_isSender = true;
    Serial.printf("[PROVISION] SENDER active — SSID '%s' on ch%u\n", _prov_txSSID, ch);
}

static void espnow_provision_loop() {}

static bool espnow_provision_is_sender() { return _prov_isSender; }

REGISTER_PLUGIN(espnow_provision);
