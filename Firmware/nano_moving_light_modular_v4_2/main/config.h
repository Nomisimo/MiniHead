// ── Plugin Config ─────────────────────────────────────────────────
// This is the only file you need to edit to add/remove plugins.
// main.ino iterates _plugins[] — it never needs to be changed.
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"

// ── WiFi network list ─────────────────────────────────────────────
// Add all known networks. The ESP tries the last-connected first,
// then scans for any visible network from this list.
// Last-connected SSID is persisted in /wifi_last.json on LittleFS.
struct WifiCredential { const char* ssid; const char* password; };
static const WifiCredential WIFI_NETWORKS[] = {
  { "DastutWlan", "4BYvsiFnDGz-!czw9iWP*R6*nB82pB" },
  // { "AnotherNetwork", "AnotherPassword" },
};
static const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// ── Plugins ───────────────────────────────────────────────────────
// To disable a plugin: comment out its #include line.
// To add a plugin:     add #include "plugins/<name>/<name>.h"
//
// Each plugin auto-registers its setup()/loop() via REGISTER_PLUGIN.
// Plugins run in the order they appear here.
//
// NOTE: startup_animation must come first (runs before WiFi is up).
//       wifi must come after startup_animation.

//#include "plugins/startup_animation/startup_animation.h"
#include "plugins/wifi/wifi.h"
#include "plugins/artnet/artnet.h"
