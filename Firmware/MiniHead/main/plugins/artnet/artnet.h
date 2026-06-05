#pragma once

// Signals to udp_commands.h that the real artnet_upsertPatch is provided here.
#define PLUGIN_ARTNET

// ── Art-Net Plugin ────────────────────────────────────────────────
// Entry point for the Art-Net / DMX512 plugin.
// Registers artnet_setup() and artnet_loop() via REGISTER_PLUGIN.
//
// Owns:
//   • Art-Net UDP receiver on port 6454    (artnet_receiver.h)
//   • HTTP patch management API            (artnet_control.h)
//   • Art-Net panel HTML                   (artnet_panel_html.h)
//
// Must come after plugins/wifi/wifi.h (and udp_control if used) in config.h.
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include "artnet_globals.h"
#include "artnet_receiver.h"
#include "artnet_control.h"
#include "artnet_panel_html.h"

void artnet_setup() {
  artnet_receiver_setup();
  artnet_control_setup();

  // Panel HTML
  server.on("/plugins/artnet/panel.html", HTTP_GET,
    [](AsyncWebServerRequest* r) { sendHtmlProgmem(r, ARTNET_PANEL_HTML, true); });

  // Art-Net API routes
  server.on("/api/artnet/status",     HTTP_GET,    [](AsyncWebServerRequest* r){ handleArtnetStatus(r); });
  server.on("/api/artnet/patch",      HTTP_GET,    [](AsyncWebServerRequest* r){ handleGetArtnetPatch(r); });
  server.on("/api/artnet/patch",      HTTP_DELETE, [](AsyncWebServerRequest* r){ handleClearAllArtnetPatches(r); });
  server.on("/api/artnet/patch/bulk", HTTP_POST,
    [](AsyncWebServerRequest* r){ handleBulkArtnetPatch(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/artnet/patch",      HTTP_POST,
    [](AsyncWebServerRequest* r){ handlePostArtnetPatch(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/artnet/patch/*",    HTTP_DELETE, [](AsyncWebServerRequest* r){
    String path = r->url();
    if (path.startsWith("/api/artnet/patch/universe/")) handleClearUniverseArtnetPatches(r);
    else                                                handleDeleteArtnetPatch(r);
  });
  server.on("/api/artnet/patch/*",    HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleUpdateArtnetPatch(r); },
    nullptr, _bodyAccumulator);
}

void artnet_loop() {
  artnet_receiver_loop();
}

REGISTER_PLUGIN(artnet);
