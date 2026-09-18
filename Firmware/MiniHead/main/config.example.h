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
// To disable PLUGIN_STARTUP_ANIMATION/PLUGIN_DEBUGGER: comment out its
// #define — the #include is handled automatically by the #ifdef
// blocks below. MUST be defined BEFORE the plugin #includes so those
// files can read them at compile time.
//
// PLUGIN_UDP_CONTROL and PLUGIN_ARTNET are NOT toggled here anymore —
// both are always compiled in, and which one is ACTIVE is chosen at
// runtime via the Network Mode panel in the web UI (persisted to
// /network_mode.json, applied on reboot). Running both at once used
// to overload the ESP32-C3's single core; now only one mode's logic
// actually runs, the other's code just sits inert in flash.
#define PLUGIN_UDP_CONTROL
#define PLUGIN_ARTNET
//#define PLUGIN_STARTUP_ANIMATION  // servo calibration sweep + color test on boot
//#define PLUGIN_DEBUGGER

// ── Core (always included — hardware drivers, not a plugin) ───────
#include "core.h"
#include "core/device_mode.h"      // runtime Network Mode (UDP vs Art-Net) — must load before wifi.h

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

// ── WiFi watchdog ─────────────────────────────────────────────────
// Interval (ms) between connectivity checks in loop().
// After WIFI_WATCHDOG_MISSES consecutive disconnected checks a full
// wifi_connectMulti() is triggered (blocking). Increase MISSES to tolerate
// brief drops without reconnecting.
#ifndef WIFI_WATCHDOG_INTERVAL_MS
#define WIFI_WATCHDOG_INTERVAL_MS 15000
#endif
#ifndef WIFI_WATCHDOG_MISSES
#define WIFI_WATCHDOG_MISSES 3
#endif

// ── Saved WiFi networks (added via the web UI) ────────────────────
// Networks added through the device's own web UI (Saved Networks
// panel) are stored as JSON on LittleFS (/wifi_networks.json), not
// here — this only caps how many can be saved at once.
#ifndef MAX_SAVED_NETWORKS
#define MAX_SAVED_NETWORKS 10
#endif

// ── Cue / sequencer limits ────────────────────────────────────────
#ifndef MAX_CUES
#define MAX_CUES    32
#endif
#ifndef MAX_TARGETS
#define MAX_TARGETS 16
#endif

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
#include "core/wifi/wifi.h"               // HTTP server, cues, sequencer
#include "core/udp/udp_control.h"         // discovery + leader election + UDP commands (active only in UDP mode)
#include "plugins/artnet/artnet.h"        // Art-Net / DMX512 receiver — port 6454 (active only in Art-Net mode)

#ifdef PLUGIN_DEBUGGER
#include "plugins/debugger/debugger.h"            // log config UI + loop timing profiler
#endif
