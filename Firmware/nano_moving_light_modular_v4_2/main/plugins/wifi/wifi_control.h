#pragma once

// ── WiFi Control Plugin ───────────────────────────────────────────
// Handles: HTTP server (ESPAsyncWebServer), all API endpoints,
//          cue storage, sequencer.
// Only active when nodeRole == ROLE_LEADER (started by discovery.h).
// Depends on: core.h, discovery_globals.h
// UDP functions are weak stubs — overridden by udp_control plugin.
// Art-Net routes are registered by artnet plugin.
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "html_page.h"
#include "theme.h"
#include "core_globals.h"
#include "log_config.h"
#include "log_panel_html.h"
#include "discovery_globals.h"

// ── UDP forward declarations ──────────────────────────────────────
// Implemented by plugins/udp_control/udp_commands.h.
void udp_sendCommand(const char* ip, const char* mac, const char* cmd);
void udp_broadcastCommand(const char* cmd);
void udp_sendIdentifyOn(const char* ip, const char* mac);
void udp_sendIdentifyOff(const char* ip, const char* mac);

// ── ArtNet forward declaration ────────────────────────────────────
// Implemented by plugins/artnet/artnet_control.h (included after this file).
#ifdef PLUGIN_ARTNET
void artnet_upsertPatch(uint16_t universe, uint16_t startAddr);
#endif

#define MAX_CUES    32
#define MAX_TARGETS 16   // max FixID targets per cue

AsyncWebServer server(80);
static bool _serverStarted = false;  // server.begin() must only be called once
static bool _serverActive  = false;  // true only while this node is LEADER
bool wifiAPMode    = false;   // true when running as standalone hotspot
bool apPasswordSet = false;   // true when AP has a password ≥8 chars

// ── Leader guard ──────────────────────────────────────────────────
// Call at the top of HTML handlers that should be unreachable for followers.
// Smart redirect:
//   - request comes FROM the leader machine  → http://127.0.0.1:8080  (PC App)
//   - request comes from any other device    → http://<leaderIP>:8080
// Returns false when redirected; caller must return immediately.
static bool requireLeader(AsyncWebServerRequest* req) {
  if (_serverActive) return true;

  String leaderIP = "";
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && peers[i].role == ROLE_LEADER) {
      leaderIP = String(peers[i].ip); break;
    }
  }

  if (leaderIP.length() > 0) {
    String clientIP = req->client()->remoteIP().toString();
    if (clientIP == leaderIP) {
      // Browser is running on the leader PC itself → go to local PC App
      req->redirect("http://127.0.0.1:8080");
    } else {
      // Remote device (phone/tablet) → redirect to leader's LAN address
      req->redirect("http://" + leaderIP + ":8080");
    }
    return false;
  }

  req->send(503, "application/json", "{\"status\":\"follower\",\"message\":\"Not the leader\"}");
  return false;
}

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

bool seqRunning  = false;
unsigned long seqInterval = 1000;
bool seqLoop     = true;
unsigned long seqIds[MAX_CUES];
int seqIdCount   = 0;
int seqIndex     = 0;
unsigned long lastSeqStep = 0;

// ── Storage (/cues.json) ──────────────────────────────────────────
// Schema: [{"id":1234,"name":"...","r":255,"g":0,"b":0,"w":0,
//            "pan":90,"tilt":90,"fixTargets":[1,2]}]

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
    c.pan  = constrain((int)(o["pan"]  | 90), 0, 270);
    c.tilt = constrain((int)(o["tilt"] | 90), 0, 270);
    c.targetCount = 0;
    for (JsonVariant v : o["fixTargets"].as<JsonArray>()) {
      if (c.targetCount >= MAX_TARGETS) break;
      c.fixTargets[c.targetCount++] = v.as<int>();
    }
    if (c.targetCount == 0) { c.fixTargets[0] = 0; c.targetCount = 1; }
    cueCount++;
  }
  Serial.printf("[WiFi] Loaded %d cue(s) from /cues.json\n", cueCount);
}

// ── Async body accumulation ───────────────────────────────────────
// ESPAsyncWebServer delivers POST/PUT bodies via a separate callback.
// We accumulate chunks into request->_tempObject (a String*), then
// read it once in the request handler via _getBody().

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
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void sendHtmlProgmem(AsyncWebServerRequest* req,
                            const char* progmemStr,
                            bool /*noCache*/ = false) {
  // Always no-store: ESPAsyncWebServer doesn't send ETag/Last-Modified so
  // browsers mishandle conditional GETs on reload — panels stop loading.
  AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html", progmemStr);
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Cache-Control", "no-store");
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

// ── Fire a cue to all its targets ────────────────────────────────

void fireCueToTargets(const Cue& c) {
  String cmd = "R:" + String(c.r) + ",G:" + String(c.g) +
               ",B:" + String(c.b) + ",W:" + String(c.w) +
               ",PAN:" + String(c.pan) + ",TILT:" + String(c.tilt);

  bool toAll = (c.targetCount == 0) || (c.targetCount == 1 && c.fixTargets[0] == 0);
  Serial.printf("[Seq] Fire '%s': %s  toAll=%d\n", c.name, cmd.c_str(), (int)toAll);
  if (toAll) {
    rainbowActive = false; demoActive = false;
    applyCommand(cmd);
    udp_broadcastCommand(cmd.c_str());
    return;
  }
  for (int t = 0; t < c.targetCount; t++) {
    int fid = c.fixTargets[t];
    if (ownFixID > 0 && ownFixID == fid) { rainbowActive = false; demoActive = false; applyCommand(cmd); }
    for (int i = 0; i < peerCount; i++) {
      if (peers[i].active && peers[i].fixID == fid) {
        udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str());
        break;
      }
    }
  }
}

// ── Route handlers ────────────────────────────────────────────────

void handleRoot(AsyncWebServerRequest* req)           { if (!requireLeader(req)) return; sendHtmlProgmem(req, INDEX_HTML); }

static String _wifiIP() {
  IPAddress ip = WiFi.localIP();
  return (ip != IPAddress(0,0,0,0)) ? ip.toString() : WiFi.softAPIP().toString();
}

void handleStatus(AsyncWebServerRequest* req) {
  String json = "{\"connected\":true,\"port\":\"WiFi\",\"ip\":\"" + _wifiIP() + "\""
              + ",\"apMode\":"        + (wifiAPMode    ? "true" : "false")
              + ",\"apPasswordSet\":" + (apPasswordSet ? "true" : "false")
              + ",\"rainbowActive\":" + (rainbowActive ? "true" : "false")
              + ",\"demoActive\":"     + (demoActive     ? "true" : "false")
              + ",\"animSpeed\":"      + String(animSpeed, 2)
              + "}";
  sendJson(req, 200, json);
}
void handleVersion(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"version\":\"4.2\"}"); }
void handlePorts(AsyncWebServerRequest* req)      { sendJson(req, 200, "[{\"port\":\"WiFi\",\"description\":\"ESP32 @ "+_wifiIP()+"\"}]"); }
void handleConnect(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"status\":\"ok\",\"port\":\"WiFi\"}"); }
void handleDisconnect(AsyncWebServerRequest* req) { sendJson(req, 200, "{\"status\":\"ok\"}"); }
void handleSeqStop(AsyncWebServerRequest* req)    { seqRunning = false; sendJson(req, 200, "{\"status\":\"ok\"}"); }
void handleSeqStatus(AsyncWebServerRequest* req)  { sendJson(req, 200, String("{\"running\":"+(seqRunning?String("true"):String("false"))+"}")); }

void handleGetHeads(AsyncWebServerRequest* req) {
  String json = "[";
  json += "{\"mac\":\"" + String(ownMAC) + "\",\"ip\":\"" + String(ownIP) + "\""
        + ",\"fixID\":" + String(ownFixID) + ",\"name\":\"" + String(ownName) + "\""
        + ",\"mode\":\"" + String(ownMode) + "\""
        + ",\"role\":\"LEADER\",\"self\":true}";
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active) continue;
    json += ",{\"mac\":\"" + String(peers[i].mac) + "\",\"ip\":\"" + String(peers[i].ip) + "\""
          + ",\"fixID\":" + String(peers[i].fixID) + ",\"name\":\"" + String(peers[i].name) + "\""
          + ",\"mode\":\"" + String(peers[i].mode) + "\""
          + ",\"role\":\"" + (peers[i].role == ROLE_LEADER ? "LEADER" : "FOLLOWER") + "\",\"self\":false}";
  }
  json += "]";
  sendJson(req, 200, json);
}

void handleGetFixtures(AsyncWebServerRequest* req) {
  String json = "[";
  bool first = true;
  if (ownFixID > 0) {
    json += "{\"id\":" + String(ownFixID) + ",\"name\":\"" + String(ownName) + "\",\"mac\":\"" + String(ownMAC) + "\",\"online\":true,\"ip\":\"" + String(ownIP) + "\"}";
    first = false;
  }
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active || peers[i].fixID <= 0) continue;
    if (!first) json += ",";
    json += "{\"id\":" + String(peers[i].fixID) + ",\"name\":\"" + String(peers[i].name) + "\",\"mac\":\"" + String(peers[i].mac) + "\",\"online\":true,\"ip\":\"" + String(peers[i].ip) + "\"}";
    first = false;
  }
  json += "]";
  sendJson(req, 200, json);
}

void handleSetName(AsyncWebServerRequest* req) {
  String path = req->url();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  const char* name = doc["name"] | "";
  if (mac == String(ownMAC)) { discovery_saveName(name); sendJson(req, 200, "{\"status\":\"ok\"}"); return; }
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && String(peers[i].mac) == mac) {
      // Forward via HTTP — config commands no longer use UDP
      String url = "http://" + String(peers[i].ip) + "/api/config/name";
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      String body = "{\"name\":\"" + String(name) + "\"}";
      http.POST(body);
      http.end();
      strlcpy(peers[i].name, name, sizeof(peers[i].name));
      sendJson(req, 200, "{\"status\":\"ok\"}"); return;
    }
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
  if (mac == String(ownMAC)) { discovery_saveFixID(newID); sendJson(req, 200, "{\"status\":\"ok\",\"fixID\":"+String(newID)+"}"); return; }
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && String(peers[i].mac) == mac) {
      // Forward via HTTP — config commands no longer use UDP
      String url = "http://" + String(peers[i].ip) + "/api/config/fixid";
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      String body = "{\"fixID\":" + String(newID) + "}";
      http.POST(body);
      http.end();
      peers[i].fixID = newID;
      sendJson(req, 200, "{\"status\":\"ok\",\"fixID\":"+String(newID)+"}"); return;
    }
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
    if (peers[i].active && String(peers[i].mac) == mac) {
      if (on) udp_sendIdentifyOn(peers[i].ip, peers[i].mac);
      else    udp_sendIdentifyOff(peers[i].ip, peers[i].mac);
      sendJson(req, 200, "{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleSend(AsyncWebServerRequest* req) {
  String body = _getBody(req);
  Serial.printf("[Send] body=%s\n", body.isEmpty() ? "<empty>" : body.c_str());
  if (body.isEmpty()) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String cmd = doc["command"] | "";
  cmd.trim();
  JsonArray targets = doc["targets"].as<JsonArray>();
  if (targets.isNull() || targets.size() == 0) {
    rainbowActive = false; demoActive = false;
    applyCommand(cmd);
  } else {
    for (JsonVariant t : targets) {
      String mac = t.as<String>();
      if (mac == "*") { rainbowActive = false; demoActive = false; applyCommand(cmd); udp_broadcastCommand(cmd.c_str()); break; }
      else if (mac == String(ownMAC) || mac == "self") { rainbowActive = false; demoActive = false; applyCommand(cmd); }
      else {
        for (int i = 0; i < peerCount; i++) {
          if (peers[i].active && String(peers[i].mac) == mac) {
            udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str()); break;
          }
        }
      }
    }
  }
  sendJson(req, 200, "{\"status\":\"ok\",\"response\":\"OK\"}");
}

void handleRainbow(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  String cmd = on ? "RAINBOW:1" : "RAINBOW:0";
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  Serial.printf("[WiFi] Rainbow global %s\n", on ? "ON" : "OFF");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleDemo(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  String cmd = on ? "DEMO:1" : "DEMO:0";
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  Serial.printf("[WiFi] Demo global %s\n", on ? "ON" : "OFF");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleAnimSpeed(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  float s = doc["speed"] | 1.0f;
  s = constrain(s, 0.1f, 3.0f);
  String cmd = "SPEED:" + String(s, 2);
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleBlackout(AsyncWebServerRequest* req) {
  applyCommand("BLACKOUT");
  udp_broadcastCommand("BLACKOUT");
  Serial.println("[WiFi] Blackout");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// ── AP password ───────────────────────────────────────────────────
void handleSetAPPassword(AsyncWebServerRequest* req) {
  if (!wifiAPMode) { sendJson(req, 403, "{\"status\":\"error\",\"message\":\"Not in AP mode\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String pw = doc["password"] | "";
  if (pw.length() > 0 && pw.length() < 8) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Min. 8 characters (or empty to remove)\"}"); return;
  }
  // Save to /config.json — read first to preserve other keys
  JsonDocument cfg;
  storage_readJson("/config.json", cfg);
  cfg["apPassword"] = pw;
  storage_writeJson("/config.json", cfg);
  apPasswordSet = (pw.length() >= 8);
  // Restart softAP with updated credentials
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "MiniHead-%02X%02X", deviceMAC[4], deviceMAC[5]);
  WiFi.softAPdisconnect(false);
  if (apPasswordSet) WiFi.softAP(ssid, pw.c_str());
  else               WiFi.softAP(ssid);
  Serial.printf("[WiFi] AP password %s\n", apPasswordSet ? "updated" : "cleared");
  sendJson(req, 200, String("{\"status\":\"ok\",\"reconnect\":") + (apPasswordSet ? "true" : "false") + "}");
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
  c.pan  = constrain((int)doc["pan"],  0, 270);
  c.tilt = constrain((int)doc["tilt"], 0, 270);
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
  String body = _getBody(req);
  if (body.isEmpty()) { Serial.println("[Seq] Start: empty body"); sendJson(req, 400, "{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  if (deserializeJson(doc, body)) { Serial.println("[Seq] Start: bad JSON"); sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  seqInterval = doc["interval_ms"] | 1000;
  seqLoop     = doc["loop"]        | true;
  JsonArray ids = doc["cue_ids"].as<JsonArray>();
  seqIdCount = 0;
  for (JsonVariant v : ids) if (seqIdCount < MAX_CUES) seqIds[seqIdCount++] = v.as<unsigned long>();
  seqIndex = 0; seqRunning = (seqIdCount > 0);
  lastSeqStep = millis() - seqInterval;  // fire first cue immediately
  Serial.printf("[Seq] Start: %d cue(s), interval=%lums, loop=%d\n", seqIdCount, seqInterval, (int)seqLoop);
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// ── Log config handlers ───────────────────────────────────────────

void handleLogPanel(AsyncWebServerRequest* req) {
  sendHtmlProgmem(req, LOG_PANEL_HTML);
}
void handleGetLogConfig(AsyncWebServerRequest* req) {
  sendJson(req, 200, logcfg_toJson());
}
void handleSetLogConfig(AsyncWebServerRequest* req) {
  logcfg_fromJson(_getBody(req));
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// ── Config routes (always active — no requireLeader() guard) ─────
// Called from setupRoutes() for LEADER/FOLLOWER and directly for
// ARTNET ESPs which skip the full HTTP server.

void setupConfigRoutes() {
  // POST /api/config/fixid  {"fixID": N}
  server.on("/api/config/fixid", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      int id = doc["fixID"] | 0;
      if (id > 0) { discovery_saveFixID(id); Serial.printf("[Config] FixID set to %d\n", id); }
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });

  // POST /api/config/name  {"name": "..."}
  server.on("/api/config/name", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      const char* name = doc["name"] | "";
      discovery_saveName(name);
      Serial.printf("[Config] Name set to \"%s\"\n", ownName);
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });

#ifdef PLUGIN_ARTNET
  // POST /api/config/patch  {"universe": U, "startAddr": A}
  // Only on ARTNET ESPs — UDP ESPs have no patch concept.
  server.on("/api/config/patch", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      int uni  = doc["universe"]  | 0;
      int addr = doc["startAddr"] | 1;
      artnet_upsertPatch((uint16_t)uni, (uint16_t)addr);
      Serial.printf("[Config] Patch: U%d @%d\n", uni, addr);
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });
#endif
}

// ── Route setup ───────────────────────────────────────────────────

void setupRoutes() {
  // ── Config endpoints (always active, no leader guard) ────────
  setupConfigRoutes();

  // ── CORS preflight (OPTIONS) ──────────────────────────────────
  server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200);
    r->addHeader("Access-Control-Allow-Origin",  "*");
    r->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    r->addHeader("Access-Control-Allow-Headers", "Content-Type");
    req->send(r);
  });

  // ── Theme CSS ─────────────────────────────────────────────────
  server.on("/theme", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/css", THEME_CSS);
    r->addHeader("Access-Control-Allow-Origin", "*");
    req->send(r);
  });

  // ── Static HTML pages ─────────────────────────────────────────
  server.on("/",                       HTTP_GET, [](AsyncWebServerRequest* r){ handleRoot(r); });
  server.on("/plugins/log/panel.html", HTTP_GET, [](AsyncWebServerRequest* r){ handleLogPanel(r); });
  // Discovery panel  → registered by udp_control plugin
  // Art-Net panel    → registered by artnet plugin
  // Art-Net API      → registered by artnet plugin

  // ── Log config API ────────────────────────────────────────────
  server.on("/api/logconfig", HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetLogConfig(r); });
  server.on("/api/logconfig", HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSetLogConfig(r); },
    nullptr, _bodyAccumulator);

  // ── General API ───────────────────────────────────────────────
  server.on("/api/status",    HTTP_GET,  [](AsyncWebServerRequest* r){ handleStatus(r); });
  server.on("/api/version",   HTTP_GET,  [](AsyncWebServerRequest* r){ handleVersion(r); });
  server.on("/api/ports",     HTTP_GET,  [](AsyncWebServerRequest* r){ handlePorts(r); });
  server.on("/api/connect",   HTTP_POST, [](AsyncWebServerRequest* r){ handleConnect(r); });
  server.on("/api/disconnect",HTTP_POST, [](AsyncWebServerRequest* r){ handleDisconnect(r); });
  server.on("/api/send",      HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSend(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/rainbow",          HTTP_POST,
    [](AsyncWebServerRequest* r){ handleRainbow(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/demo",             HTTP_POST,
    [](AsyncWebServerRequest* r){ handleDemo(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/animation/speed",  HTTP_POST,
    [](AsyncWebServerRequest* r){ handleAnimSpeed(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/blackout",         HTTP_POST,
    [](AsyncWebServerRequest* r){ handleBlackout(r); });
  server.on("/api/ap/password",      HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSetAPPassword(r); },
    nullptr, _bodyAccumulator);

  // ── Cues — /reorder must come before /* ───────────────────────
  server.on("/api/cues",         HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetCues(r); });
  server.on("/api/cues",         HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSaveCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/reorder", HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleReorderCues(r); },
    nullptr, _bodyAccumulator);
  // Wildcard for /api/cues/<id>/fire|targets and DELETE <id>
  server.on("/api/cues/*",       HTTP_POST,
    [](AsyncWebServerRequest* r){ handleFireCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleUpdateCueTargets(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_DELETE, [](AsyncWebServerRequest* r){ handleDeleteCue(r); });

  // ── Sequencer ─────────────────────────────────────────────────
  server.on("/api/sequencer/start",  HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSeqStart(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/sequencer/stop",   HTTP_POST, [](AsyncWebServerRequest* r){ handleSeqStop(r); });
  server.on("/api/sequencer/status", HTTP_GET,  [](AsyncWebServerRequest* r){ handleSeqStatus(r); });

  // ── Heads & Fixtures ──────────────────────────────────────────
  server.on("/api/heads",    HTTP_GET, [](AsyncWebServerRequest* r){ handleGetHeads(r); });
  server.on("/api/fixtures", HTTP_GET, [](AsyncWebServerRequest* r){ handleGetFixtures(r); });
  // Wildcard for /api/heads/<mac>/identify|fixid|name
  server.on("/api/heads/*",  HTTP_POST,
    [](AsyncWebServerRequest* r){
      String path = r->url();
      if (path.endsWith("/identify")) handleIdentify(r);
      else if (path.endsWith("/fixid"))   handleSetFixID(r);
      else if (path.endsWith("/name"))    handleSetName(r);
      else r->send(404, "text/plain", "Not found");
    },
    nullptr, _bodyAccumulator);

  // ── Captive portal suppression ───────────────────────────────────
  // iOS/Android probe specific URLs to detect captive portals. Returning
  // the exact "Success" response makes them think there is internet access
  // → no CNA popup appears at all → network connects silently.
  // User then opens Safari and navigates to 192.168.4.1 / minihead.local.
  auto _ok = [](AsyncWebServerRequest* r){
    r->send(200, "text/html",
      "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); };
  server.on("/hotspot-detect.html",       HTTP_GET, _ok);  // iOS / macOS
  server.on("/library/test/success.html", HTTP_GET, _ok);  // iOS older
  server.on("/generate_204",              HTTP_GET, [](AsyncWebServerRequest* r){ r->send(204); });  // Android
  server.on("/connecttest.txt",           HTTP_GET, [](AsyncWebServerRequest* r){ r->send(200,"text/plain","Microsoft Connect Test"); });  // Windows
  server.on("/redirect",                  HTTP_GET, _ok);
  server.on("/canonical.html",            HTTP_GET, _ok);
  server.on("/success.txt",               HTTP_GET, _ok);

  // ── 404 fallback ─────────────────────────────────────────────────
  server.onNotFound([](AsyncWebServerRequest* r){
    r->send(404, "text/plain", "Not found");
  });
}

// ── Lifecycle ─────────────────────────────────────────────────────

void wifi_control_setup() {
#ifdef PLUGIN_ARTNET
  // Art-Net mode: start the FULL HTTP server so the web UI is accessible
  // for standalone operation (Demo, Blackout, Speed, cues) without a PC App.
  // ArtNet reception runs independently on UDP port 6454 — no conflict.
  _serverActive = true;
  logcfg_load();
  setupRoutes();   // already calls setupConfigRoutes() internally
  if (!_serverStarted) {
    server.begin();
    _serverStarted = true;
  }
  Serial.println("[WiFi] Art-Net mode — full HTTP server :80");
  return;
#endif
  _serverActive = true;
  logcfg_load();
  loadCuesFromFlash();
  // In AP mode localIP() returns 0.0.0.0 — use softAPIP() as fallback
  IPAddress _ip = WiFi.localIP();
  String _ipStr = (_ip != IPAddress(0,0,0,0)) ? _ip.toString() : WiFi.softAPIP().toString();
  Serial.println("[WiFi] IP: " + _ipStr);
  Serial.flush();

  Serial.println("[WiFi] Registering routes...");
  Serial.flush();
  setupRoutes();
  Serial.println("[WiFi] Routes OK");
  Serial.flush();

  if (!_serverStarted) {
    Serial.println("[WiFi] Calling server.begin()...");
    Serial.flush();
    server.begin();
    _serverStarted = true;
    Serial.println("[WiFi] Async server started");
    Serial.flush();
  } else {
    Serial.println("[WiFi] Async server resumed (already started)");
  }
}

void wifi_control_stop() {
  _serverActive = false;
  // Stop sequencer — if we lose leader role mid-sequence, don't resume
  // in a corrupt state when we become leader again later.
  seqRunning = false; seqIndex = 0; seqIdCount = 0;
  // AsyncWebServer has no stop() — socket stays bound on the WiFi task.
  // requireLeader() now redirects browsers to the actual leader's IP,
  // so a user navigating to this ESP's IP gets sent to the right place.
  Serial.println("[WiFi] Follower mode — HTTP requests will redirect to leader");
}

// Start the HTTP server in follower mode (routes registered, APIs inactive).
// Called by discovery when this node first boots as a follower so the browser
// gets a redirect response instead of a connection-refused error.
void wifi_control_setup_follower() {
  _serverActive = false;   // follower — APIs will redirect
  logcfg_load();
  // Skip loadCuesFromFlash — that writes state only meaningful on the leader.
  Serial.println("[WiFi] Follower — starting server for browser redirects");
  setupRoutes();
  if (!_serverStarted) {
    server.begin();
    _serverStarted = true;
    Serial.println("[WiFi] Async server started (follower)");
  }
}

// Called when this node wins election after having been a follower —
// the server is already running, just activate the API layer.
void wifi_control_promote() {
#ifdef PLUGIN_ARTNET
  return;  // Art-Net mode: never promote — PC App is always leader
#endif
  _serverActive = true;
  logcfg_load();
  loadCuesFromFlash();
  Serial.println("[WiFi] Promoted to LEADER — APIs active");
}

void wifi_control_loop() {
  if (!_serverActive) return;
  if (seqRunning && seqIdCount > 0) {
    unsigned long now = millis();
    if (now - lastSeqStep >= seqInterval) {
      lastSeqStep = now;
      unsigned long tid = seqIds[seqIndex];
      Serial.printf("[Seq] Step %d/%d  cue#%lu\n", seqIndex + 1, seqIdCount, tid);
      bool found = false;
      for (int i = 0; i < cueCount; i++) {
        if (cues[i].id == tid) { fireCueToTargets(cues[i]); found = true; break; }
      }
      if (!found) Serial.printf("[Seq] Cue #%lu not found (cueCount=%d)\n", tid, cueCount);
      if (++seqIndex >= seqIdCount) { if (seqLoop) seqIndex = 0; else seqRunning = false; }
    }
  }
}
// Note: wifi_control is NOT registered as a plugin — it is started
// and stopped by discovery.h when the node wins/loses election.
