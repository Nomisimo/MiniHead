#pragma once
// ── Serial Log Configuration ─────────────────────────────────────
// Persisted to /logcfg.json. Toggleable at runtime via /api/logconfig.
// Include this header before any plugin that uses logCfg.
// ─────────────────────────────────────────────────────────────────
#include <ArduinoJson.h>
#include "../storage/storage.h"

struct LogConfig {
  bool artnetFrames     = false;  // per-frame R/G/B/W/Pan/Tilt values (very spammy)
  bool artnetEvents     = true;   // ArtNet active / timeout / patch changed
  bool discoveryBeacons = false;  // every beacon heard (spammy)
  bool discoveryEvents  = true;   // role change, leader found/lost
  bool udpVerbose       = false;  // all UDP commands received / identify events
};

LogConfig logCfg;

void logcfg_load() {
  JsonDocument doc;
  if (!storage_readJson("/logcfg.json", doc)) return;
  logCfg.artnetFrames     = doc["artnetFrames"]     | false;
  logCfg.artnetEvents     = doc["artnetEvents"]     | true;
  logCfg.discoveryBeacons = doc["discoveryBeacons"] | false;
  logCfg.discoveryEvents  = doc["discoveryEvents"]  | true;
  logCfg.udpVerbose       = doc["udpVerbose"]       | false;
  Serial.println("[LogCfg] Loaded");
}

void logcfg_save() {
  JsonDocument doc;
  doc["artnetFrames"]     = logCfg.artnetFrames;
  doc["artnetEvents"]     = logCfg.artnetEvents;
  doc["discoveryBeacons"] = logCfg.discoveryBeacons;
  doc["discoveryEvents"]  = logCfg.discoveryEvents;
  doc["udpVerbose"]       = logCfg.udpVerbose;
  storage_writeJson("/logcfg.json", doc);
}

String logcfg_toJson() {
  String j = "{";
  j += "\"artnetFrames\":"     + String(logCfg.artnetFrames     ? "true":"false") + ",";
  j += "\"artnetEvents\":"     + String(logCfg.artnetEvents     ? "true":"false") + ",";
  j += "\"discoveryBeacons\":" + String(logCfg.discoveryBeacons ? "true":"false") + ",";
  j += "\"discoveryEvents\":"  + String(logCfg.discoveryEvents  ? "true":"false") + ",";
  j += "\"udpVerbose\":"       + String(logCfg.udpVerbose       ? "true":"false");
  j += "}";
  return j;
}

void logcfg_fromJson(const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  if (doc.containsKey("artnetFrames"))     logCfg.artnetFrames     = doc["artnetFrames"];
  if (doc.containsKey("artnetEvents"))     logCfg.artnetEvents     = doc["artnetEvents"];
  if (doc.containsKey("discoveryBeacons")) logCfg.discoveryBeacons = doc["discoveryBeacons"];
  if (doc.containsKey("discoveryEvents"))  logCfg.discoveryEvents  = doc["discoveryEvents"];
  if (doc.containsKey("udpVerbose"))       logCfg.udpVerbose       = doc["udpVerbose"];
  logcfg_save();
}
