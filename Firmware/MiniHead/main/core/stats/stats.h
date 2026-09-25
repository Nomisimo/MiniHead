#pragma once

// ── Runtime Statistics ────────────────────────────────────────────
// Tracks: total run hours, pan/tilt servo rotations (540° = 1 rotation),
// LED-on hours per RGBW channel and overall.
// Persisted to /stats.json every 5 minutes via LittleFS.
//
// API:
//   GET    /api/stats              — return current counters as JSON
//   DELETE /api/stats?confirm=DELETE — reset all counters and remove /stats.json
//
// Included unconditionally from config.h after core/wifi/wifi.h so
// `server` and `sendJson` are in scope.
// ─────────────────────────────────────────────────────────────────

#include <LittleFS.h>

#define STATS_SAVE_INTERVAL_MS 300000UL   // save every 5 minutes

// ── Persisted counters (accumulated since last reset) ─────────────
static double _statsRunMs   = 0.0;
static double _statsPanDeg  = 0.0;
static double _statsTiltDeg = 0.0;
static double _statsLedMs   = 0.0;
static double _statsLedRMs  = 0.0;
static double _statsLedGMs  = 0.0;
static double _statsLedBMs  = 0.0;
static double _statsLedWMs  = 0.0;

// ── Previous servo positions for delta computation ────────────────
static float _statsPrevPan  = 135.0f;
static float _statsPrevTilt = 135.0f;

// ── Persistence helpers ───────────────────────────────────────────

static void stats_load() {
  JsonDocument doc;
  if (!storage_readJson("/stats.json", doc)) return;
  _statsRunMs   = doc["run_ms"]   | 0.0;
  _statsPanDeg  = doc["pan_deg"]  | 0.0;
  _statsTiltDeg = doc["tilt_deg"] | 0.0;
  _statsLedMs   = doc["led_ms"]   | 0.0;
  _statsLedRMs  = doc["led_r_ms"] | 0.0;
  _statsLedGMs  = doc["led_g_ms"] | 0.0;
  _statsLedBMs  = doc["led_b_ms"] | 0.0;
  _statsLedWMs  = doc["led_w_ms"] | 0.0;
}

static void stats_save() {
  JsonDocument doc;
  doc["run_ms"]   = _statsRunMs;
  doc["pan_deg"]  = _statsPanDeg;
  doc["tilt_deg"] = _statsTiltDeg;
  doc["led_ms"]   = _statsLedMs;
  doc["led_r_ms"] = _statsLedRMs;
  doc["led_g_ms"] = _statsLedGMs;
  doc["led_b_ms"] = _statsLedBMs;
  doc["led_w_ms"] = _statsLedWMs;
  storage_writeJson("/stats.json", doc);
}

// ── HTTP handlers ─────────────────────────────────────────────────

static void handleGetStats(AsyncWebServerRequest* req) {
  char json[512];
  snprintf(json, sizeof(json),
    "{"
      "\"run_hours\":%.4f,"
      "\"pan_rotations\":%.4f,"
      "\"tilt_rotations\":%.4f,"
      "\"led_on_hours\":%.4f,"
      "\"led_r_hours\":%.4f,"
      "\"led_g_hours\":%.4f,"
      "\"led_b_hours\":%.4f,"
      "\"led_w_hours\":%.4f"
    "}",
    _statsRunMs   / 3600000.0,
    _statsPanDeg  / 540.0,
    _statsTiltDeg / 540.0,
    _statsLedMs   / 3600000.0,
    _statsLedRMs  / 3600000.0,
    _statsLedGMs  / 3600000.0,
    _statsLedBMs  / 3600000.0,
    _statsLedWMs  / 3600000.0
  );
  sendJson(req, 200, String(json));
}

static void handleDeleteStats(AsyncWebServerRequest* req) {
  String confirm = req->hasParam("confirm") ? req->getParam("confirm")->value() : "";
  if (confirm != "DELETE") {
    sendJson(req, 200, "{\"status\":\"confirm\",\"message\":\"Send DELETE /api/stats?confirm=DELETE to reset all counters\"}");
    return;
  }
  _statsRunMs = _statsPanDeg = _statsTiltDeg = 0.0;
  _statsLedMs = _statsLedRMs = _statsLedGMs = _statsLedBMs = _statsLedWMs = 0.0;
  LittleFS.remove("/stats.json");
  sendJson(req, 200, "{\"status\":\"ok\",\"message\":\"Stats reset\"}");
}

// ── Lifecycle ─────────────────────────────────────────────────────

void stats_setup() {
  stats_load();
  _statsPrevPan  = _curPanF;
  _statsPrevTilt = _curTiltF;

  server.on("/api/stats", HTTP_GET,    handleGetStats);
  server.on("/api/stats", HTTP_DELETE, handleDeleteStats);

  Serial.println("[Stats] Ready — GET /api/stats");
}

void stats_loop() {
  static unsigned long _lastMs   = 0;
  static unsigned long _lastSave = 0;

  unsigned long now = millis();

  // First call: initialise without counting the boot time as run time
  if (_lastMs == 0) { _lastMs = now; _lastSave = now; return; }

  unsigned long dt = now - _lastMs;
  _lastMs = now;
  if (dt > 1000) dt = 1000;  // clamp: ignore time lost to blocking calls

  _statsRunMs += (double)dt;

  // Servo rotation — accumulate absolute angular travel; 540° = 1 rotation
  float panDelta  = fabsf(_curPanF  - _statsPrevPan);
  float tiltDelta = fabsf(_curTiltF - _statsPrevTilt);
  _statsPanDeg  += panDelta;
  _statsTiltDeg += tiltDelta;
  _statsPrevPan  = _curPanF;
  _statsPrevTilt = _curTiltF;

  // LED-on time per channel
  if (curR > 0 || curG > 0 || curB > 0 || curW > 0) _statsLedMs  += (double)dt;
  if (curR > 0) _statsLedRMs += (double)dt;
  if (curG > 0) _statsLedGMs += (double)dt;
  if (curB > 0) _statsLedBMs += (double)dt;
  if (curW > 0) _statsLedWMs += (double)dt;

  if (now - _lastSave >= STATS_SAVE_INTERVAL_MS) {
    _lastSave = now;
    stats_save();
  }
}

REGISTER_PLUGIN(stats)
