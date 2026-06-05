#pragma once

// ── Art-Net Control ───────────────────────────────────────────────
// HTTP API routes for Art-Net patch management.
// LittleFS JSON storage for patch records (/artnet.json).
//
// Included from wifi_control.h, which owns `server`, `sendJson`,
// `_getBody`, and `_bodyAccumulator`.
//
// Each ESP stores exactly ONE patch — its own universe + startAddr.
// fixID is NOT stored on the ESP; the PC App uses fixID to route
// HTTP POSTs to the right ESP's IP.
// ─────────────────────────────────────────────────────────────────

#include "artnet_globals.h"
#include "artnet_receiver.h"

// ── Upsert helper — overwrite own patch ──────────────────────────
// artnet_savePatches() and artnet_loadPatches() live in artnet_receiver.h
// so they are available on ALL nodes (leader + follower).
void artnet_upsertPatch(uint16_t universe, uint16_t startAddr) {
  artnetPatches[0].universe  = universe;
  artnetPatches[0].startAddr = startAddr;
  artnetPatchCount = 1;
  artnet_savePatches();
}

// ── JSON serialiser ───────────────────────────────────────────────
// Include fixID in the API response for UI display (panels use it to
// match patches to fixtures in the network view). Not stored on disk.
String artnetPatchToJson(const ArtnetPatch& p) {
  return "{\"fixID\":" + String(ownFixID) +
         ",\"universe\":" + String(p.universe) +
         ",\"startAddr\":" + String(p.startAddr) + "}";
}

// ── HTTP handlers ─────────────────────────────────────────────────

void handleArtnetStatus(AsyncWebServerRequest* req) {
  // Include live channel values so the web UI can display them in the Art-Net bar
  String json = "{\"active\":";
  json += artnetActive ? "true" : "false";
  json += ",\"patchCount\":" + String(artnetPatchCount);
  json += ",\"r\":"    + String(curR);
  json += ",\"g\":"    + String(curG);
  json += ",\"b\":"    + String(curB);
  json += ",\"w\":"    + String(curW);
  json += ",\"pan\":"  + String(curPan);
  json += ",\"tilt\":" + String(curTilt);
  json += "}";
  sendJson(req, 200, json);
}

void handleGetArtnetPatch(AsyncWebServerRequest* req) {
  String json = "[";
  for (int i = 0; i < artnetPatchCount; i++) {
    if (i > 0) json += ",";
    json += artnetPatchToJson(artnetPatches[i]);
  }
  json += "]";
  sendJson(req, 200, json);
}

// POST /api/artnet/patch  {universe, startAddr}  (fixID accepted but ignored)
void handlePostArtnetPatch(AsyncWebServerRequest* req) {
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}");
    return;
  }
  int universe  = doc["universe"]  | 0;
  int startAddr = doc["startAddr"] | 1;
  if (startAddr < 1 || startAddr + DMX_FOOTPRINT - 1 > DMX_CHANNELS) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Invalid startAddr\"}");
    return;
  }
  artnet_upsertPatch((uint16_t)universe, (uint16_t)startAddr);
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// PUT /api/artnet/patch/*  — update universe / startAddr of own patch
void handleUpdateArtnetPatch(AsyncWebServerRequest* req) {
  if (artnetPatchCount == 0) {
    sendJson(req, 404, "{\"status\":\"error\",\"message\":\"No patch configured\"}"); return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return;
  }
  int universe  = doc["universe"]  | -1;
  int startAddr = doc["startAddr"] | -1;
  if (universe  >= 0) artnetPatches[0].universe  = (uint16_t)universe;
  if (startAddr >= 1 && startAddr + DMX_FOOTPRINT - 1 <= DMX_CHANNELS)
    artnetPatches[0].startAddr = (uint16_t)startAddr;
  artnet_savePatches();
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// DELETE /api/artnet/patch/*  — clear own patch
void handleDeleteArtnetPatch(AsyncWebServerRequest* req) {
  artnetPatchCount = 0;
  artnet_savePatches();
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// ── Clear all patches ─────────────────────────────────────────────
void handleClearAllArtnetPatches(AsyncWebServerRequest* req) {
  artnetPatchCount = 0;
  artnet_savePatches();
  sendJson(req, 200, "{\"status\":\"ok\",\"removed\":1}");
}

// ── Clear patches for one universe ───────────────────────────────
void handleClearUniverseArtnetPatches(AsyncWebServerRequest* req) {
  String path = req->url();
  int uni = path.substring(path.lastIndexOf('/') + 1).toInt();
  bool removed = false;
  if (artnetPatchCount > 0 && (int)artnetPatches[0].universe == uni) {
    artnetPatchCount = 0;
    artnet_savePatches();
    removed = true;
  }
  sendJson(req, 200, String("{\"status\":\"ok\",\"removed\":") + (removed ? "1" : "0") + "}");
}

// ── Bulk patch — apply own slice only ────────────────────────────
// The PC App handles pushing to other ESPs individually.
// This endpoint lets the ESP web UI set up the whole show from one
// request; the ESP stores only its own entry from the bulk range.
void handleBulkArtnetPatch(AsyncWebServerRequest* req) {
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}");
    return;
  }
  int universe   = doc["universe"]   | 0;
  int startAddr  = doc["startAddr"]  | 1;
  int count      = doc["count"]      | 0;
  int firstFixID = doc["firstFixID"] | 1;

  if (count <= 0 || firstFixID <= 0 || startAddr < 1) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Invalid parameters\"}");
    return;
  }
  for (int n = 0; n < count; n++) {
    int fixID = firstFixID + n;
    int addr  = startAddr + n * DMX_FOOTPRINT;
    int uni   = universe;
    while (addr + DMX_FOOTPRINT - 1 > DMX_CHANNELS) { uni++; addr -= DMX_CHANNELS; }
    if (addr < 1) addr = 1;
    if (fixID == ownFixID) {
      // This is our slice — store it.
      artnet_upsertPatch((uint16_t)uni, (uint16_t)addr);
      break;
    }
  }
  sendJson(req, 200, "{\"status\":\"ok\",\"count\":" + String(count) + "}");
}

// ── Setup (called from wifi_control_setup) ───────────────────────
void artnet_control_setup() {
  artnet_loadPatches();
}
