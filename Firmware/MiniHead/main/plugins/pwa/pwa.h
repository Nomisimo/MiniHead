#pragma once

// ── PWA Plugin ────────────────────────────────────────────────────
// Serves the MiniHead PWA from LittleFS so the app is available on
// any phone that connects to the ESP's WiFi or AP — no PC server
// needed and no cache eviction possible.
//
// Files must be in /pwa/** on LittleFS.
// Run deploy.sh in the MiniHead App repo to populate them, then
// use Arduino IDE Tools → ESP32 Sketch Data Upload to flash.
//
// Route strategy:
//   /sw.js, /manifest.json  → served with Cache-Control: no-cache
//   /css/**, /js/**, /icons/** → serveStatic, max-age=86400
//   /         → handled by handleRoot() in wifi_api.h (checks PLUGIN_PWA)
//   any other non-/api/ path  → index.html (SPA fallback)
//
// Must come AFTER wifi in config.h so all /api/* routes are already
// registered before serveStatic and onNotFound are overridden here.
// ─────────────────────────────────────────────────────────────────

#include "../../plugin_registry.h"
#include <LittleFS.h>

static void _pwaNocache(AsyncWebServerRequest* req, const char* fsPath, const char* mime) {
  if (!LittleFS.exists(fsPath)) { req->send(404, "text/plain", "Not found"); return; }
  AsyncWebServerResponse* res = req->beginResponse(LittleFS, fsPath, mime);
  res->addHeader("Cache-Control", "no-cache");
  req->send(res);
}

void pwa_setup() {
  // SW and manifest must never be cached by the HTTP layer
  // (the SW itself handles cache versioning)
  server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* r) {
    _pwaNocache(r, "/pwa/sw.js", "application/javascript");
  });
  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* r) {
    _pwaNocache(r, "/pwa/manifest.json", "application/json");
  });

  // Static app shell — long cache; SW handles versioning via content hash
  server.serveStatic("/css/",   LittleFS, "/pwa/css/")  .setCacheControl("max-age=86400");
  server.serveStatic("/js/",    LittleFS, "/pwa/js/")   .setCacheControl("max-age=86400");
  server.serveStatic("/icons/", LittleFS, "/pwa/icons/").setCacheControl("max-age=86400");

  // SPA fallback: any unmatched non-/api/ path gets index.html so the
  // JS router can handle client-side navigation after a hard reload.
  server.onNotFound([](AsyncWebServerRequest* req) {
    if (!req->url().startsWith("/api/") && LittleFS.exists("/pwa/index.html")) {
      AsyncWebServerResponse* res = req->beginResponse(LittleFS, "/pwa/index.html", "text/html");
      res->addHeader("Cache-Control", "no-cache");
      req->send(res);
      return;
    }
    req->send(404, "text/plain", "Not found");
  });

  Serial.println("[PWA] Serving app from LittleFS /pwa/");
}

void pwa_loop() {}

REGISTER_PLUGIN(pwa);
