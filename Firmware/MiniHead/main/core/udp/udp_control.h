#pragma once

// Signals to wifi_control.h that real UDP/discovery symbols are provided here.
#define PLUGIN_UDP_CONTROL

// ── UDP Control Plugin ────────────────────────────────────────────
// Groups discovery beacon/election and UDP command send/receive.
//
// Provides:
//   • UDP beacon + leader election           (discovery.h)
//   • UDP command sender/receiver            (udp_commands.h)
//   • Discovery panel HTML route             (discovery_panel_html.h)
//
// Disable by commenting out in config.h:
//   #include "plugins/udp_control/udp_control.h"
//
// Must come after plugins/wifi/wifi.h in config.h.
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "../wifi/discovery_globals.h"
#include "discovery.h"
#include "udp_commands.h"
#include "discovery_panel_html.h"

void udp_control_setup() {
  discovery_setup();
  _udp_cmds_setup();
  server.on("/plugins/wifi/discovery_panel.html", HTTP_GET,
    [](AsyncWebServerRequest* r) { sendHtmlProgmem(r, DISCOVERY_PANEL_HTML); });
}

void udp_control_loop() {
  discovery_loop();
  _udp_cmds_loop();
}

REGISTER_PLUGIN(udp_control);
