#pragma once

// ── Art-Net Control ───────────────────────────────────────────────
// HTTP API routes for Art-Net patch management.
// LittleFS JSON storage for patch records (/artnet.json).
// Helpers for leader→follower UDP patch push.
//
// Included from wifi_control.h, which owns `server` and `prefs`.
// ─────────────────────────────────────────────────────────────────

#include "artnet_globals.h"
#include "artnet_receiver.h"

// ── Storage (/artnet.json) ────────────────────────────────────────
// Schema: [{"fixID":1,"universe":0,"startAddr":1}]

void artnet_savePatches() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < artnetPatchCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["fixID"]     = artnetPatches[i].fixID;
    o["universe"]  = artnetPatches[i].universe;
    o["startAddr"] = artnetPatches[i].startAddr;
  }
  storage_writeJson("/artnet.json", doc);
}

void artnet_loadPatches() {
  artnetPatchCount = 0;
  JsonDocument doc;
  if (!storage_readJson("/artnet.json", doc)) {
    Serial.println("[ArtNet] No patch data — starting fresh");
    return;
  }
  for (JsonObject o : doc.as<JsonArray>()) {
    if (artnetPatchCount >= MAX_PATCHES) break;
    artnetPatches[artnetPatchCount].fixID     = o["fixID"]     | 0;
    artnetPatches[artnetPatchCount].universe  = o["universe"]  | 0;
    artnetPatches[artnetPatchCount].startAddr = o["startAddr"] | 1;
    artnetPatchCount++;
  }
  Serial.printf("[ArtNet] Loaded %d patch(es) from /artnet.json\n", artnetPatchCount);
}

// ── Upsert helper — add or update a patch by fixID ───────────────
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr) {
  for (int i = 0; i < artnetPatchCount; i++) {
    if (artnetPatches[i].fixID == fixID) {
      artnetPatches[i].universe  = universe;
      artnetPatches[i].startAddr = startAddr;
      artnet_savePatches();
      return;
    }
  }
  if (artnetPatchCount >= MAX_PATCHES) return;
  artnetPatches[artnetPatchCount].fixID     = fixID;
  artnetPatches[artnetPatchCount].universe  = universe;
  artnetPatches[artnetPatchCount].startAddr = startAddr;
  artnetPatchCount++;
  artnet_savePatches();
}

// ── UDP push helper — send patch to a follower ───────────────────
void artnet_pushPatchToFollower(const char* ip, const char* mac,
                                int fixID, int universe, int startAddr) {
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "SETPATCH:%d,%d,%d", fixID, universe, startAddr);
  udp_sendCommand(ip, mac, cmd);
}

// ── JSON serialiser ───────────────────────────────────────────────
String artnetPatchToJson(const ArtnetPatch& p) {
  return "{\"fixID\":" + String(p.fixID) +
         ",\"universe\":" + String(p.universe) +
         ",\"startAddr\":" + String(p.startAddr) + "}";
}

// ── HTTP handlers ─────────────────────────────────────────────────

void handleArtnetStatus() {
  String json = "{\"active\":";
  json += artnetActive ? "true" : "false";
  json += ",\"patchCount\":" + String(artnetPatchCount) + "}";
  sendJson(200, json);
}

void handleGetArtnetPatch() {
  String json = "[";
  for (int i = 0; i < artnetPatchCount; i++) {
    if (i > 0) json += ",";
    json += artnetPatchToJson(artnetPatches[i]);
  }
  json += "]";
  sendJson(200, json);
}

void handlePostArtnetPatch() {
  if (artnetPatchCount >= MAX_PATCHES) {
    sendJson(500, "{\"status\":\"error\",\"message\":\"Max patches reached\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson(400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}");
    return;
  }
  int fixID     = doc["fixID"]     | 0;
  int universe  = doc["universe"]  | 0;
  int startAddr = doc["startAddr"] | 1;
  if (fixID <= 0 || startAddr < 1 || startAddr + DMX_FOOTPRINT - 1 > DMX_CHANNELS) {
    sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid fixID or startAddr\"}");
    return;
  }
  artnet_upsertPatch(fixID, (uint16_t)universe, (uint16_t)startAddr);
  // Push to follower peer if applicable
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && peers[i].fixID == fixID) {
      artnet_pushPatchToFollower(peers[i].ip, peers[i].mac, fixID, universe, startAddr);
      break;
    }
  }
  sendJson(200, "{\"status\":\"ok\"}");
}

// PUT /api/artnet/patch/:fixID  — update universe / startAddr of existing patch
void handleUpdateArtnetPatch() {
  String path = server.uri();
  int fixID = path.substring(path.lastIndexOf('/') + 1).toInt();
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson(400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return;
  }
  int universe  = doc["universe"]  | -1;
  int startAddr = doc["startAddr"] | -1;
  // Find existing patch
  for (int i = 0; i < artnetPatchCount; i++) {
    if (artnetPatches[i].fixID == fixID) {
      if (universe  >= 0) artnetPatches[i].universe  = (uint16_t)universe;
      if (startAddr >= 1 && startAddr + DMX_FOOTPRINT - 1 <= DMX_CHANNELS)
        artnetPatches[i].startAddr = (uint16_t)startAddr;
      artnet_savePatches();
      // Push to follower if applicable
      for (int j = 0; j < peerCount; j++) {
        if (peers[j].active && peers[j].fixID == fixID) {
          artnet_pushPatchToFollower(peers[j].ip, peers[j].mac, fixID,
                                     artnetPatches[i].universe, artnetPatches[i].startAddr);
          break;
        }
      }
      sendJson(200, "{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(404, "{\"status\":\"error\",\"message\":\"Patch not found\"}");
}

void handleDeleteArtnetPatch() {
  String path = server.uri();
  int fixID = path.substring(path.lastIndexOf('/') + 1).toInt();
  for (int i = 0; i < artnetPatchCount; i++) {
    if (artnetPatches[i].fixID == fixID) {
      for (int j = i; j < artnetPatchCount - 1; j++)
        artnetPatches[j] = artnetPatches[j + 1];
      artnetPatchCount--;
      artnet_savePatches();
      sendJson(200, "{\"status\":\"ok\"}");
      return;
    }
  }
  sendJson(404, "{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleBulkArtnetPatch() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson(400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}");
    return;
  }
  int universe   = doc["universe"]   | 0;
  int startAddr  = doc["startAddr"]  | 1;
  int count      = doc["count"]      | 0;
  int firstFixID = doc["firstFixID"] | 1;

  if (count <= 0 || firstFixID <= 0 || startAddr < 1) {
    sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid parameters\"}");
    return;
  }

  int patched = 0;
  for (int n = 0; n < count; n++) {
    int fixID = firstFixID + n;
    int addr  = startAddr + n * DMX_FOOTPRINT;
    int uni   = universe;
    // Auto-overflow to next universe when address exceeds 512
    while (addr + DMX_FOOTPRINT - 1 > DMX_CHANNELS) {
      uni++;
      addr -= DMX_CHANNELS;
    }
    if (addr < 1) addr = 1;
    artnet_upsertPatch(fixID, (uint16_t)uni, (uint16_t)addr);
    // Push to follower if applicable
    for (int i = 0; i < peerCount; i++) {
      if (peers[i].active && peers[i].fixID == fixID) {
        artnet_pushPatchToFollower(peers[i].ip, peers[i].mac, fixID, uni, addr);
        break;
      }
    }
    patched++;
  }
  sendJson(200, "{\"status\":\"ok\",\"count\":" + String(patched) + "}");
}

// ── Setup (called from wifi_control_setup) ───────────────────────
void artnet_control_setup() {
  artnet_loadPatches();
}
