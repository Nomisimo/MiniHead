#pragma once

// ── Cue storage, sequencer, and HTTP handlers ─────────────────────
// Cue struct + globals, LittleFS persistence (/cues.json),
// handleGetCues, handleSaveCue, handleUpdateCueTargets,
// handleDeleteCue — also removes deleted cue from active sequence,
// handleFireCue, handleReorderCues,
// handleSeqStart, handleSeqStop, handleSeqStatus,
// wifi_cues_loop (call from wifi_control_loop each iteration)
// ─────────────────────────────────────────────────────────────────

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
static unsigned long _cueIdSeq = 0;

bool seqRunning           = false;
unsigned long seqInterval = 1000;
bool seqLoop              = true;
unsigned long seqIds[MAX_CUES];
int seqIdCount            = 0;
int seqIndex              = 0;
unsigned long lastSeqStep = 0;

// ── LittleFS persistence ──────────────────────────────────────────
// Schema: [{"id":N,"name":"...","r":255,"g":0,"b":0,"w":0,
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
    c.id = o["id"] | (unsigned long)0;
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
  // Seed the ID counter above the highest loaded ID to avoid collisions
  for (int i = 0; i < cueCount; i++)
    if (cues[i].id > _cueIdSeq) _cueIdSeq = cues[i].id;
  Serial.printf("[WiFi] Loaded %d cue(s) from /cues.json\n", cueCount);
}

// ── Cue JSON helpers ──────────────────────────────────────────────

static String fixTargetsToJson(const Cue& c) {
  String s = "[";
  for (int i = 0; i < c.targetCount; i++) { if (i > 0) s += ","; s += String(c.fixTargets[i]); }
  return s + "]";
}

static String cueToJson(const Cue& c) {
  return "{\"id\":" + String(c.id) +
    ",\"name\":\"" + _jsonEscapeStr(c.name) + "\"" +
    ",\"r\":"   + c.r   + ",\"g\":" + c.g  +
    ",\"b\":"   + c.b   + ",\"w\":" + c.w  +
    ",\"pan\":" + c.pan + ",\"tilt\":" + c.tilt +
    ",\"fixTargets\":" + fixTargetsToJson(c) + "}";
}

// ── Fire a cue to all its targets ────────────────────────────────

static void fireCueToTargets(const Cue& c) {
  String cmd = "R:" + String(c.r) + ",G:" + String(c.g) +
               ",B:" + String(c.b) + ",W:" + String(c.w) +
               ",PAN:" + String(c.pan) + ",TILT:" + String(c.tilt);

  bool toAll = (c.targetCount == 0) || (c.targetCount == 1 && c.fixTargets[0] == 0);
  Serial.printf("[Seq] Fire '%s': %s  toAll=%d\n", c.name, cmd.c_str(), (int)toAll);
  if (toAll) {
    rainbowActive = false; demoActive = false;
    applyCommand(cmd.c_str());
    udp_broadcastCommand(cmd.c_str());
    return;
  }
  for (int t = 0; t < c.targetCount; t++) {
    int fid = c.fixTargets[t];
    if (ownFixID > 0 && ownFixID == fid) { rainbowActive = false; demoActive = false; applyCommand(cmd.c_str()); }
    for (int i = 0; i < peerCount; i++) {
      if (peers[i].active && peers[i].fixID == fid) {
        udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str());
        break;
      }
    }
  }
}

// ── HTTP handlers ─────────────────────────────────────────────────

void handleGetCues(AsyncWebServerRequest* req) {
  String json = "[";
  for (int i = 0; i < cueCount; i++) { if (i > 0) json += ","; json += cueToJson(cues[i]); }
  sendJson(req, 200, json + "]");
}

void handleSaveCue(AsyncWebServerRequest* req) {
  if (cueCount >= MAX_CUES) { sendJson(req, 500, "{\"status\":\"error\",\"message\":\"Max cues reached\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\"}"); return; }
  Cue& c = cues[cueCount];
  c.id = ++_cueIdSeq;
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
  String path    = req->url();
  String trimmed = path.substring(0, path.lastIndexOf('/'));
  unsigned long id = trimmed.substring(trimmed.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id != id) continue;
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
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleDeleteCue(AsyncWebServerRequest* req) {
  String path    = req->url();
  unsigned long id = path.substring(path.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id != id) continue;
    for (int j = i; j < cueCount - 1; j++) cues[j] = cues[j + 1];
    cueCount--;
    saveCuesToFlash();
    // Remove from active sequence so playback has no gaps
    for (int j = 0; j < seqIdCount; j++) {
      if (seqIds[j] == id) {
        for (int k = j; k < seqIdCount - 1; k++) seqIds[k] = seqIds[k + 1];
        seqIdCount--;
        if (seqIndex > 0 && seqIndex >= seqIdCount) seqIndex = 0;
        break;
      }
    }
    if (seqIdCount == 0) seqRunning = false;
    sendJson(req, 200, "{\"status\":\"ok\"}"); return;
  }
  sendJson(req, 404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleFireCue(AsyncWebServerRequest* req) {
  String path    = req->url();
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
  String body = _getBody(req);
  if (body.isEmpty()) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  seqInterval = doc["interval_ms"] | 1000;
  seqLoop     = doc["loop"]        | true;
  JsonArray ids = doc["cue_ids"].as<JsonArray>();
  seqIdCount = 0;
  for (JsonVariant v : ids) if (seqIdCount < MAX_CUES) seqIds[seqIdCount++] = v.as<unsigned long>();
  seqIndex    = 0;
  seqRunning  = (seqIdCount > 0);
  lastSeqStep = millis() - seqInterval;  // fire first cue immediately
  Serial.printf("[Seq] Start: %d cue(s), interval=%lums, loop=%d\n", seqIdCount, seqInterval, (int)seqLoop);
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleSeqStop(AsyncWebServerRequest* req)   { seqRunning = false; sendJson(req, 200, "{\"status\":\"ok\"}"); }
void handleSeqStatus(AsyncWebServerRequest* req) { sendJson(req, 200, String("{\"running\":") + (seqRunning ? "true" : "false") + "}"); }

// ── Sequencer tick (called from wifi_control_loop each iteration) ─
void wifi_cues_loop() {
  if (!seqRunning || seqIdCount == 0) return;
  unsigned long now = millis();
  if (now - lastSeqStep < seqInterval) return;
  lastSeqStep = now;
  unsigned long tid = seqIds[seqIndex];
  Serial.printf("[Seq] Step %d/%d  cue#%lu\n", seqIndex + 1, seqIdCount, tid);
  bool found = false;
  for (int i = 0; i < cueCount; i++) {
    if (cues[i].id == tid) { fireCueToTargets(cues[i]); found = true; break; }
  }
  if (!found) Serial.printf("[Seq] Cue #%lu not found\n", tid);
  if (++seqIndex >= seqIdCount) { if (seqLoop) seqIndex = 0; else seqRunning = false; }
}
