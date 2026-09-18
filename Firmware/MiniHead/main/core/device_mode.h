#pragma once

// ── Network Mode (runtime-selectable) ──────────────────────────────
// Both UDP-control (standalone fleet) and Art-Net (DMX receiver) code
// are always compiled in now — only one mode's leader-election / HTTP
// role / DMX-receive logic actually RUNS at a time. Running both
// simultaneously (own webserver + follower command forwarding +
// Art-Net receive + local changes, all at once) is what overloads the
// ESP32-C3's single core and makes it overheat — this used to be
// avoided by only ever compiling one mode in at all.
//
// Persisted at /network_mode.json: {"mode":"udp"|"artnet"}
// Changing it takes effect after a reboot (POST /api/mode, then
// POST /api/reboot — see core/wifi/device_mode_routes.h).
//
// Must be included before core/wifi/wifi.h and core/udp/udp_control.h
// so their behavioral forks can reference `networkMode`.
// ─────────────────────────────────────────────────────────────────

enum NetworkMode { MODE_UDP = 0, MODE_ARTNET = 1 };

NetworkMode networkMode = MODE_UDP;

void deviceMode_load() {
  JsonDocument doc;
  if (!storage_readJson("/network_mode.json", doc)) {
    Serial.println("[Mode] No saved network mode — defaulting to UDP");
    return;
  }
  const char* m = doc["mode"] | "udp";
  networkMode = (strcmp(m, "artnet") == 0) ? MODE_ARTNET : MODE_UDP;
  Serial.printf("[Mode] Loaded network mode: %s\n", networkMode == MODE_ARTNET ? "artnet" : "udp");
}

void deviceMode_save(NetworkMode mode) {
  JsonDocument doc;
  doc["mode"] = (mode == MODE_ARTNET) ? "artnet" : "udp";
  storage_writeJson("/network_mode.json", doc);
}
