#pragma once

// ── Art-Net Receiver ─────────────────────────────────────────────
// Receives ArtDmx packets via the ArtnetWifi library.
// Every node (leader AND follower) runs this independently:
//   - Applies channels that match its own ownFixID patch locally.
//   - Leader additionally fans out to followers via UDP CMD.
//
// Library: ArtnetWifi by rstephan
//   https://github.com/rstephan/ArtnetWifi
// Depends on: artnet_globals.h, core.h, discovery_globals.h
// ─────────────────────────────────────────────────────────────────

#include <ArtnetWifi.h>
#include "artnet_globals.h"
#include "../../core.h"
#include "../wifi/discovery_globals.h"
#include "../wifi/udp_control.h"

// ── Global definitions ────────────────────────────────────────────
ArtnetPatch   artnetPatches[MAX_PATCHES];
int           artnetPatchCount  = 0;
bool          artnetActive      = false;
unsigned long artnetLastPacket  = 0;

static ArtnetWifi _artnet;

// ── Forward declarations ──────────────────────────────────────────
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr);

// ── Apply DMX to own fixture ──────────────────────────────────────
void artnet_applyOwnPatch(uint16_t universe, uint16_t length, uint8_t* data) {
  // Debug: throttled print — shows which universe arrived and whether
  // our patch matched. Fires on first packet and whenever uni changes.
  static uint16_t  dbg_lastUni   = 0xFFFF;
  static bool      dbg_matched   = false;
  static unsigned long dbg_t     = 0;
  bool print = (universe != dbg_lastUni || millis() - dbg_t > 2000);

  bool matched = false;
  for (int i = 0; i < artnetPatchCount; i++) {
    const ArtnetPatch& p = artnetPatches[i];

    if (print) {
      Serial.printf("[ArtNet] patch[%d] fixID=%d ownFixID=%d uni=%u p.uni=%u addr=%u\n",
                    i, p.fixID, ownFixID, universe, p.universe, p.startAddr);
    }

    if (p.fixID    != ownFixID) continue;
    if (p.universe != universe) continue;

    int base = (int)p.startAddr - 1;  // convert to 0-based
    if (base < 0 || base + DMX_FOOTPRINT > (int)length) {
      Serial.printf("[ArtNet] SKIP Fix#%d — base=%d length=%u\n",
                    p.fixID, base, length);
      continue;
    }

    uint8_t master = data[base + CH_MASTER];
    uint8_t r = (uint16_t)data[base + CH_RED]   * master / 255;
    uint8_t g = (uint16_t)data[base + CH_GREEN]  * master / 255;
    uint8_t b = (uint16_t)data[base + CH_BLUE]   * master / 255;
    uint8_t w = (uint16_t)data[base + CH_WHITE]  * master / 255;
    int pan   = map(data[base + CH_PAN],  0, 255, 0, 180);
    int tilt  = map(data[base + CH_TILT], 0, 255, 0, 180);

    if (print) {
      Serial.printf("[ArtNet] MATCH Fix#%d uni=%u addr=%u  M=%u R=%u G=%u B=%u W=%u PAN=%d TILT=%d\n",
                    p.fixID, universe, p.startAddr,
                    master, r, g, b, w, pan, tilt);
    }

    rainbowActive = false;
    setLED(r, g, b, w);
    setPan(pan);
    setTilt(tilt);
    matched = true;
    break;
  }

  if (print && !matched) {
    Serial.printf("[ArtNet] NO MATCH for uni=%u  (patchCount=%d  ownFixID=%d)\n",
                  universe, artnetPatchCount, ownFixID);
  }

  dbg_lastUni = universe;
  dbg_matched = matched;
  dbg_t       = millis();
}

// ── Leader relay: fan Art-Net data to followers ───────────────────
void artnet_relayToFollowers(uint16_t universe, uint16_t length, uint8_t* data) {
  for (int pi = 0; pi < artnetPatchCount; pi++) {
    const ArtnetPatch& p = artnetPatches[pi];
    if (p.universe != universe) continue;
    if (p.fixID    == ownFixID) continue;  // already applied locally

    int base = (int)p.startAddr - 1;
    if (base < 0 || base + DMX_FOOTPRINT > (int)length) continue;

    uint8_t master = data[base + CH_MASTER];
    uint8_t r = (uint16_t)data[base + CH_RED]   * master / 255;
    uint8_t g = (uint16_t)data[base + CH_GREEN]  * master / 255;
    uint8_t b = (uint16_t)data[base + CH_BLUE]   * master / 255;
    uint8_t w = (uint16_t)data[base + CH_WHITE]  * master / 255;
    int pan   = map(data[base + CH_PAN],  0, 255, 0, 180);
    int tilt  = map(data[base + CH_TILT], 0, 255, 0, 180);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "R:%d,G:%d,B:%d,W:%d,PAN:%d,TILT:%d",
             r, g, b, w, pan, tilt);

    for (int j = 0; j < peerCount; j++) {
      if (peers[j].active && peers[j].fixID == p.fixID) {
        udp_sendCommand(peers[j].ip, peers[j].mac, cmd);
        break;
      }
    }
  }
}

// ── ArtDmx callback — invoked by ArtnetWifi on every valid packet ─
void artnet_onDmxFrame(uint16_t universe, uint16_t length,
                       uint8_t sequence, uint8_t* data) {
  artnetActive     = true;
  artnetLastPacket = millis();
  artnet_applyOwnPatch(universe, length, data);
  if (nodeRole == ROLE_LEADER)
    artnet_relayToFollowers(universe, length, data);
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void artnet_receiver_setup() {
  _artnet.setArtDmxCallback(artnet_onDmxFrame);
  _artnet.begin();
  Serial.printf("[ArtNet] Listening on port %d\n", ARTNET_PORT);
}

void artnet_receiver_loop() {
  _artnet.read();

  // Timeout watchdog — mark inactive if no packet for ARTNET_TIMEOUT_MS
  if (artnetActive && (millis() - artnetLastPacket > ARTNET_TIMEOUT_MS)) {
    artnetActive = false;
    Serial.println("[ArtNet] Timeout — inactive");
  }
}
