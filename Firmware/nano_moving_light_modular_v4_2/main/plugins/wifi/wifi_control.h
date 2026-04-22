#pragma once

// ── WiFi Control Plugin ───────────────────────────────────────────
// Handles: HTTP server, all API endpoints, cue storage, sequencer
// Only runs when nodeRole == ROLE_LEADER (started by discovery.h)
// Depends on: core.h, discovery_globals.h, udp_control.h
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "html_page.h"
#include "core_globals.h"
#include "discovery_panel_html.h"
#include "discovery_globals.h"
#include "udp_control.h"
#include "../artnet/artnet_panel_html.h"

#define MAX_CUES    32
#define MAX_TARGETS 16   // max FixID targets per cue

WebServer server(80);

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

bool seqRunning = false;
unsigned long seqInterval = 1000;
bool seqLoop = true;
unsigned long seqIds[MAX_CUES];
int seqIdCount = 0;
int seqIndex = 0;
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
  Serial.printf("[WiFi] Loaded %d cue(s) from /cues.json\n", cueCount);
}

// ── Helpers ───────────────────────────────────────────────────────

void sendJson(int code, const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", json);
}

// artnet_control.h needs server + sendJson — include after they're defined
#include "../artnet/artnet_control.h"

String fixTargetsToJson(const Cue& c) {
  String s = "[";
  for (int i=0; i<c.targetCount; i++) { if (i>0) s+=","; s+=String(c.fixTargets[i]); }
  s+="]"; return s;
}

String cueToJson(const Cue& c) {
  return "{\"id\":" + String(c.id) +
    ",\"name\":\"" + String(c.name) + "\"" +
    ",\"r\":"   + c.r   + ",\"g\":" + c.g +
    ",\"b\":"   + c.b   + ",\"w\":" + c.w +
    ",\"pan\":" + c.pan + ",\"tilt\":" + c.tilt +
    ",\"fixTargets\":" + fixTargetsToJson(c) + "}";
}

// ── Fire a cue to all its targets ────────────────────────────────

void fireCueToTargets(const Cue& c) {
  String cmd = "R:" + String(c.r) + ",G:" + String(c.g) +
               ",B:" + String(c.b) + ",W:" + String(c.w) +
               ",PAN:" + String(c.pan) + ",TILT:" + String(c.tilt);

  bool toAll = (c.targetCount == 0) || (c.targetCount == 1 && c.fixTargets[0] == 0);
  if (toAll) {
    rainbowActive = false;
    applyCommand(cmd);
    udp_broadcastCommand(cmd.c_str());
    return;
  }

  for (int t = 0; t < c.targetCount; t++) {
    int fid = c.fixTargets[t];
    if (ownFixID > 0 && ownFixID == fid) { rainbowActive = false; applyCommand(cmd); }
    for (int i = 0; i < peerCount; i++) {
      if (peers[i].active && peers[i].fixID == fid) {
        udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str());
        break;
      }
    }
  }
}

// ── Route handlers ────────────────────────────────────────────────

void handleRoot()           { server.sendHeader("Access-Control-Allow-Origin","*"); server.send_P(200,"text/html",INDEX_HTML); }
void handleDiscoveryPanel() { server.sendHeader("Access-Control-Allow-Origin","*"); server.send_P(200,"text/html",DISCOVERY_PANEL_HTML); }
void handleStatus()     { sendJson(200,"{\"connected\":true,\"port\":\"WiFi\",\"ip\":\""+WiFi.localIP().toString()+"\"}"); }
void handlePorts()      { sendJson(200,"[{\"port\":\"WiFi\",\"description\":\"ESP32 @ "+WiFi.localIP().toString()+"\"}]"); }
void handleConnect()    { sendJson(200,"{\"status\":\"ok\",\"port\":\"WiFi\"}"); }
void handleDisconnect() { sendJson(200,"{\"status\":\"ok\"}"); }
void handleSeqStop()    { seqRunning=false; sendJson(200,"{\"status\":\"ok\"}"); }
void handleSeqStatus()  { sendJson(200,String("{\"running\":"+(seqRunning?String("true"):String("false"))+"}")); }

void handleGetHeads() {
  String json = "[";
  json += "{\"mac\":\"" + String(ownMAC) + "\",\"ip\":\"" + String(ownIP) + "\"" +
          ",\"fixID\":" + String(ownFixID) + ",\"name\":\"" + String(ownName) + "\"" +
          ",\"role\":\"LEADER\",\"self\":true}";
  for (int i=0; i<peerCount; i++) {
    if (!peers[i].active) continue;
    json += ",{\"mac\":\"" + String(peers[i].mac) + "\",\"ip\":\"" + String(peers[i].ip) + "\"" +
            ",\"fixID\":" + String(peers[i].fixID) + ",\"name\":\"" + String(peers[i].name) + "\"" +
            ",\"role\":\"" + (peers[i].role==ROLE_LEADER?"LEADER":"FOLLOWER") + "\",\"self\":false}";
  }
  json += "]";
  sendJson(200, json);
}

void handleSetName() {
  String path = server.uri();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  const char* name = doc["name"] | "";
  if (mac == String(ownMAC)) { discovery_saveName(name); sendJson(200,"{\"status\":\"ok\"}"); return; }
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && String(peers[i].mac) == mac) {
      udp_sendSetName(peers[i].ip, peers[i].mac, name);
      strlcpy(peers[i].name, name, sizeof(peers[i].name));
      sendJson(200,"{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleGetFixtures() {
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
  sendJson(200, json);
}

void handleSetFixID() {
  String path = server.uri();
  String mac = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  int newID = doc["fixID"] | 0;
  if (mac == String(ownMAC)) { discovery_saveFixID(newID); sendJson(200,"{\"status\":\"ok\",\"fixID\":"+String(newID)+"}"); return; }
  for (int i=0; i<peerCount; i++) {
    if (peers[i].active && String(peers[i].mac) == mac) {
      char cmd[32]; snprintf(cmd, sizeof(cmd), "SETFIXID:%d", newID);
      udp_sendCommand(peers[i].ip, peers[i].mac, cmd);
      peers[i].fixID = newID;
      sendJson(200,"{\"status\":\"ok\",\"fixID\":"+String(newID)+"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleIdentify() {
  String path = server.uri();
  String mac  = path.substring(String("/api/heads/").length());
  mac = mac.substring(0, mac.lastIndexOf('/'));
  mac.toUpperCase();
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  bool on = doc["on"] | false;
  if (mac == String(ownMAC) || mac == "SELF") {
    if (on) setLED(255,255,255,255); else setLED(curR,curG,curB,curW);
    sendJson(200,"{\"status\":\"ok\"}"); return;
  }
  for (int i=0; i<peerCount; i++) {
    if (peers[i].active && String(peers[i].mac) == mac) {
      if (on) udp_sendIdentifyOn(peers[i].ip, peers[i].mac);
      else    udp_sendIdentifyOff(peers[i].ip, peers[i].mac);
      sendJson(200,"{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Peer not found\"}");
}

void handleSend() {
  if (!server.hasArg("plain")) { sendJson(400,"{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String cmd = doc["command"] | "";
  cmd.trim();
  JsonArray targets = doc["targets"].as<JsonArray>();
  if (targets.isNull() || targets.size() == 0) {
    applyCommand(cmd);
  } else {
    for (JsonVariant t : targets) {
      String mac = t.as<String>();
      if (mac == "*") { applyCommand(cmd); udp_broadcastCommand(cmd.c_str()); break; }
      else if (mac == String(ownMAC) || mac == "self") { applyCommand(cmd); }
      else {
        for (int i=0; i<peerCount; i++) {
          if (peers[i].active && String(peers[i].mac) == mac) { udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str()); break; }
        }
      }
    }
  }
  sendJson(200,"{\"status\":\"ok\",\"response\":\"OK\"}");
}

// ── /api/rainbow  POST {on:true/false} ───────────────────────────
// Broadcasts RAINBOW:1 or RAINBOW:0 to self + all followers.
void handleRainbow() {
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  bool on = doc["on"] | false;
  String cmd = on ? "RAINBOW:1" : "RAINBOW:0";
  applyCommand(cmd);                    // apply on leader
  udp_broadcastCommand(cmd.c_str());    // send to all followers
  Serial.printf("[WiFi] Rainbow global %s\n", on ? "ON" : "OFF");
  sendJson(200, "{\"status\":\"ok\"}");
}

void handleGetCues() {
  String json="[";
  for (int i=0; i<cueCount; i++) { if(i>0) json+=","; json+=cueToJson(cues[i]); }
  json+="]"; sendJson(200,json);
}

void handleSaveCue() {
  if (cueCount>=MAX_CUES) { sendJson(500,"{\"status\":\"error\",\"message\":\"Max cues\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  Cue& c = cues[cueCount];
  c.id = millis();
  strlcpy(c.name, doc["name"]|"Cue", sizeof(c.name));
  c.r    = constrain((int)doc["r"],    0,255);
  c.g    = constrain((int)doc["g"],    0,255);
  c.b    = constrain((int)doc["b"],    0,255);
  c.w    = constrain((int)doc["w"],    0,255);
  c.pan  = constrain((int)doc["pan"],  0,180);
  c.tilt = constrain((int)doc["tilt"], 0,180);
  c.targetCount = 0;
  JsonArray tArr = doc["fixTargets"].as<JsonArray>();
  if (!tArr.isNull()) { for (JsonVariant v : tArr) { if (c.targetCount >= MAX_TARGETS) break; c.fixTargets[c.targetCount++] = v.as<int>(); } }
  if (c.targetCount == 0) { c.fixTargets[0] = 0; c.targetCount = 1; }
  cueCount++;
  saveCuesToFlash();
  sendJson(200,"{\"status\":\"ok\",\"cue\":"+cueToJson(c)+"}");
}

void handleUpdateCueTargets() {
  String path = server.uri();
  String trimmed = path.substring(0, path.lastIndexOf('/'));
  unsigned long id = trimmed.substring(trimmed.lastIndexOf('/')+1).toInt();
  for (int i=0; i<cueCount; i++) {
    if (cues[i].id == id) {
      JsonDocument doc;
      if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
      JsonArray tArr = doc["fixTargets"].as<JsonArray>();
      cues[i].targetCount = 0;
      for (JsonVariant v : tArr) { if (cues[i].targetCount >= MAX_TARGETS) break; cues[i].fixTargets[cues[i].targetCount++] = v.as<int>(); }
      saveCuesToFlash();
      sendJson(200,"{\"status\":\"ok\",\"cue\":"+cueToJson(cues[i])+"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleDeleteCue() {
  String path=server.uri();
  unsigned long id=path.substring(path.lastIndexOf('/')+1).toInt();
  for (int i=0; i<cueCount; i++) {
    if (cues[i].id==id) {
      for (int j=i; j<cueCount-1; j++) cues[j]=cues[j+1];
      cueCount--; saveCuesToFlash();
      sendJson(200,"{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleFireCue() {
  String path=server.uri();
  String trimmed=path.substring(0,path.lastIndexOf('/'));
  unsigned long id=trimmed.substring(trimmed.lastIndexOf('/')+1).toInt();
  for (int i=0; i<cueCount; i++) {
    if (cues[i].id==id) { fireCueToTargets(cues[i]); sendJson(200,"{\"status\":\"ok\",\"command\":\"fired\",\"response\":\"OK\"}"); return; }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleReorderCues() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  JsonArray order = doc["order"].as<JsonArray>();
  if (order.isNull()) { sendJson(400,"{\"status\":\"error\",\"message\":\"Missing order\"}"); return; }
  Cue temp[MAX_CUES];
  int newCount = 0;
  for (JsonVariant v : order) {
    unsigned long id = v.as<unsigned long>();
    for (int i=0; i<cueCount; i++) {
      if (cues[i].id == id) { temp[newCount++] = cues[i]; break; }
    }
  }
  if (newCount != cueCount) { sendJson(400,"{\"status\":\"error\",\"message\":\"Count mismatch\"}"); return; }
  for (int i=0; i<cueCount; i++) cues[i] = temp[i];
  saveCuesToFlash();
  sendJson(200,"{\"status\":\"ok\"}");
}

void handleSeqStart() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  seqInterval = doc["interval_ms"]|1000;
  seqLoop     = doc["loop"]|true;
  JsonArray ids = doc["cue_ids"].as<JsonArray>();
  seqIdCount=0;
  for (JsonVariant v : ids) if(seqIdCount<MAX_CUES) seqIds[seqIdCount++]=v.as<unsigned long>();
  seqIndex=0; seqRunning=(seqIdCount>0); lastSeqStep=millis();
  sendJson(200,"{\"status\":\"ok\"}");
}

void setupRoutes() {
  server.on("/",                                           HTTP_GET,  handleRoot);
  server.on("/plugins/wifi/discovery_panel.html",          HTTP_GET,  handleDiscoveryPanel);
  server.on("/plugins/artnet/panel.html",    HTTP_GET,  [](){
    server.sendHeader("Cache-Control","no-cache");
    server.send_P(200,"text/html", ARTNET_PANEL_HTML);
  });
  // Art-Net API — register /bulk before / so more-specific path wins
  server.on("/api/artnet/status",           HTTP_GET,    handleArtnetStatus);
  server.on("/api/artnet/patch",            HTTP_GET,    handleGetArtnetPatch);
  server.on("/api/artnet/patch/bulk",       HTTP_POST,   handleBulkArtnetPatch);
  server.on("/api/artnet/patch",            HTTP_POST,   handlePostArtnetPatch);
  server.on("/api/artnet/patch",            HTTP_DELETE, handleClearAllArtnetPatches);
  server.on("/api/status",            HTTP_GET,  handleStatus);
  server.on("/api/ports",             HTTP_GET,  handlePorts);
  server.on("/api/connect",           HTTP_POST, handleConnect);
  server.on("/api/disconnect",        HTTP_POST, handleDisconnect);
  server.on("/api/send",              HTTP_POST, handleSend);
  server.on("/api/rainbow",           HTTP_POST, handleRainbow);
  server.on("/api/cues",              HTTP_GET,  handleGetCues);
  server.on("/api/cues",              HTTP_POST, handleSaveCue);
  server.on("/api/cues/reorder",      HTTP_PUT,  handleReorderCues);
  server.on("/api/sequencer/start",   HTTP_POST, handleSeqStart);
  server.on("/api/sequencer/stop",    HTTP_POST, handleSeqStop);
  server.on("/api/sequencer/status",  HTTP_GET,  handleSeqStatus);
  server.on("/api/heads",             HTTP_GET,  handleGetHeads);
  server.on("/api/fixtures",          HTTP_GET,  handleGetFixtures);
  server.onNotFound([](){
    String path=server.uri();
    if (path.startsWith("/api/heads/")) {
      if (path.endsWith("/identify") && server.method()==HTTP_POST) { handleIdentify(); return; }
      if (path.endsWith("/fixid")    && server.method()==HTTP_POST) { handleSetFixID(); return; }
      if (path.endsWith("/name")     && server.method()==HTTP_POST) { handleSetName();  return; }
    }
    if (path.startsWith("/api/cues/")) {
      if (path.endsWith("/fire")    && server.method()==HTTP_POST) { handleFireCue(); return; }
      if (path.endsWith("/targets") && server.method()==HTTP_PUT)  { handleUpdateCueTargets(); return; }
      if (server.method()==HTTP_DELETE) { handleDeleteCue(); return; }
    }
    if (path.startsWith("/api/artnet/patch/universe/") && server.method()==HTTP_DELETE)
      { handleClearUniverseArtnetPatches(); return; }
    if (path.startsWith("/api/artnet/patch/") && server.method()==HTTP_DELETE)
      { handleDeleteArtnetPatch(); return; }
    if (path.startsWith("/api/artnet/patch/") && server.method()==HTTP_PUT)
      { handleUpdateArtnetPatch(); return; }
    server.send(404,"text/plain","Not found");
  });
}

void wifi_control_setup() {
  loadCuesFromFlash();
  artnet_control_setup();
  Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
  setupRoutes();
  server.begin();
  Serial.println("[WiFi] Server started");
}

void wifi_control_stop() {
  server.stop();
  Serial.println("[WiFi] Server stopped (not leader)");
}

void wifi_control_loop() {
  server.handleClient();
  if (seqRunning && seqIdCount>0) {
    unsigned long now=millis();
    if (now-lastSeqStep>=seqInterval) {
      lastSeqStep=now;
      unsigned long tid=seqIds[seqIndex];
      for (int i=0; i<cueCount; i++) {
        if (cues[i].id==tid) { fireCueToTargets(cues[i]); break; }
      }
      if (++seqIndex>=seqIdCount) { if(seqLoop) seqIndex=0; else seqRunning=false; }
    }
  }
}
// Note: wifi_control is NOT registered as a plugin — it is started
// and stopped by discovery.h when the node wins/loses election.
