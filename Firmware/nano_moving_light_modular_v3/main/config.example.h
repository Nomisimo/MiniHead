// ── Plugin Config ─────────────────────────────────────────────────
// Copy this file to config.h and fill in your credentials.
// config.h is gitignored — never commit it.
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"

// ── WiFi credentials — CHANGE THESE ──────────────────────────────
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"

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
#include "plugins/artnet/artnet.h"    // Art-Net / DMX512 receiver — port 6454, runs on every node
