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
    unsigned long tickEnd = millis() + 500;
    while (millis() < tickEnd) { sled_wifiConnect(millis()); delay(20); }
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

static void wifi_saveCreds(const char* ssid, const char* pass) {
  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["pass"] = pass;
  storage_writeJson("/wifi_saved.json", doc);
}

// ── AP fallback ───────────────────────────────────────────────────
// Called when wifi_connectMulti() fails twice. Starts an open hotspot
// so the user can still reach the web UI without a known network.
static void wifi_startAPMode() {
  // Load AP password from /config.json (overrides config.h default if set)
  String apPw = AP_PASSWORD;
  { JsonDocument cfg;
    if (storage_readJson("/config.json", cfg) && cfg["apPassword"].is<const char*>())
      apPw = String(cfg["apPassword"].as<const char*>()); }

  // Read base MAC directly from eFuse — hardware-permanent, never changes,
  // works regardless of WiFi mode or connection state.
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "MiniHead-%02X%02X", deviceMAC[4], deviceMAC[5]);

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

  sled_apMode();  // triple red flash → solid red (stays; no ArtNet in AP mode)
}

static void wifi_connectMulti() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

#ifdef PLUGIN_BLE_PROVISION
  // First-boot provisioning: save credentials permanently, then discard the transport file.
  { JsonDocument provDoc;
    if (storage_readJson("/wifi_provision.json", provDoc)) {
      const char* s = provDoc["ssid"] | "";
      const char* p = provDoc["pass"] | "";
      if (strlen(s) > 0 && strlen(p) > 0) {
        wifi_saveCreds(s, p);          // persist to /wifi_saved.json for all future boots
        LittleFS.remove("/wifi_provision.json");
      }
    }
  }
#endif

  // Load saved (BLE-provisioned) credential — treated identically to WIFI_NETWORKS[] entries.
  String savedSSID = "", savedPass = "";
  { JsonDocument savedDoc;
    if (storage_readJson("/wifi_saved.json", savedDoc)) {
      savedSSID = String(savedDoc["ssid"] | "");
      savedPass = String(savedDoc["pass"] | "");
    }
  }

  // Load networks added via the web UI (Saved Networks panel).
  loadWifiNetworksFromFlash();

  // Combine all three sources into one list — WIFI_NETWORKS[], the
  // BLE-provisioned credential, and user-added networks — so the
  // scan/retry logic below only has to iterate a single array.
  WifiCredential runtimeNets[WIFI_NETWORK_COUNT + 1 + MAX_SAVED_NETWORKS];
  int runtimeCount = 0;
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) runtimeNets[runtimeCount++] = WIFI_NETWORKS[i];
  if (savedSSID.length() > 0) runtimeNets[runtimeCount++] = { savedSSID.c_str(), savedPass.c_str() };
  for (int i = 0; i < savedNetworkCount; i++) runtimeNets[runtimeCount++] = { savedNetworks[i].ssid, savedNetworks[i].pass };

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
      for (int i = 0; i < runtimeCount; i++) {
        if (lastSSID == runtimeNets[i].ssid && wifi_ssidVisible(runtimeNets[i].ssid, found)) {
          if (wifi_tryConnect(runtimeNets[i].ssid, runtimeNets[i].password)) {
            wifi_saveLastSSID(runtimeNets[i].ssid);
            return;
          }
        }
      }
    }

    // 2. Try remaining visible networks in list order
    for (int i = 0; i < runtimeCount; i++) {
      if (lastSSID == runtimeNets[i].ssid) continue;  // already tried above
      if (!wifi_ssidVisible(runtimeNets[i].ssid, found)) continue;
      if (wifi_tryConnect(runtimeNets[i].ssid, runtimeNets[i].password)) {
        wifi_saveLastSSID(runtimeNets[i].ssid);
        return;
      }
    }

    // 3. No known SSID visible — try all without scan filter
    bool anyVisible = false;
    for (int i = 0; i < runtimeCount; i++)
      if (wifi_ssidVisible(runtimeNets[i].ssid, found)) { anyVisible = true; break; }

    if (!anyVisible) {
      Serial.println("[WiFi] No known SSIDs visible — trying all without scan filter...");
      for (int i = 0; i < runtimeCount; i++) {
        if (wifi_tryConnect(runtimeNets[i].ssid, runtimeNets[i].password)) {
          wifi_saveLastSSID(runtimeNets[i].ssid);
          return;
        }
      }
    }

    failCycles++;

    // AP fallback: fast (2 cycles) when we have zero known networks at all
    // (fresh device, nothing to lose); patient (AP_FALLBACK_MAX_CYCLES) when
    // we DO have credentials but none of them ever connect — long enough to
    // ride out a brief outage without a live fleet device dropping out, but
    // not forever, so a stale/wrong config is still fixable via the Saved
    // Networks panel instead of a USB reflash.
    int apFallbackAt = (runtimeCount == 0) ? 2 : AP_FALLBACK_MAX_CYCLES;
    if (failCycles >= apFallbackAt) {
      wifi_startAPMode();
      return;   // proceed with setup() in AP mode
    }

    Serial.printf("[WiFi] All networks failed (%d) — retrying...\n", failCycles);

#ifdef PLUGIN_BLE_PROVISION
    ble_provision_pre_wifi();
    if (runtimeCount > 0) {
      unsigned long waitEnd = millis() + 10000;
      while (millis() < waitEnd) { sled_wifiRetry(millis()); delay(50); }
    }
#else
    { unsigned long waitEnd = millis() + 10000;
      while (millis() < waitEnd) { sled_wifiRetry(millis()); delay(50); } }
#endif
  }
}

// ── Arduino entry points ──────────────────────────────────────────

uint8_t deviceMAC[6];  // hardware STA MAC — read once before any WiFi mode changes

void setup() {
  Serial.begin(115200);
  core_setup();             // mounts LittleFS — must run before wifi_connectMulti
  deviceMode_load();        // which network mode (UDP/Art-Net) is active this boot
  sled_bootInit();          // orange solid — hardware alive

  // Read MAC before any WiFi mode changes — result is stable regardless of
  // whether the device ends up in STA or AP mode.
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(deviceMAC);
  WiFi.mode(WIFI_OFF);

#ifdef PLUGIN_BLE_PROVISION
  ble_provision_pre_wifi();  // SEEKER: BLE scan loop; reboots if provisioned, returns on timeout
#endif

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

  sled_bootDone();  // LED off — ArtNet/UDP takes over from here
}

void loop() {
  // Process DNS in AP/captive-portal mode
  if (wifiAPMode) _dnsServer.processNextRequest();

  // WiFi watchdog — detect prolonged disconnection and trigger a full reconnect.
  // Checks every WIFI_WATCHDOG_INTERVAL_MS; reconnects after WIFI_WATCHDOG_MISSES
  // consecutive misses (default: 3 × 15 s = 45 s total outage before reconnect).
  if (!wifiAPMode) {
    static unsigned long _wifiCheckAt = 0;
    static uint8_t       _wifiMisses  = 0;
    if (millis() > _wifiCheckAt) {
      _wifiCheckAt = millis() + WIFI_WATCHDOG_INTERVAL_MS;
      if (WiFi.status() != WL_CONNECTED) {
        _wifiMisses++;
        Serial.printf("[WiFi] Watchdog: disconnected (miss %u/%u)\n",
                      _wifiMisses, WIFI_WATCHDOG_MISSES);
        if (_wifiMisses >= WIFI_WATCHDOG_MISSES) {
          Serial.println("[WiFi] Watchdog: triggering full reconnect");
          _wifiMisses = 0;
          wifi_connectMulti();
        }
      } else {
        _wifiMisses = 0;
      }
    }
  }

  core_loop();
  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].loop();
}
