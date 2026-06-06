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

//#define PLUGIN_STARTUP_ANIMATION  // servo calibration sweep + color test on boot
#define PLUGIN_UDP_CONTROL
#define PLUGIN_ARTNET
//#define PLUGIN_DEBUGGER

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

// ── AP hotspot password ───────────────────────────────────────────
// Used when no known WiFi is reachable and the ESP starts its own hotspot.
// Must be at least 8 characters. Use "" for an open (no-password) network.
#define AP_PASSWORD "minihead"

// ── BLE Provisioning ──────────────────────────────────────────────
// Devices with an empty WIFI_NETWORKS[] start as SEEKER: they scan via
// Bluetooth and receive encrypted credentials from a SENDER (a device
// already on WiFi). After receiving, they store to /wifi_provision.json
// and reboot.
//
// PROVISION_KEY: 32 hex chars = 16-byte AES-128 key.
// All devices in the same fleet must share the same key.
// Change this before flashing — do not leave it at the placeholder below.
#define PROVISION_KEY "00000000000000000000000000000000"

// Uncomment to enable BLE credential provisioning:
//#define PLUGIN_BLE_PROVISION

#ifdef PLUGIN_BLE_PROVISION
#include "plugins/ble_provision/ble_provision.h"
#endif

// ── Plugins ───────────────────────────────────────────────────────
// NOTE: startup_animation must come first (runs before WiFi is up).
//       wifi must come after startup_animation.

#ifdef PLUGIN_STARTUP_ANIMATION
#include "plugins/startup_animation/startup_animation.h"
#endif
#include "plugins/wifi/wifi.h"                    // HTTP server, cues, sequencer

#ifdef PLUGIN_UDP_CONTROL
#include "plugins/udp_control/udp_control.h"      // discovery + leader election + UDP commands
#endif

#ifdef PLUGIN_ARTNET
#include "plugins/artnet/artnet.h"                // Art-Net / DMX512 receiver — port 6454
#endif

#ifdef PLUGIN_DEBUGGER
#include "plugins/debugger/debugger.h"            // log config UI + loop timing profiler
#endif

// ── Stubs ─────────────────────────────────────────────────────────
// When a plugin is disabled its symbols must still resolve at link time.

#ifndef PLUGIN_UDP_CONTROL
#include "plugins/wifi/discovery_stubs.h"
#endif
