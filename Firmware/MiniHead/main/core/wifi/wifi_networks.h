#pragma once

// ── Saved WiFi Networks ────────────────────────────────────────────
// Lets users add/remove WiFi networks (SSID + password) through the
// device's own web UI, persisted to LittleFS — no reflash required.
// Consulted by wifi_connectMulti() in main.ino in addition to the
// compile-time WIFI_NETWORKS[] list and the BLE-provisioned
// /wifi_saved.json credential.
//
// Schema (/wifi_networks.json): [{"ssid":"...","pass":"..."}, ...]
// Passwords are write-only — GET /api/wifi/networks never returns them.
// ─────────────────────────────────────────────────────────────────

#ifndef MAX_SAVED_NETWORKS
#define MAX_SAVED_NETWORKS 10
#endif

struct SavedNetwork {
  char ssid[33];
  char pass[65];
};
SavedNetwork savedNetworks[MAX_SAVED_NETWORKS];
int savedNetworkCount = 0;

// Set by handleReboot(); checked in wifi_control_loop() so the HTTP
// response has time to flush before the device restarts.
unsigned long _wifiRebootAt = 0;

// ── LittleFS persistence ──────────────────────────────────────────

void loadWifiNetworksFromFlash() {
  savedNetworkCount = 0;
  JsonDocument doc;
  if (!storage_readJson("/wifi_networks.json", doc)) {
    Serial.println("[WiFi] No saved networks — starting fresh");
    return;
  }
  for (JsonObject o : doc.as<JsonArray>()) {
    if (savedNetworkCount >= MAX_SAVED_NETWORKS) break;
    SavedNetwork& n = savedNetworks[savedNetworkCount];
    strlcpy(n.ssid, o["ssid"] | "", sizeof(n.ssid));
    strlcpy(n.pass, o["pass"] | "", sizeof(n.pass));
    if (strlen(n.ssid) == 0) continue;
    savedNetworkCount++;
  }
  Serial.printf("[WiFi] Loaded %d saved network(s) from /wifi_networks.json\n", savedNetworkCount);
}

void saveWifiNetworksToFlash() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < savedNetworkCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = savedNetworks[i].ssid;
    o["pass"] = savedNetworks[i].pass;
  }
  storage_writeJson("/wifi_networks.json", doc);
}

// ── HTTP handlers ─────────────────────────────────────────────────

void handleGetWifiNetworks(AsyncWebServerRequest* req) {
  String json = "[";
  for (int i = 0; i < savedNetworkCount; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + _jsonEscapeStr(savedNetworks[i].ssid) + "\"}";
  }
  json += "]";
  sendJson(req, 200, json);
}

void handleAddWifiNetwork(AsyncWebServerRequest* req) {
  String body = _getBody(req);  // must be called before any early return — frees _tempObject
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String ssid = doc["ssid"] | "";
  ssid.trim();
  String pass = doc["pass"] | "";
  if (ssid.length() == 0 || ssid.length() > 32) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"SSID must be 1-32 characters\"}"); return;
  }
  if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 64)) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Password must be empty (open) or 8-64 characters\"}"); return;
  }

  // Update in place if this SSID is already saved, otherwise append.
  for (int i = 0; i < savedNetworkCount; i++) {
    if (ssid == savedNetworks[i].ssid) {
      strlcpy(savedNetworks[i].pass, pass.c_str(), sizeof(savedNetworks[i].pass));
      saveWifiNetworksToFlash();
      Serial.printf("[WiFi] Saved network updated: %s\n", ssid.c_str());
      sendJson(req, 200, "{\"status\":\"ok\"}"); return;
    }
  }
  if (savedNetworkCount >= MAX_SAVED_NETWORKS) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Max saved networks reached\"}"); return;
  }
  SavedNetwork& n = savedNetworks[savedNetworkCount++];
  strlcpy(n.ssid, ssid.c_str(), sizeof(n.ssid));
  strlcpy(n.pass, pass.c_str(), sizeof(n.pass));
  saveWifiNetworksToFlash();
  Serial.printf("[WiFi] Saved network added: %s\n", ssid.c_str());
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleDeleteWifiNetwork(AsyncWebServerRequest* req) {
  String body = _getBody(req);  // must be called before any early return — frees _tempObject
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String ssid = doc["ssid"] | "";
  for (int i = 0; i < savedNetworkCount; i++) {
    if (ssid != savedNetworks[i].ssid) continue;
    for (int j = i; j < savedNetworkCount - 1; j++) savedNetworks[j] = savedNetworks[j + 1];
    savedNetworkCount--;
    saveWifiNetworksToFlash();
    Serial.printf("[WiFi] Saved network removed: %s\n", ssid.c_str());
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleReboot(AsyncWebServerRequest* req) {
  Serial.println("[WiFi] Reboot requested via web UI");
  sendJson(req, 200, "{\"status\":\"ok\"}");
  _wifiRebootAt = millis() + 300;  // let the async response flush before restarting
}
