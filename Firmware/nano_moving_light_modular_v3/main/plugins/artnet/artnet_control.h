#pragma once

// ── Art-Net Control ───────────────────────────────────────────────
// HTTP API routes for Art-Net patch management.
// NVS storage for patch records.
// Helpers for leader→follower UDP patch push.
//
// Included from wifi_control.h, which owns `server` and `prefs`.
// ─────────────────────────────────────────────────────────────────

#include "artnet_globals.h"
#include "artnet_receiver.h"

// ── NVS helpers ───────────────────────────────────────────────────

void artnet_savePatches() {
  Preferences p;
  p.begin("artnet", false);
  p.putInt("version", ARTNET_NVS_VERSION);
  p.putInt("count", artnetPatchCount);
  for (int i = 0; i < artnetPatchCount; i++)
    p.putBytes(("p" + String(i)).c_str(), &artnetPatches[i], sizeof(ArtnetPatch));
  p.end();
}

void artnet_loadPatches() {
  Preferences p;
  p.begin("artnet", true);
  if (p.getInt("version", 0) != ARTNET_NVS_VERSION) {
    p.end();
    artnetPatchCount = 0;
    Serial.println("[ArtNet] Patch store version mismatch — cleared");
    return;
  }
  artnetPatchCount = constrain(p.getInt("count", 0), 0, MAX_PATCHES);
  for (int i = 0; i < artnetPatchCount; i++)
    p.getBytes(("p" + String(i)).c_str(), &artnetPatches[i], sizeof(ArtnetPatch));
  p.end();
  Serial.printf("[ArtNet] Loaded %d patch(es)\n", artnetPatchCount);
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
