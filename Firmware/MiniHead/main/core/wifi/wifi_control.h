#pragma once

// ── WiFi Control Plugin ───────────────────────────────────────────
// Coordinates the HTTP server (ESPAsyncWebServer), route registration,
// and server lifecycle (start/stop/promote/follower).
//
// Sub-modules (included below in dependency order):
//   wifi_helpers.h  — sendJson, requireLeader, _jsonEscapeStr, etc.
//   wifi_api.h      — general API handlers (send, rainbow, status…)
//   wifi_cues.h     — cue storage, sequencer, cue/seq handlers
//   wifi_heads.h    — heads/fixtures/identify, async config forward
//
// Only active as LEADER (started by discovery.h).
// Art-Net routes are registered by the artnet plugin.
// UDP stubs are overridden by udp_control plugin.
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "html_page.h"
#include "theme.h"
#include "core_globals.h"
#include "log_config.h"
#include "discovery_globals.h"
#include "../../storage.h"

// ── UDP forward declarations ──────────────────────────────────────
// Implemented by plugins/udp_control/udp_commands.h.
void udp_sendCommand(const char* ip, const char* mac, const char* cmd);
void udp_broadcastCommand(const char* cmd);
void udp_sendIdentifyOn(const char* ip, const char* mac);
void udp_sendIdentifyOff(const char* ip, const char* mac);

// ── ArtNet forward declarations ───────────────────────────────────
#ifdef PLUGIN_ARTNET
#include "../../plugins/artnet/artnet_globals.h"
void artnet_upsertPatch(uint16_t universe, uint16_t startAddr);
#endif

// ── Server and control-state declarations ─────────────────────────
// These must appear before the sub-module includes so that all
// handlers can reference server, _serverActive, wifiAPMode, etc.
AsyncWebServer server(80);
static bool _serverStarted = false;  // server.begin() must only be called once
static bool _serverActive  = false;  // true only while this node is LEADER
bool wifiAPMode    = false;           // true when running as standalone hotspot
bool apPasswordSet = false;           // true when AP has a password ≥8 chars

// ── Sub-modules ───────────────────────────────────────────────────
#include "wifi_helpers.h"   // sendJson, requireLeader, _jsonEscapeStr, _wifiIP
#include "wifi_api.h"       // handleRoot/Status/Send/Rainbow/Blackout/APPassword…
#include "wifi_cues.h"      // Cue struct, storage, cue+seq handlers, wifi_cues_loop
#include "wifi_heads.h"     // handleGetHeads/Fixtures, handleSetName/FixID (async), handleIdentify
#include "wifi_networks.h"  // saved network CRUD + reboot (Saved Networks panel)

// ── Config routes (always active — no requireLeader() guard) ─────
// Called by both setupRoutes() and wifi_control_setup() in ARTNET mode.
void setupConfigRoutes() {
  server.on("/api/config/fixid", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      int id = doc["fixID"] | 0;
      if (id > 0) { discovery_saveFixID(id); Serial.printf("[Config] FixID set to %d\n", id); }
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });

  server.on("/api/config/name", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      const char* name = doc["name"] | "";
      discovery_saveName(name);
      Serial.printf("[Config] Name set to \"%s\"\n", ownName);
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });

#ifdef PLUGIN_ARTNET
  server.on("/api/config/patch", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      int uni  = doc["universe"]  | 0;
      int addr = doc["startAddr"] | 1;
      artnet_upsertPatch((uint16_t)uni, (uint16_t)addr);
      Serial.printf("[Config] Patch: U%d @%d\n", uni, addr);
      sendJson(req, 200, "{\"status\":\"ok\"}");
    });
#endif

  // Saved WiFi networks — Saved Networks panel (add/remove without reflashing)
  server.on("/api/wifi/networks", HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetWifiNetworks(r); });
  server.on("/api/wifi/networks", HTTP_POST,
    [](AsyncWebServerRequest* r){ handleAddWifiNetwork(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/wifi/networks", HTTP_DELETE,
    [](AsyncWebServerRequest* r){ handleDeleteWifiNetwork(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r){ handleReboot(r); });
}

// ── Route setup ───────────────────────────────────────────────────
void setupRoutes() {
  setupConfigRoutes();

  // CORS preflight
  server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200);
    r->addHeader("Access-Control-Allow-Origin",  "*");
    r->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    r->addHeader("Access-Control-Allow-Headers", "Content-Type");
    req->send(r);
  });

  // Theme CSS
  server.on("/theme", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/css", THEME_CSS);
    r->addHeader("Access-Control-Allow-Origin", "*");
    req->send(r);
  });

  // Root + general API
  server.on("/",                        HTTP_GET,  [](AsyncWebServerRequest* r){ handleRoot(r); });
  server.on("/api/status",    HTTP_GET,  [](AsyncWebServerRequest* r){ handleStatus(r); });
  server.on("/api/version",   HTTP_GET,  [](AsyncWebServerRequest* r){ handleVersion(r); });
  server.on("/api/ports",     HTTP_GET,  [](AsyncWebServerRequest* r){ handlePorts(r); });
  server.on("/api/connect",   HTTP_POST, [](AsyncWebServerRequest* r){ handleConnect(r); });
  server.on("/api/disconnect",HTTP_POST, [](AsyncWebServerRequest* r){ handleDisconnect(r); });
  server.on("/api/send",      HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSend(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/rainbow",         HTTP_POST,
    [](AsyncWebServerRequest* r){ handleRainbow(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/demo",            HTTP_POST,
    [](AsyncWebServerRequest* r){ handleDemo(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/animation/speed", HTTP_POST,
    [](AsyncWebServerRequest* r){ handleAnimSpeed(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/blackout",        HTTP_POST,
    [](AsyncWebServerRequest* r){ handleBlackout(r); });
  server.on("/api/ap/password",     HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSetAPPassword(r); },
    nullptr, _bodyAccumulator);

  // Cues — /reorder must be registered before the wildcard
  server.on("/api/cues",         HTTP_GET,  [](AsyncWebServerRequest* r){ handleGetCues(r); });
  server.on("/api/cues",         HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSaveCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/reorder", HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleReorderCues(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_POST,
    [](AsyncWebServerRequest* r){ handleFireCue(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_PUT,
    [](AsyncWebServerRequest* r){ handleUpdateCueTargets(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/cues/*",       HTTP_DELETE, [](AsyncWebServerRequest* r){ handleDeleteCue(r); });

  // Sequencer
  server.on("/api/sequencer/start",  HTTP_POST,
    [](AsyncWebServerRequest* r){ handleSeqStart(r); },
    nullptr, _bodyAccumulator);
  server.on("/api/sequencer/stop",   HTTP_POST, [](AsyncWebServerRequest* r){ handleSeqStop(r); });
  server.on("/api/sequencer/status", HTTP_GET,  [](AsyncWebServerRequest* r){ handleSeqStatus(r); });

  // Heads & fixtures
  server.on("/api/heads",    HTTP_GET, [](AsyncWebServerRequest* r){ handleGetHeads(r); });
  server.on("/api/fixtures", HTTP_GET, [](AsyncWebServerRequest* r){ handleGetFixtures(r); });
  server.on("/api/heads/*",  HTTP_POST,
    [](AsyncWebServerRequest* r){
      String path = r->url();
      if      (path.endsWith("/identify")) handleIdentify(r);
      else if (path.endsWith("/fixid"))    handleSetFixID(r);
      else if (path.endsWith("/name"))     handleSetName(r);
      else r->send(404, "text/plain", "Not found");
    },
    nullptr, _bodyAccumulator);

  // Captive portal suppression (iOS / Android / Windows probes)
  auto _ok = [](AsyncWebServerRequest* r){
    r->send(200, "text/html",
      "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); };
  server.on("/hotspot-detect.html",       HTTP_GET, _ok);
  server.on("/library/test/success.html", HTTP_GET, _ok);
  server.on("/generate_204",    HTTP_GET, [](AsyncWebServerRequest* r){ r->send(204); });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* r){ r->send(200,"text/plain","Microsoft Connect Test"); });
  server.on("/redirect",        HTTP_GET, _ok);
  server.on("/canonical.html",  HTTP_GET, _ok);
  server.on("/success.txt",     HTTP_GET, _ok);

  server.onNotFound([](AsyncWebServerRequest* r){
    r->send(404, "text/plain", "Not found");
  });
}

// ── Lifecycle ─────────────────────────────────────────────────────

void wifi_control_setup() {
#ifdef PLUGIN_ARTNET
  _serverActive = true;
  logcfg_load();
  setupRoutes();
  if (!_serverStarted) { server.begin(); _serverStarted = true; }
  Serial.println("[WiFi] Art-Net mode — full HTTP server :80");
  return;
#endif
  _serverActive = true;
  logcfg_load();
  loadCuesFromFlash();
  Serial.println("[WiFi] IP: " + _wifiIP());
  setupRoutes();
  if (!_serverStarted) {
    server.begin();
    _serverStarted = true;
    Serial.println("[WiFi] Async server started");
  } else {
    Serial.println("[WiFi] Async server resumed (already started)");
  }
}

void wifi_control_stop() {
  _serverActive = false;
  seqRunning = false; seqIndex = 0; seqIdCount = 0;
  // AsyncWebServer has no stop() — socket stays bound.
  // requireLeader() redirects browsers to the actual leader.
  Serial.println("[WiFi] Follower mode — HTTP requests will redirect to leader");
}

void wifi_control_setup_follower() {
  _serverActive = false;
  logcfg_load();
  setupRoutes();
  if (!_serverStarted) {
    server.begin();
    _serverStarted = true;
    Serial.println("[WiFi] Async server started (follower)");
  }
}

void wifi_control_promote() {
#ifdef PLUGIN_ARTNET
  return;  // Art-Net mode: PC App is always leader
#endif
  _serverActive = true;
  logcfg_load();
  loadCuesFromFlash();
  Serial.println("[WiFi] Promoted to LEADER — APIs active");
}

void wifi_control_loop() {
  if (_wifiRebootAt && millis() > _wifiRebootAt) {
    Serial.println("[WiFi] Rebooting (requested via web UI)...");
    ESP.restart();
  }
  if (!_serverActive) return;
  wifi_cues_loop();
}
// Note: wifi_control is NOT registered as a plugin — it is started
// and stopped by discovery.h when the node wins/loses election.
