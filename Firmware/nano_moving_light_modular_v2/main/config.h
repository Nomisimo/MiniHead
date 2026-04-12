// ── Plugin Config ─────────────────────────────────────────────────
// This is the only file you need to edit to add/remove plugins.
// main.ino iterates _plugins[] — it never needs to be changed.
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"

// ── WiFi credentials ──────────────────────────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ── Plugins ───────────────────────────────────────────────────────
// To disable a plugin: comment out its #include line.
// To add a plugin:     add #include "plugins/<name>/<name>.h"
//
// Each plugin auto-registers its setup()/loop() via REGISTER_PLUGIN.
// Plugins run in the order they appear here.
//
// NOTE: startup_animation must come first (runs before WiFi is up).
//       wifi must come after startup_animation.

#include "plugins/startup_animation/startup_animation.h"
#include "plugins/wifi/wifi.h"
