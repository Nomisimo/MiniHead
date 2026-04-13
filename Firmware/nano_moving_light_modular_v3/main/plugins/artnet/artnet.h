#pragma once

// ── Art-Net Plugin ────────────────────────────────────────────────
// Entry point for the Art-Net / DMX512 plugin.
// Registers artnet_setup() and artnet_loop() via REGISTER_PLUGIN.
//
// This plugin runs on EVERY node (both leader and follower):
//   - Every ESP listens on port 6454 for Art-Net packets
//   - Each ESP applies channels that match its own patched fixID
//   - The leader additionally fans out data to followers via UDP CMD
//
// HTTP routes and NVS storage are handled by artnet_control.h,
// which is included from wifi_control.h (shares the HTTP server).
//
// Add to config.h:
//   #include "plugins/artnet/artnet.h"
// (after plugins/wifi/wifi.h)
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "artnet_globals.h"
#include "artnet_receiver.h"

void artnet_setup() {
  artnet_receiver_setup();
  // artnet_control_setup() (NVS patch load) is called from wifi_control_setup()
  // so patches are available before the first Art-Net packet arrives.
}

void artnet_loop() {
  artnet_receiver_loop();
}

REGISTER_PLUGIN(artnet);
