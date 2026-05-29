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

SET_LOOP_TASK_STACK_SIZE(16384);  // default is 8192, double it

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

static void wifi_connectMulti() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

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

    Serial.println("[WiFi] All networks failed — retrying in 10 s...");
    setLED(20, 0, 0, 0);   // dim red while waiting
    delay(10000);
    setLED(0, 0, 0, 0);
  }
}

// ── Arduino entry points ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  core_setup();             // mounts LittleFS — must run before wifi_connectMulti

  wifi_connectMulti();      // blocks until connected

  // Disable WiFi modem sleep — without this the radio powers down between
  // beacon intervals and drops ~60% of incoming UDP packets (ArtNet etc.).
  WiFi.setSleep(false);
  Serial.println("[WiFi] Modem sleep disabled (low-latency UDP mode)");

  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].setup();
}

void loop() {
  core_loop();
  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].loop();
}
