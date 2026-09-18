#pragma once

// ── Storage Utility ───────────────────────────────────────────────
// Centralised LittleFS JSON helpers for v4.2.
// Replaces all Preferences / NVS usage across plugins.
//
// Usage:
//   Call storage_begin() once at boot (in core_setup, before plugins).
//   Use storage_readJson() / storage_writeJson() in place of Preferences.
//
// JSON files live at the root of LittleFS:
//   /discovery.json  — fixID, name
//   /cues.json       — cue array
//   /artnet.json     — patch array
// ─────────────────────────────────────────────────────────────────

#include <LittleFS.h>
#include <ArduinoJson.h>

// Mount LittleFS. Call once before any plugin setup().
// Returns true on success. Formats the partition on first boot (format-on-fail).
bool storage_begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] LittleFS mount failed");
    return false;
  }
  Serial.println("[Storage] LittleFS OK");
  return true;
}

// Read a JSON file from LittleFS into doc.
// Returns true and fills doc on success.
// Returns false (and leaves doc empty) if the file doesn't exist or is malformed.
bool storage_readJson(const char* path, JsonDocument& doc) {
  File f = LittleFS.open(path, "r");
  if (!f || f.isDirectory()) {
    Serial.printf("[Storage] %s — not found\n", path);
    return false;
  }
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[Storage] %s — parse error: %s\n", path, err.c_str());
    return false;
  }
  return true;
}

// Write doc as compact JSON to path on LittleFS.
// Returns true on success.
bool storage_writeJson(const char* path, const JsonDocument& doc) {
  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.printf("[Storage] %s — open for write failed\n", path);
    return false;
  }
  serializeJson(doc, f);
  f.close();
  Serial.printf("[Storage] %s — written\n", path);
  return true;
}
