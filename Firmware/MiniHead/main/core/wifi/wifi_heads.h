#pragma once

// ── Heads, fixtures, and identify handlers ────────────────────────
// handleGetHeads, handleGetFixtures, handleIdentify,
// handleSetName, handleSetFixID — config forwarded to followers via a
// background FreeRTOS task so the async handler returns immediately.
// ─────────────────────────────────────────────────────────────────

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ── Async HTTP config forward ─────────────────────────────────────
// handleSetName / handleSetFixID need to POST to a follower's HTTP
// server. Doing this synchronously inside an AsyncWebServer handler
// blocks the TCP task and can cause WDT resets under load.
// Instead, spawn a tiny one-shot FreeRTOS task for the round-trip.

struct _HttpFwdTask {
  char url[128];
  char body[128];
};

static void _httpFwdRun(void* arg) {
  _HttpFwdTask* t = (_HttpFwdTask*)arg;
  HTTPClient http;
  http.begin(t->url);
  http.addHeader("Content-Type", "application/json");
  http.POST(t->body);
  http.end();
  delete t;
  vTaskDelete(nullptr);
}

static void _httpFwdPost(const char* url, const char* body) {
  _HttpFwdTask* t = new _HttpFwdTask();
  strlcpy(t->url,  url,  sizeof(t->url));
  strlcpy(t->body, body, sizeof(t->body));
  if (xTaskCreate(_httpFwdRun, "http_fwd", 4096, t, 5, nullptr) != pdPASS) {
    Serial.println("[WiFi] http_fwd task create failed — falling back to sync");
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.POST(body);
    http.end();
    delete t;
  }
}

// ── Handlers ──────────────────────────────────────────────────────

void handleGetHeads(AsyncWebServerRequest* req) {
  String json = "[";
  json += "{\"mac\":\"" + String(ownMAC) + "\",\"ip\":\"" + String(ownIP) + "\""
        + ",\"fixID\":" + String(ownFixID) + ",\"name\":\"" + _jsonEscapeStr(ownName) + "\""
        + ",\"mode\":\"" + String(ownMode) + "\""
        + ",\"role\":\"LEADER\",\"self\":true}";
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active) continue;
    json += ",{\"mac\":\"" + String(peers[i].mac) + "\",\"ip\":\"" + String(peers[i].ip) + "\""
          + ",\"fixID\":" + String(peers[i].fixID) + ",\"name\":\"" + _jsonEscapeStr(peers[i].name) + "\""
          + ",\"mode\":\"" + String(peers[i].mode) + "\""
          + ",\"role\":\"" + (peers[i].role == ROLE_LEADER ? "LEADER" : "FOLLOWER") + "\",\"self\":false}";
  }
  sendJson(req, 200, json + "]");
}

void handleGetFixtures(AsyncWebServerRequest* req) {
  String json = "[";
  bool first = true;
  if (ownFixID > 0) {
    json += "{\"id\":" + String(ownFixID) + ",\"name\":\"" + _jsonEscapeStr(ownName) + "\",\"mac\":\"" + String(ownMAC) + "\",\"online\":true,\"ip\":\"" + String(ownIP) + "\"}";
    first = false;
  }
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active || peers[i].fixID <= 0) continue;
    if (!first) json += ",";
    json += "{\"id\":" + String(peers[i].fixID) + ",\"name\":\"" + _jsonEscapeStr(peers[i].name) + "\",\"mac\":\"" + String(peers[i].mac) + "\",\"online\":true,\"ip\":\"" + String(peers[i].ip) + "\"}";
    first = false;
  }
  sendJson(req, 200, json + "]");
}

void handleSetName(AsyncWebServerRequest* req) {
  String path = req->url();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  const char* name = doc["name"] | "";
  if (mac == String(ownMAC)) {
    discovery_saveName(name);
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active || String(peers[i].mac) != mac) continue;
    String url  = "http://" + String(peers[i].ip) + "/api/config/name";
    String body = "{\"name\":\"" + _jsonEscapeStr(name) + "\"}";
    _httpFwdPost(url.c_str(), body.c_str());
    strlcpy(peers[i].name, name, sizeof(peers[i].name));
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleSetFixID(AsyncWebServerRequest* req) {
  String path = req->url();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
  int newID = doc["fixID"] | 0;
  if (mac == String(ownMAC)) {
    discovery_saveFixID(newID);
    sendJson(req, 200, "{\"status\":\"ok\",\"fixID\":" + String(newID) + "}"); return;
  }
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active || String(peers[i].mac) != mac) continue;
    String url  = "http://" + String(peers[i].ip) + "/api/config/fixid";
    String body = "{\"fixID\":" + String(newID) + "}";
    _httpFwdPost(url.c_str(), body.c_str());
    peers[i].fixID = newID;
    sendJson(req, 200, "{\"status\":\"ok\",\"fixID\":" + String(newID) + "}"); return;
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleIdentify(AsyncWebServerRequest* req) {
  String path = req->url();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  if (mac == String(ownMAC) || mac == "SELF") {
    if (on) setLED(255, 255, 255, 255); else setLED(curR, curG, curB, curW);
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active || String(peers[i].mac) != mac) continue;
    if (on) udp_sendIdentifyOn(peers[i].ip, peers[i].mac);
    else    udp_sendIdentifyOff(peers[i].ip, peers[i].mac);
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Peer not found\"}");
}
