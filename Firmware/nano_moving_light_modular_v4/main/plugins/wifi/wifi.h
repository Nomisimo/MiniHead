#pragma once

// ── WiFi Plugin (v4) ──────────────────────────────────────────────
// Boot sequence:
//   1. wifi_manager: connect to known WiFi or start captive-portal AP
//   2. (if connected) discovery: listen 4s for PC-leader beacon
//   3. → ROLE_STANDALONE (own server) or ROLE_FOLLOWER (UDP receiver)
//
// Modes:
//   ROLE_AP_PORTAL  — no WiFi found → captive portal on 192.168.4.1
//   ROLE_STANDALONE — WiFi ok, no PC leader → own HTTP server at ESP IP
//   ROLE_FOLLOWER   — PC leader alive → receive UDP commands
//
// Include order below is critical — do not reorder.
// ─────────────────────────────────────────────────────────────────

#include "discovery_globals.h"   // shared types, extern declarations
#include "wifi_manager.h"        // WiFi connect + AP captive portal (NEW v4)
#include "wifi_control.h"        // HTTP server — also pulls in udp_control.h
#include "discovery.h"           // beacon + mode election

void wifi_setup() {
  wifi_manager_setup();            // connect WiFi or start AP_PORTAL
  if (nodeRole == ROLE_AP_PORTAL) return;  // no discovery in AP mode
  discovery_setup();               // listen for PC leader → STANDALONE or FOLLOWER
}

void wifi_loop() {
  if (nodeRole == ROLE_AP_PORTAL) {
    wifi_manager_loop();           // serve captive portal only
    return;
  }
  discovery_loop();                // mode switching, web server, UDP receiver
}

REGISTER_PLUGIN(wifi);
