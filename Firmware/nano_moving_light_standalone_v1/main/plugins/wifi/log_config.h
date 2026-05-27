#pragma once
// ── Serial Log Configuration (Standalone v1) ─────────────────────
// Persisted to /logcfg.json. Toggleable at runtime via /api/logconfig.
// ─────────────────────────────────────────────────────────────────
#include <ArduinoJson.h>
#include "../storage/storage.h"

struct LogConfig {
  bool httpVerbose = false;  // log every HTTP request
};

LogConfig logCfg;

void logcfg_load() {
  JsonDocument doc;
  if (!storage_readJson("/logcfg.json", doc)) return;
  logCfg.httpVerbose = doc["httpVerbose"] | false;
}

void logcfg_save() {
  JsonDocument doc;
  doc["httpVerbose"] = logCfg.httpVerbose;
  storage_writeJson("/logcfg.json", doc);
}

String logcfg_toJson() {
  return String("{\"httpVerbose\":") + (logCfg.httpVerbose ? "true" : "false") + "}";
}

void logcfg_fromJson(const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  if (doc.containsKey("httpVerbose")) logCfg.httpVerbose = doc["httpVerbose"];
  logcfg_save();
}
