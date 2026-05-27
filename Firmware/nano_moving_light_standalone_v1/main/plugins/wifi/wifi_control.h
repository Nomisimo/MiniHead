#pragma once

// ── WiFi Control Plugin (Standalone v1) ───────────────────────────
// HTTP server always active — no leader/follower, no UDP, no Art-Net.
// Handles: all API endpoints, cue storage, sequencer.
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "html_page.h"
#include "theme.h"
#include "core_globals.h"
#include "log_config.h"
#include "log_panel_html.h"

#define MAX_CUES    32
#define MAX_TARGETS 16

AsyncWebServer server(80);
static bool _serverStarted = false;

// ── Own identity (populated in setup) ────────────────────────────
static char ownMAC[18]  = "";
static char ownIP[16]   = "";
static int  ownFixID    = 0;
static char ownName[32] = "";

static void standalone_loadSettings() {
  JsonDocument doc;
  if (storage_readJson("/discovery.json", doc)) {
    ownFixID = doc["fixID"] | 0;
    strlcpy(ownName, doc["name"] | "", sizeof(ownName));
  }
}

static void standalone_saveSettings() {
  JsonDocument doc;
  doc["fixID"] = ownFixID;
  doc["name"]  = ownName;
  storage_writeJson("/discovery.json", doc);
}

// ── Cues ──────────────────────────────────────────────────────────

struct Cue {
  unsigned long id;
  char name[32];
  uint8_t r, g, b, w;
  int pan, tilt;
  int fixTargets[MAX_TARGETS];
  int targetCount;
};
Cue cues[MAX_CUES];
int cueCount = 0;

bool seqRunning           = false;
unsigned long seqInterval = 1000;
bool seqLoop              = true;
unsigned long seqIds[MAX_CUES];
int seqIdCount            = 0;
int seqIndex              = 0;
unsigned long lastSeqStep = 0;

void saveCuesToFlash() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < cueCount; i++) {
    const Cue& c = cues[i];
    JsonObject o = arr.add<JsonObject>();
    o["id"] = c.id; o["name"] = c.name;
    o["r"] = c.r; o["g"] = c.g; o["b"] = c.b; o["w"] = c.w;
    o["pan"] = c.pan; o["tilt"] = c.tilt;
    JsonArray ft = o["fixTargets"].to<JsonArray>();
    for (int t = 0; t < c.targetCount; t++) ft.add(c.fixTargets[t]);
  }
  storage_writeJson("/cues.json", doc);
}

void loadCuesFromFlash() {
  cueCount = 0;
  JsonDocument doc;
  if (!storage_readJson("/cues.json", doc)) {
    Serial.println("[WiFi] No cue data — starting fresh");
    return;
  }
  for (JsonObject o : doc.as<JsonArray>()) {
    if (cueCount >= MAX_CUES) break;
    Cue& c = cues[cueCount];
    c.id = o["id"] | (unsigned long)millis();
    strlcpy(c.name, o["name"] | "Cue", sizeof(c.name));
    c.r    = constrain((int)(o["r"]    | 0),  0, 255);
    c.g    = constrain((int)(o["g"]    | 0),  0, 255);
    c.b    = constrain((int)(o["b"]    | 0),  0, 255);
    c.w    = constrain((int)(o["w"]    | 0),  0, 255);
    c.pan  = constrain((int)(o["pan"]  | 90), 0, 180);
    c.tilt = constrain((int)(o["tilt"] | 90), 0, 180);
    c.targetCount = 0;
    for (JsonVariant v : o["fixTargets"].as<JsonArray>()) {
      if (c.targetCount >= MAX_TARGETS) break;
      c.fixTargets[c.targetCount++] = v.as<int>();
    }
    if (c.targetCount == 0) { c.fixTargets[0] = 0; c.targetCount = 1; }
    cueCount++;
  }
  Serial.printf("[WiFi] Loaded %d cue(s)\n", cueCount);
}

// ── Async body accumulation ───────────────────────────────────────

static void _bodyAccumulator(AsyncWebServerRequest* req,
                              uint8_t* data, size_t len,
                              size_t index, size_t total) {
  if (index == 0) {
    req->_tempObject = new String();
    ((String*)req->_tempObject)->reserve(total > 0 ? total : 64);
  }
  if (req->_tempObject)
    ((String*)req->_tempObject)->concat((char*)data, len);
}

static String _getBody(AsyncWebServerRequest* req) {
  if (!req->_tempObject) return "";
  String body = *((String*)req->_tempObject);
  delete (String*)req->_tempObject;
  req->_tempObject = nullptr;
  return body;
}

// ── Response helpers ──────────────────────────────────────────────

void sendJson(AsyncWebServerRequest* req, int code, const String& json) {
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", json);
  r->addHeader("Access-Control-Allow-Origin", "*");
  req->send(r);
}

static void sendHtmlProgmem(AsyncWebServerRequest* req, const char* progmemStr) {
  AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html", progmemStr);
  r->addHeader("Access-Control-Allow-Origin", "*");
  req->send(r);
}

// ── Cue helpers ───────────────────────────────────────────────────

String fixTargetsToJson(const Cue& c) {
  String s = "[";
  for (int i = 0; i < c.targetCount; i++) { if (i > 0) s += ","; s += String(c.fixTargets[i]); }
  s += "]"; return s;
}

String cueToJson(const Cue& c) {
  return "{\"id\":" + String(c.id) +
    ",\"name\":\"" + String(c.name) + "\"" +
    ",\"r\":"   + c.r   + ",\"g\":" + c.g  +
    ",\"b\":"   + c.b   + ",\"w\":" + c.w  +
    ",\"pan\":" + c.pan + ",\"tilt\":" + c.tilt +
    ",\"fixTargets\":" + fixTargetsToJson(c) + "}";
}

void fireCueToTargets(const Cue& c) {
  String cmd = "R:" + String(c.r) + ",G:" + String(c.g) +
               ",B:" + String(c.b) + ",W:" + String(c.w) +
               ",PAN:" + String(c.pan) + ",TILT:" + String(c.tilt);
  rainbowActive = false;
  applyCommand(cmd);
}

// ── Route handlers ────────────────────────────────────────────────

void handleRoot(AsyncWebServerRequest* req)     { sendHtmlProgmem(req, INDEX_HTML); }
void handleLogPanel(AsyncWebServerRequest* req) { sendHtmlProgmem(req, LOG_PANEL_HTML); }
void handleTheme(AsyncWebServerRequest* req) {
  AsyncWebServerResponse* r = req->beginResponse_P(200, "text/css", THEME_CSS);
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Cache-Control", "max-age=3600");
  req->send(r);
}

void handleStatus(AsyncWebServerRequest* req)     { sendJson(req, 200, "{\"connected\":true,\"port\":\"WiFi\",\"ip\":\""+WiFi.localIP().toString()+"\"}"); }
void handleVersion(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"version\":\"standalone-1\"}"); }
void handlePorts(AsyncWebServerRequest* req)      { sendJson(req, 200, "[{\"port\":\"WiFi\",\"description\":\"ESP32 @ "+WiFi.localIP().toString()+"\"}]"); }
void handleConnect(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"status\":\"ok\",\"port\":\"WiFi\"}"); }
void handleDisconnect(AsyncWebServerRequest* req) { sendJson(req, 200, "{\"status\":\"ok\"}"); }
void handleSeqStop(AsyncWebServerRequest* req)    { seqRunning = false; sendJson(req, 200, "{\"status\":\"ok\"}"); }
void handleSeqStatus(AsyncWebServerRequest* req)  { sendJson(req, 200, String("{\"running\":"+(seqRunning?String("true"):String("false"))+"}")); }

void handleGetFixtures(AsyncWebServerRequest* req) {
  String json = "[";
  if (ownFixID > 0)
    json += "{\"id\":" + String(ownFixID) + ",\"name\":\"" + String(ownName) +
            "\",\"mac\":\"" + String(ownMAC) + "\",\"online\":true,\"ip\":\"" + String(ownIP) + "\"}";
  json += "]";
  sendJson(req, 200, json);
}

void handleSend(AsyncWebServerRequest* req) {
  String body = _getBody(req);
  if (body.isEmpty()) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String cmd = doc["command"] | "";
  cmd.trim();
  rainbowActive = false;
  applyCommand(cmd);
  sendJson(req, 200, "{\"status\":\"ok\",\"response\":\"OK\"}");
}

void handleRainbow(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  applyCommand(on ? "RAINBOW:1" : "RAINBOW:0");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleGetCues(AsyncWebServerRequest* req) {
  String json = "[";
  for (int i = 0; i < cueCount; i++) { if (i > 0) json += ","; json += cueToJson(cues[i]); }
  json += "]";
  sendJson(req, 200, json);
}

void handleSaveCue(AsyncWebServerRequest* req) {
  if (cueCount >= MAX_CUES) { sendJson(req, 500, "{\"status\":\"error\",\"message\":\"Max cues\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
  Cue& c = cues[cueCount];
  c.id = millis();
  strlcpy(c.name, doc["name"] | "Cue", sizeof(c.name));
  c.r    = constrain((int)doc["r"],    0, 255);
  c.g    = constrain((int)doc["g"],    0, 255);
  c.b    = constrain((int)doc["b"],    0, 255);
  c.w    = constrain((int)doc["w"],    0, 255);
  c.pan  = constrain((int)doc["pan"],  0, 180);
  c.tilt = constrain((int)doc["tilt"], 0, 180);
  c.targetCount = 0;
  JsonArray tArr = doc["fixTargets"].as<JsonArray>();
  if (!tArr.isNull()) {
    for (JsonVariant v : tArr) {
      if (c.targetCount >= MAX_TARGETS) break;
      c.fixTargets[c.targetCount++] = v.as<int>();
    }
  }
  if (c.targetCount == 0) { c.fixTargets[0] = 0; c.targetCount = 1; }
  cueCount++;
  saveCuesToFlash();
  sendJson(req, 200, "{\"status\":\"ok\",\"cue\":" + cueToJson(c) + "}");
}

void handleUpdateCueTargets(AsyncWebServerRequest* req) {
  String path = req->url();
  String trimmed = path.substring(0, path.lastIndexOf('/'));
  unsigned long id = trimmed.substring(trimmed.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id == id) {
      JsonDocument doc;
      if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
      JsonArray tArr = doc["fixTargets"].as<JsonArray>();
      cues[i].targetCount = 0;
      for (JsonVariant v : tArr) {
        if (cues[i].targetCount >= MAX_TARGETS) break;
        cues[i].fixTargets[cues[i].targetCount++] = v.as<int>();
      }
      saveCuesToFlash();
      sendJson(req, 200, "{\"status\":\"ok\",\"cue\":" + cueToJson(cues[i]) + "}"); return;
    }
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleDeleteCue(AsyncWebServerRequest* req) {
  String path = req->url();
  unsigned long id = path.substring(path.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id == id) {
      for (int j = i; j < cueCount - 1; j++) cues[j] = cues[j + 1];
      cueCount--;
      saveCuesToFlash();
      sendJson(req, 200, "{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleFireCue(AsyncWebServerRequest* req) {
  String path = req->url();
  String trimmed = path.substring(0, path.lastIndexOf('/'));
  unsigned long id = trimmed.substring(trimmed.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id == id) {
      fireCueToTargets(cues[i]);
      sendJson(req, 200, "{\"status\":\"ok\",\"command\":\"fired\",\"response\":\"OK\"}"); return;
    }
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleReorderCues(AsyncWebServerRequest* req) {
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
  JsonArray order = doc["order"].as<JsonArray>();
  if (order.isNull()) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Missing order\"}"); return; }
  Cue temp[MAX_CUES];
  int newCount = 0;
  for (JsonVariant v : order) {
    unsigned long id = v.as<unsigned long>();
    for (int i = 0; i < cueCount; i++) {
      if (cues[i].id == id) { temp[newCount++] = cues[i]; break; }
    }
  }
  if (newCount != cueCount) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Count mismatch\"}"); return; }
  for (int i = 0; i < cueCount; i++) cues[i] = temp[i];
  saveCuesToFlash();
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleSeqStart(AsyncWebServerRequest* req) {
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
  seqInterval = doc["interval_ms"] | 1000;
  seqLoop     = doc["loop"]        | true;
  JsonArray ids = doc["cue_ids"].as<JsonArray>();
  seqIdCount = 0;
  for (JsonVariant v : ids) if (seqIdCount < MAX_CUES) seqIds[seqIdCount++] = v.as<unsigned long>();
  seqIndex = 0; seqRunning = (seqIdCount > 0); lastSeqStep = millis();
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleGetLogConfig(AsyncWebServerRequest* req) { sendJson(req, 200, logcfg_toJson()); }
void handleSetLogConfig(AsyncWebServerRequest* req) { logcfg_fromJson(_getBody(req)); sendJson(req, 200, "{\"status\":\"ok\"}"); }

// ── Route setup ───────────────────────────────────────────────────

void setupRoutes() {
  server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200);
    r->addHeader("Access-Control-Allow-Origin",  "*");
    r->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    r->addHeader("Access-Control-Allow-Headers", "Content-Type");
    req->send(r);
  });

  server.on("/",                       HTTP_GET, [](AsyncWebServerRequest* r){ handleRoot(r); });
  server.on("/theme",                  HTTP_GET, [](AsyncWebServerRequest* r){ handleTheme(r); });
  server.on("/plugins/log/panel.html", HTTP_GET, [](AsyncWebServerRequest* r){ handleLogPanel(r); });

  server.on("/api/logconfig", HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetLogConfig(r); });
  server.on("/api/logconfig", HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSetLogConfig(r); },
    nullptr, _bodyAccumulator);

  server.on("/api/status",     HTTP_GET,  [](AsyncWebServerRequest* r){ handleStatus(r); });
  server.on("/api/version",    HTTP_GET,  [](AsyncWebServerRequest* r){ handleVersion(r); });
  server.on("/api/ports",      HTTP_GET,  [](AsyncWebServerRequest* r){ handlePorts(r); });
  server.on("/api/connect",    HTTP_POST, [](AsyncWebServerRequest* r){ handleConnect(r); });
  server.on("/api/disconnect", HTTP_POST, [](AsyncWebServerRequest* r){ handleDisconnect(r); });
  server.on("/api/fixtures",   HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetFixtures(r); });
  server.on("/api/send",       HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSend(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/rainbow",    HTTP_POST,
    [](AsyncWebServerRequest* r){ handleRainbow(r); },
    nullptr, _bodyAccumulator);

  server.on("/api/cues",         HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetCues(r); });
  server.on("/api/cues",         HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSaveCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/reorder", HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleReorderCues(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_POST,
    [](AsyncWebServerRequest* r){ handleFireCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleUpdateCueTargets(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_DELETE, [](AsyncWebServerRequest* r){ handleDeleteCue(r); });

  server.on("/api/sequencer/start",  HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSeqStart(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/sequencer/stop",   HTTP_POST, [](AsyncWebServerRequest* r){ handleSeqStop(r); });
  server.on("/api/sequencer/status", HTTP_GET,  [](AsyncWebServerRequest* r){ handleSeqStatus(r); });

  server.onNotFound([](AsyncWebServerRequest* r){ r->send(404, "text/plain", "Not found"); });
}

// ── Lifecycle ─────────────────────────────────────────────────────

void wifi_control_setup() {
  standalone_loadSettings();

  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(ownMAC, sizeof(ownMAC), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  strlcpy(ownIP, WiFi.localIP().toString().c_str(), sizeof(ownIP));

  logcfg_load();
  loadCuesFromFlash();

  Serial.printf("[WiFi] Standalone — IP: %s  MAC: %s\n", ownIP, ownMAC);
  setupRoutes();
  if (!_serverStarted) {
    server.begin();
    _serverStarted = true;
    Serial.println("[WiFi] HTTP server started");
  }
}

void wifi_control_loop() {
  if (seqRunning && seqIdCount > 0) {
    unsigned long now = millis();
    if (now - lastSeqStep >= seqInterval) {
      lastSeqStep = now;
      unsigned long tid = seqIds[seqIndex];
      for (int i = 0; i < cueCount; i++) {
        if (cues[i].id == tid) { fireCueToTargets(cues[i]); break; }
      }
      if (++seqIndex >= seqIdCount) { if (seqLoop) seqIndex = 0; else seqRunning = false; }
    }
  }
}

REGISTER_PLUGIN(wifi_control);
