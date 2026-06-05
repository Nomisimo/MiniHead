#pragma once

// ── BLE Provisioning Plugin ───────────────────────────────────────
//
// Replaces the ESP-NOW provisioning transport with Bluetooth GATT.
// BLE uses frequency-hopping FHSS on 2.4 GHz — completely independent
// of the WiFi channel, so the "home channel mismatch" problem disappears.
//
// SENDER  (device with WiFi): starts a BLE GATT server in a background
//         FreeRTOS task. Characteristic value = AES-CBC encrypted SSID+PW
//         with HMAC-SHA256 auth. Advertises for PROVISION_SENDER_MS, then
//         deinits BLE. Rest of setup() continues immediately (non-blocking).
//
// SEEKER  (no WiFi creds): BLE scan loop in ble_provision_pre_wifi().
//         Every BLE_SCAN_SECONDS it looks for the SENDER's service UUID.
//         On find: connects, reads characteristic, verifies HMAC, decrypts,
//         saves /wifi_provision.json, reboots.
//
// Security: same AES-128-CBC + HMAC-SHA256 as the ESP-NOW implementation.
//           PROVISION_KEY (config.h) must match on all devices.
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "../storage/storage.h"
#include "ble_provision_config.h"
#include "crypto.h"

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>

// ── Key buffers ───────────────────────────────────────────────────
// PROVISION_KEY (config.h) = 32 hex chars → 16 bytes AES.
// HMAC key = AES key repeated twice (32 bytes).
static uint8_t _blep_keyAes[16]  = {};
static uint8_t _blep_keyHmac[32] = {};
static bool    _blep_keyParsed   = false;

static bool _blep_parseKey() {
    if (_blep_keyParsed) return true;
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
        _blep_keyAes[i] = (uint8_t)((hi << 4) | lo);
    }
    memcpy(_blep_keyHmac,      _blep_keyAes, 16);
    memcpy(_blep_keyHmac + 16, _blep_keyAes, 16);
    _blep_keyParsed = true;
    return true;
}

// ── Payload builder (SENDER) ──────────────────────────────────────
// Format: magic(2) + version(1) + role(1) + nonce(8) + IV(16) +
//         AES-CBC(ssid\0pass) + HMAC-SHA256(32)
// Total ≤ 28 + 128 + 32 = 188 bytes (well within BLE GATT 512-byte limit).

static size_t _blep_buildPayload(uint8_t* buf, size_t bufLen,
                                  const char* ssid, const char* pass) {
    size_t ssidLen  = strlen(ssid);
    size_t passLen  = strlen(pass);
    size_t plainLen = ssidLen + 1 + passLen;
    if (plainLen > 97 || bufLen < 220) return 0;

    buf[0] = 0xE5; buf[1] = 0x50;
    buf[2] = 0x01;  // version
    buf[3] = 0x01;  // role = SENDER

    uint32_t n1 = esp_random(), n2 = esp_random();
    buf[4]  = (n1>>24)&0xFF; buf[5]  = (n1>>16)&0xFF;
    buf[6]  = (n1>> 8)&0xFF; buf[7]  = (n1    )&0xFF;
    buf[8]  = (n2>>24)&0xFF; buf[9]  = (n2>>16)&0xFF;
    buf[10] = (n2>> 8)&0xFF; buf[11] = (n2    )&0xFF;

    uint8_t* iv = buf + 12;
    uint32_t iv1=esp_random(), iv2=esp_random(), iv3=esp_random(), iv4=esp_random();
    iv[0]=(iv1>>24)&0xFF; iv[1]=(iv1>>16)&0xFF; iv[2]=(iv1>>8)&0xFF; iv[3]=iv1&0xFF;
    iv[4]=(iv2>>24)&0xFF; iv[5]=(iv2>>16)&0xFF; iv[6]=(iv2>>8)&0xFF; iv[7]=iv2&0xFF;
    iv[8]=(iv3>>24)&0xFF; iv[9]=(iv3>>16)&0xFF; iv[10]=(iv3>>8)&0xFF; iv[11]=iv3&0xFF;
    iv[12]=(iv4>>24)&0xFF; iv[13]=(iv4>>16)&0xFF; iv[14]=(iv4>>8)&0xFF; iv[15]=iv4&0xFF;

    static uint8_t plain[128];
    memcpy(plain, ssid, ssidLen);
    plain[ssidLen] = 0;
    memcpy(plain + ssidLen + 1, pass, passLen);

    uint8_t* cipher = buf + 28;
    size_t cipherLen = crypto_aes128_cbc_encrypt(_blep_keyAes, iv, plain, plainLen, cipher);
    if (!cipherLen) return 0;

    uint8_t hmac[32];
    crypto_hmac_sha256(_blep_keyHmac, 32, buf, 28 + cipherLen, hmac);
    memcpy(buf + 28 + cipherLen, hmac, 32);

    return 28 + cipherLen + 32;
}

// ── Payload verifier + decryptor (SEEKER) ────────────────────────
// Returns true and fills ssidOut/passOut on success.
static bool _blep_decodePayload(const uint8_t* data, size_t len,
                                 char* ssidOut, char* passOut) {
    if (len < 76) return false;
    if (data[0] != 0xE5 || data[1] != 0x50) return false;
    if (data[2] != 0x01 || data[3] != 0x01) return false;

    size_t covered = len - 32;
    uint8_t expected[32];
    if (!crypto_hmac_sha256(_blep_keyHmac, 32, data, covered, expected)) return false;
    if (memcmp(data + covered, expected, 32) != 0) {
        Serial.println("[PROVISION] HMAC mismatch — wrong PROVISION_KEY?");
        return false;
    }

    const uint8_t* iv      = data + 12;
    const uint8_t* cipher  = data + 28;
    size_t cipherLen = covered - 28;
    static uint8_t plain[160];
    size_t plainLen = crypto_aes128_cbc_decrypt(_blep_keyAes, iv, cipher, cipherLen, plain);
    if (plainLen == 0 || plainLen >= sizeof(plain)) return false;
    plain[plainLen] = 0;

    const char* ssid = (const char*)plain;
    size_t ssidLen   = strnlen(ssid, plainLen);
    if (ssidLen == 0 || ssidLen >= plainLen) return false;
    const char* pass = ssid + ssidLen + 1;
    size_t passLen   = strnlen(pass, plainLen - ssidLen - 1);
    if (ssidLen > 32 || passLen > 64) return false;

    memcpy(ssidOut, ssid, ssidLen); ssidOut[ssidLen] = 0;
    memcpy(passOut, pass, passLen); passOut[passLen] = 0;
    return true;
}

// ── SENDER — FreeRTOS task ────────────────────────────────────────
static char         _blep_txSSID[33] = {};
static char         _blep_txPass[65] = {};
static StackType_t  _blep_taskStack[8192 / sizeof(StackType_t)];
static StaticTask_t _blep_taskBuf;

static void _blep_senderTask(void*) {
    static uint8_t payload[220];
    size_t len = _blep_buildPayload(payload, sizeof(payload),
                                     _blep_txSSID, _blep_txPass);
    if (!len) {
        Serial.println("[PROVISION] BLE SENDER payload build failed");
        vTaskDelete(nullptr);
        return;
    }

    BLEDevice::init("MiniHead");
    BLEServer*         srv = BLEDevice::createServer();
    BLEService*        svc = srv->createService(BLE_PROV_SERVICE_UUID);
    BLECharacteristic* chr = svc->createCharacteristic(
                                 BLE_PROV_CHAR_UUID,
                                 BLECharacteristic::PROPERTY_READ);
    chr->setValue(payload, len);
    svc->start();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_PROV_SERVICE_UUID);
    adv->setScanResponse(false);
    adv->setMinPreferred(0x06);
    adv->start();

    Serial.printf("[PROVISION] BLE SENDER advertising for %d s (payload %d bytes)\n",
                  PROVISION_SENDER_MS / 1000, (int)len);

    vTaskDelay(pdMS_TO_TICKS(PROVISION_SENDER_MS));

    adv->stop();
    BLEDevice::deinit(true);
    Serial.println("[PROVISION] BLE SENDER done");
    vTaskDelete(nullptr);
}

// ── Public API ────────────────────────────────────────────────────

// Call in setup() BEFORE wifi_connectMulti().
// If device has no credentials, enters SEEKER mode (BLE scan loop,
// blocking up to PROVISION_TIMEOUT_MS).
// On success: saves /wifi_provision.json, reboots.
// On timeout: returns → wifi_connectMulti() starts AP mode or retries.
static void ble_provision_pre_wifi() {
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

    if (!_blep_parseKey()) {
        Serial.println("[PROVISION] Invalid PROVISION_KEY — SEEKER skipped");
        return;
    }

    Serial.println("[PROVISION] No credentials — SEEKER mode (BLE scan)");
    BLEDevice::init("");

    unsigned long startMs = millis();
    char rxSSID[33] = {}, rxPass[65] = {};

    while (millis() - startMs < PROVISION_TIMEOUT_MS) {
        Serial.println("[PROVISION] SEEKER BLE scan...");

        BLEScan* scan = BLEDevice::getScan();
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(99);
        BLEScanResults* results = scan->start(BLE_SCAN_SECONDS, false);

        BLEAddress* addr = nullptr;
        for (int i = 0; i < results->getCount(); i++) {
            BLEAdvertisedDevice dev = results->getDevice(i);
            if (dev.haveServiceUUID() &&
                dev.isAdvertisingService(BLEUUID(BLE_PROV_SERVICE_UUID))) {
                addr = new BLEAddress(dev.getAddress());
                Serial.printf("[PROVISION] SENDER found: %s\n",
                              dev.getAddress().toString().c_str());
                break;
            }
        }
        scan->clearResults();

        if (!addr) continue;

        // Connect and read the characteristic
        BLEClient* client = BLEDevice::createClient();
        client->setMTU(512);

        bool ok = false;
        if (client->connect(*addr)) {
            BLERemoteService* rsvc = client->getService(BLEUUID(BLE_PROV_SERVICE_UUID));
            if (rsvc) {
                BLERemoteCharacteristic* rchr =
                    rsvc->getCharacteristic(BLEUUID(BLE_PROV_CHAR_UUID));
                if (rchr && rchr->canRead()) {
                    String val = rchr->readValue();
                    Serial.printf("[PROVISION] Payload received %d bytes — verifying\n",
                                  (int)val.length());
                    if (_blep_decodePayload((const uint8_t*)val.c_str(), val.length(),
                                            rxSSID, rxPass)) {
                        ok = true;
                    }
                }
            }
            client->disconnect();
        } else {
            Serial.println("[PROVISION] BLE connect failed");
        }
        delete addr;

        if (ok) {
            Serial.printf("[PROVISION] Credentials OK — SSID: %s\n", rxSSID);
            JsonDocument doc;
            doc["ssid"] = rxSSID;
            doc["pass"] = rxPass;
            BLEDevice::deinit(false);
            if (storage_writeJson("/wifi_provision.json", doc)) {
                Serial.println("[PROVISION] Saved — rebooting");
                delay(100);
                ESP.restart();
            } else {
                Serial.println("[PROVISION] Save failed — continuing without credentials");
                return;
            }
        }
    }

    BLEDevice::deinit(false);
    Serial.println("[PROVISION] SEEKER timeout — falling back");
}

// Call after wifi_connectMulti() connects in STA mode.
// Starts a background FreeRTOS task that advertises encrypted credentials
// over BLE for PROVISION_SENDER_MS, then deinits BLE automatically.
static void ble_provision_setup() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!_blep_parseKey()) {
        Serial.println("[PROVISION] Invalid PROVISION_KEY — SENDER skipped");
        return;
    }

    // Find the password for the connected SSID
    String connSSID = WiFi.SSID();
    bool found = false;

    for (int i = 0; i < WIFI_NETWORK_COUNT && !found; i++) {
        if (connSSID == WIFI_NETWORKS[i].ssid) {
            strncpy(_blep_txSSID, WIFI_NETWORKS[i].ssid,     sizeof(_blep_txSSID) - 1);
            strncpy(_blep_txPass, WIFI_NETWORKS[i].password, sizeof(_blep_txPass)  - 1);
            found = true;
        }
    }
    // Fallback: device was itself provisioned via BLE
    if (!found) {
        JsonDocument doc;
        if (storage_readJson("/wifi_provision.json", doc)) {
            const char* s = doc["ssid"] | "";
            const char* p = doc["pass"] | "";
            if (strlen(s) > 0 && strlen(p) > 0 && connSSID == s) {
                strncpy(_blep_txSSID, s, sizeof(_blep_txSSID) - 1);
                strncpy(_blep_txPass, p, sizeof(_blep_txPass)  - 1);
                found = true;
            }
        }
    }
    if (!found) {
        Serial.println("[PROVISION] No credential match for SENDER — BLE disabled");
        return;
    }

    xTaskCreateStatic(_blep_senderTask, "ble_prov",
                      8192 / sizeof(StackType_t), nullptr, 5,
                      _blep_taskStack, &_blep_taskBuf);
    Serial.printf("[PROVISION] BLE SENDER task started (SSID: %s)\n", _blep_txSSID);
}

static void ble_provision_loop() {}

REGISTER_PLUGIN(ble_provision);
