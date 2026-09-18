#pragma once

// ── Network Mode HTTP handlers ─────────────────────────────────────
// GET/POST /api/mode — read/switch the runtime Network Mode (see
// core/device_mode.h for what this actually gates). Switching mode
// schedules a reboot immediately (via the same deferred-restart flag
// wifi_networks.h's /api/reboot uses) since there's only one value to
// change — no need for a separate "apply" step like Saved Networks.
// ─────────────────────────────────────────────────────────────────

void handleGetMode(AsyncWebServerRequest* req) {
  sendJson(req, 200, String("{\"mode\":\"") + (networkMode == MODE_ARTNET ? "artnet" : "udp") + "\"}");
}

void handleSetMode(AsyncWebServerRequest* req) {
  String body = _getBody(req);  // must be called before any early return — frees _tempObject
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String mode = doc["mode"] | "";
  if (mode != "udp" && mode != "artnet") {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"mode must be 'udp' or 'artnet'\"}"); return;
  }
  NetworkMode newMode = (mode == "artnet") ? MODE_ARTNET : MODE_UDP;
  deviceMode_save(newMode);
  Serial.printf("[Mode] Network mode set to %s — rebooting to apply\n", mode.c_str());
  sendJson(req, 200, "{\"status\":\"ok\"}");
  _wifiRebootAt = millis() + 300;  // let the async response flush before restarting
}
