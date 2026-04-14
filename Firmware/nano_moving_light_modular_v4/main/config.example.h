// ── Plugin Config (v4) ────────────────────────────────────────────
// Copy this file to config.h and fill in your credentials.
// config.h is gitignored — never commit it.
//
// WiFi fallback: if connection fails, ESP starts AP "MiniHead-XXXXXX"
// with password "minihead". Open 192.168.4.1 to configure credentials.
// New credentials are saved to NVS and used automatically after reboot.
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"

// ── WiFi credentials ──────────────────────────────────────────────
// These are the FALLBACK credentials used when no saved credentials
// exist in NVS. If you have never connected to a network via the
// captive portal, the device will try these on every boot.
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"

// ── Plugins ───────────────────────────────────────────────────────
// To disable a plugin: comment out its #include line.
// To add a plugin:     add #include "plugins/<name>/<name>.h"
//
// Each plugin auto-registers its setup()/loop() via REGISTER_PLUGIN.
// Plugins run in the order they appear here.
//
// IMPORTANT ORDER:
//   startup_animation — must come first (no WiFi dependency)
//   wifi              — must come after startup_animation
//   artnet            — can come after wifi

#include "plugins/startup_animation/startup_animation.h"
#include "plugins/wifi/wifi.h"
#include "plugins/artnet/artnet.h"    // Art-Net / DMX512 receiver — port 6454
