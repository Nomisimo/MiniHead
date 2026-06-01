/*
 * =====================================================
 *   NANO MOVING LIGHT — Modular Edition
 *   Hardware : ESP32-C3 Super Mini
 * =====================================================
 * main.ino — never needs to be touched.
 * Add/remove plugins in config.h only.
 * =====================================================
 */

#include "config.h"
#include <ESPmDNS.h>
#include <DNSServer.h>

SET_LOOP_TASK_STACK_SIZE(16384);  // default is 8192, double it

static DNSServer _dnsServer;

// ── WiFi multi-network helpers ────────────────────────────────────

static bool wifi_ssidVisible(const char* ssid, int n) {
  for (int i = 0; i < n; i++)
    if (WiFi.SSID(i) == ssid) return true;
  return false;
}

static bool wifi_tryConnect(const char* ssid, const char* pass, unsigned long tms = 8000) {
  Serial.printf("[WiFi] Trying \"%s\" ", ssid);
  WiFi.begin(ssid, pass);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < tms) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected: %s  IP: %s\n", ssid, WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.printf("[WiFi] Failed: %s\n", ssid);
  WiFi.disconnect(true);
  delay(200);
  return false;
}

static void wifi_saveLastSSID(const char* ssid) {
  JsonDocument doc;
  doc["ssid"] = ssid;
  storage_writeJson("/wifi_last.json", doc);
}

// ── AP fallback ───────────────────────────────────────────────────
// Called when wifi_connectMulti() fails twice. Starts an open hotspot
// so the user can still reach the web UI without a known network.
static void wifi_startAPMode() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "MiniHead-%02X%02X%02X", mac[3], mac[4], mac[5]);

  // Load AP password from /config.json (overrides config.h default if set)
  String apPw = AP_PASSWORD;
  { JsonDocument cfg;
    if (storage_readJson("/config.json", cfg) && cfg["apPassword"].is<const char*>())
      apPw = String(cfg["apPassword"].as<const char*>()); }

  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_AP);
  if (apPw.length() >= 8) WiFi.softAP(ssid, apPw.c_str());
  else                     WiFi.softAP(ssid);
  wifiAPMode    = true;
  apPasswordSet = (apPw.length() >= 8);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi] AP mode — SSID: %s  IP: %s\n", ssid, apIP.toString().c_str());

  // DNS server: answer ALL queries with our IP → captive portal on any URL
  _dnsServer.start(53, "*", apIP);
  Serial.println("[WiFi] Captive portal DNS started");

  // mDNS: accessible as minihead.local in addition to 192.168.4.1
  if (MDNS.begin("minihead")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[WiFi] mDNS: minihead.local");
  }

  // Slow blue pulse × 3 so the user knows "I'm a hotspot, connect to me"
  for (int i = 0; i < 3; i++) {
    setLED(0, 0, 60, 0); delay(400);
    setLED(0, 0,  0, 0); delay(400);
  }
  setLED(0, 0, 20, 0);  // dim blue stays on
}

static void wifi_connectMulti() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  int failCycles = 0;

  while (true) {
    // Read last-connected SSID from flash
    String lastSSID = "";
    JsonDocument lastDoc;
    if (storage_readJson("/wifi_last.json", lastDoc))
      lastSSID = String(lastDoc["ssid"] | "");

    // Scan
    Serial.println("[WiFi] Scanning...");
    int found = WiFi.scanNetworks();
    Serial.printf("[WiFi] %d network(s) found\n", found);

    // 1. Try last-connected first (only if visible)
    if (lastSSID.length() > 0) {
      for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
        if (lastSSID == WIFI_NETWORKS[i].ssid && wifi_ssidVisible(WIFI_NETWORKS[i].ssid, found)) {
          if (wifi_tryConnect(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password)) {
            wifi_saveLastSSID(WIFI_NETWORKS[i].ssid);
            return;
          }
        }
      }
    }

    // 2. Try remaining visible networks in list order
    for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
      if (lastSSID == WIFI_NETWORKS[i].ssid) continue;  // already tried above
      if (!wifi_ssidVisible(WIFI_NETWORKS[i].ssid, found)) continue;
      if (wifi_tryConnect(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password)) {
        wifi_saveLastSSID(WIFI_NETWORKS[i].ssid);
        return;
      }
    }

    // 3. No known SSID visible — try all without scan filter
    bool anyVisible = false;
    for (int i = 0; i < WIFI_NETWORK_COUNT; i++)
      if (wifi_ssidVisible(WIFI_NETWORKS[i].ssid, found)) { anyVisible = true; break; }

    if (!anyVisible) {
      Serial.println("[WiFi] No known SSIDs visible — trying all without scan filter...");
      for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
        if (wifi_tryConnect(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password)) {
          wifi_saveLastSSID(WIFI_NETWORKS[i].ssid);
          return;
        }
      }
    }

    failCycles++;
    if (failCycles >= 2) {
      wifi_startAPMode();
      return;   // proceed with setup() in AP mode
    }

    Serial.printf("[WiFi] All networks failed (%d/2) — retrying in 10 s...\n", failCycles);
    setLED(20, 0, 0, 0);   // dim red while waiting
    delay(10000);
    setLED(0, 0, 0, 0);
  }
}

// ── Arduino entry points ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  core_setup();             // mounts LittleFS — must run before wifi_connectMulti

  wifi_connectMulti();      // blocks until connected (or starts AP mode)

  // Disable WiFi modem sleep — without this the radio powers down between
  // beacon intervals and drops ~60% of incoming UDP packets (ArtNet etc.).
  WiFi.setSleep(false);
  Serial.println("[WiFi] Modem sleep disabled (low-latency UDP mode)");

  // mDNS for STA mode — accessible as minihead.local on the local network
  if (!wifiAPMode) {
    if (MDNS.begin("minihead")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[WiFi] mDNS: minihead.local");
    }
  }

  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].setup();
}

void loop() {
  // Process DNS in AP/captive-portal mode
  if (wifiAPMode) _dnsServer.processNextRequest();

  core_loop();
  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].loop();
}
