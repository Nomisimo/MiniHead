#pragma once

// ── WiFi Plugin ───────────────────────────────────────────────────
// Enables the full WiFi subsystem:
//   • UDP beacon discovery + leader election     (discovery.h)
//   • HTTP control server (runs on leader only)  (wifi_control.h)
//   • UDP command receiver/sender on every node  (udp_control.h)
//
// To disable the entire wifi subsystem, comment out:
//   #include "plugins/wifi/wifi.h"   in config.h
//
// Include order below is critical — do not reorder.
// ─────────────────────────────────────────────────────────────────

#include "discovery_globals.h"   // shared types & extern declarations
#include "wifi_control.h"        // HTTP server — also pulls in udp_control.h
                                 //   udp_control auto-registers itself here
#include "discovery.h"           // beacon + election — auto-registers itself
