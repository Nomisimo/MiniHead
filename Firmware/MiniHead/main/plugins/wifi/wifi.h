#pragma once

// ── WiFi Plugin ───────────────────────────────────────────────────
// Core HTTP server: cue sequencer, fader control, CORS, log config.
// Shared types (NodeRole, Peer, ports) in discovery_globals.h.
//
// Additional plugins (add after this in config.h):
//   plugins/udp_control/udp_control.h  — discovery + leader election + UDP
//   plugins/artnet/artnet.h            — Art-Net DMX receiver + patch API
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "discovery_globals.h"
#include "wifi_control.h"

// wifi always starts the HTTP server — udp_control then handles promote/stop
void wifi_setup() { wifi_control_setup(); }
void wifi_loop()  { wifi_control_loop(); }

REGISTER_PLUGIN(wifi);
