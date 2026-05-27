// ── Plugin Config (v4.2) ──────────────────────────────────────────
// Copy this file to config.h and fill in your credentials.
// config.h is gitignored — never commit it.
//
// Persistent data (cues, patches, fixID, name) is stored as JSON
// on LittleFS — not NVS. Flash with partition scheme:
//   "Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)"
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Feature flags ─────────────────────────────────────────────────
// To disable a plugin: comment out its #define — the #include and
// stubs are handled automatically by the #ifdef blocks below.
// MUST be defined BEFORE the plugin #includes so that wifi_control.h
// and discovery.h can read them at compile time.

#define PLUGIN_UDP_CONTROL
#define PLUGIN_ARTNET
//#define PLUGIN_PROFILER

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"

// ── WiFi network list ─────────────────────────────────────────────
// Add all known networks. The ESP tries the last-connected first,
// then scans for any visible network from this list.
// Last-connected SSID is persisted in /wifi_last.json on LittleFS.
struct WifiCredential {
  const char* ssid;
  const char* password;
};
static const WifiCredential WIFI_NETWORKS[] = {
  { "YourPrimaryNetwork",  "YourPassword"   },
  // { "BackupNetwork",    "BackupPassword" },
};
static const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// ── Plugins ───────────────────────────────────────────────────────
// NOTE: startup_animation must come first (runs before WiFi is up).
//       wifi must come after startup_animation.

//#include "plugins/startup_animation/startup_animation.h"
#include "plugins/wifi/wifi.h"                    // HTTP server, cues, sequencer

#ifdef PLUGIN_UDP_CONTROL
#include "plugins/udp_control/udp_control.h"      // discovery + leader election + UDP commands
#endif

#ifdef PLUGIN_ARTNET
#include "plugins/artnet/artnet.h"                // Art-Net / DMX512 receiver — port 6454
#endif

#ifdef PLUGIN_PROFILER
#include "plugins/profiler/profiler.h"            // loop timing profiler (debug only)
#endif

// ── Stubs ─────────────────────────────────────────────────────────
// When a plugin is disabled its symbols must still resolve at link time.

#ifndef PLUGIN_UDP_CONTROL
#include "plugins/wifi/discovery_stubs.h"
#endif

#ifndef PLUGIN_ARTNET
#include "plugins/artnet/artnet_stubs.h"
#endif
