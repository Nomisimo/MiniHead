#pragma once

// ── Art-Net Receiver ─────────────────────────────────────────────
// Listens on UDP port 6454 for ArtDmx packets.
// Every node (leader AND follower) runs this independently:
//   - Applies channels that match its own ownFixID patch locally.
//   - Leader additionally fans out to followers via UDP CMD.
// Depends on: artnet_globals.h, core.h, discovery_globals.h
// ─────────────────────────────────────────────────────────────────

#include <WiFiUdp.h>
#include "artnet_globals.h"
#include "../../core.h"
#include "../wifi/discovery_globals.h"
#include "../wifi/udp_control.h"

// ── Global definitions ────────────────────────────────────────────
ArtnetPatch   artnetPatches[MAX_PATCHES];
int           artnetPatchCount  = 0;
bool          artnetActive      = false;
unsigned long artnetLastPacket  = 0;
uint8_t       artnetDmxBuf[DMX_CHANNELS];

static WiFiUDP _artUDP;

// ── Forward declarations ──────────────────────────────────────────
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr);

// ── Packet parser ─────────────────────────────────────────────────
// Returns number of DMX slots on success, -1 on error.
// Fills artnetDmxBuf[] with the DMX data.
// Sets outUniverse to the packet's universe number.

int artnet_parsePacket(const uint8_t* buf, int len, uint16_t& outUniverse) {
  if (len < 18) return -1;

  // Magic header "Art-Net\0"
  if (memcmp(buf, "Art-Net\0", 8) != 0) return -1;

  // OpCode: little-endian 0x0050 = ArtDmx
  uint16_t opcode = buf[8] | ((uint16_t)buf[9] << 8);
  if (opcode != 0x5000) return -1;

  // Protocol version (big-endian): must be >= 14
  uint16_t protVer = ((uint16_t)buf[10] << 8) | buf[11];
  if (protVer < 14) return -1;

  // Universe: lower 15 bits, little-endian bytes 14-15
  outUniverse = (uint16_t)buf[14] | (((uint16_t)buf[15] & 0x7F) << 8);

  // Length: big-endian bytes 16-17
  uint16_t dmxLen = ((uint16_t)buf[16] << 8) | buf[17];
  if (dmxLen == 0 || dmxLen > DMX_CHANNELS) return -1;
  if (len < 18 + (int)dmxLen) return -1;

  // Copy DMX data, zero-fill remainder
  memcpy(artnetDmxBuf, buf + 18, dmxLen);
  if (dmxLen < DMX_CHANNELS)
    memset(artnetDmxBuf + dmxLen, 0, DMX_CHANNELS - dmxLen);

  return (int)dmxLen;
}

// ── Apply DMX to own fixture ─────────────────────────────────────
void artnet_applyOwnPatch(uint16_t universe) {
  for (int i = 0; i < artnetPatchCount; i++) {
    const ArtnetPatch& p = artnetPatches[i];
    if (p.fixID    != ownFixID) continue;
    if (p.universe != universe) continue;

    int base = (int)p.startAddr - 1;  // 0-based
    if (base < 0 || base + DMX_FOOTPRINT > DMX_CHANNELS) continue;

    uint8_t master = artnetDmxBuf[base + CH_MASTER];
    uint8_t r = (uint16_t)artnetDmxBuf[base + CH_RED]   * master / 255;
    uint8_t g = (uint16_t)artnetDmxBuf[base + CH_GREEN]  * master / 255;
    uint8_t b = (uint16_t)artnetDmxBuf[base + CH_BLUE]   * master / 255;
    uint8_t w = (uint16_t)artnetDmxBuf[base + CH_WHITE]  * master / 255;
    int pan   = map(artnetDmxBuf[base + CH_PAN],  0, 255, 0, 180);
    int tilt  = map(artnetDmxBuf[base + CH_TILT], 0, 255, 0, 180);

    rainbowActive = false;
    setLED(r, g, b, w);
    setPan(pan);
    setTilt(tilt);
    break;
  }
}

// ── Leader relay: fan Art-Net data to followers ──────────────────
// Called only when nodeRole == ROLE_LEADER.
// Builds a CMD string for each follower's patched channels and
// sends it via the existing UDP CMD infrastructure.

void artnet_relayToFollowers(uint16_t universe) {
  for (int pi = 0; pi < artnetPatchCount; pi++) {
    const ArtnetPatch& p = artnetPatches[pi];
    if (p.universe != universe)  continue;
    if (p.fixID    == ownFixID)  continue;  // already applied locally

    int base = (int)p.startAddr - 1;
    if (base < 0 || base + DMX_FOOTPRINT > DMX_CHANNELS) continue;

    uint8_t master = artnetDmxBuf[base + CH_MASTER];
    uint8_t r = (uint16_t)artnetDmxBuf[base + CH_RED]   * master / 255;
    uint8_t g = (uint16_t)artnetDmxBuf[base + CH_GREEN]  * master / 255;
    uint8_t b = (uint16_t)artnetDmxBuf[base + CH_BLUE]   * master / 255;
    uint8_t w = (uint16_t)artnetDmxBuf[base + CH_WHITE]  * master / 255;
    int pan   = map(artnetDmxBuf[base + CH_PAN],  0, 255, 0, 180);
    int tilt  = map(artnetDmxBuf[base + CH_TILT], 0, 255, 0, 180);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "R:%d,G:%d,B:%d,W:%d,PAN:%d,TILT:%d",
             r, g, b, w, pan, tilt);

    // Find the peer with this fixID and send
    for (int j = 0; j < peerCount; j++) {
      if (peers[j].active && peers[j].fixID == p.fixID) {
        udp_sendCommand(peers[j].ip, peers[j].mac, cmd);
        break;
      }
    }
  }
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void artnet_receiver_setup() {
  _artUDP.begin(ARTNET_PORT);
  Serial.printf("[ArtNet] Listening on port %d\n", ARTNET_PORT);
}

void artnet_receiver_loop() {
  int sz = _artUDP.parsePacket();
  if (sz > 0) {
    uint8_t buf[530];   // 18 header + 512 max DMX
    int n = _artUDP.read(buf, sizeof(buf) - 1);
    if (n > 0) {
      uint16_t universe;
      int dmxLen = artnet_parsePacket(buf, n, universe);
      if (dmxLen > 0) {
        artnetActive     = true;
        artnetLastPacket = millis();
        artnet_applyOwnPatch(universe);
        if (nodeRole == ROLE_LEADER)
          artnet_relayToFollowers(universe);
      }
    }
  }

  // Timeout watchdog — mark inactive if no packet for ARTNET_TIMEOUT_MS
  if (artnetActive && (millis() - artnetLastPacket > ARTNET_TIMEOUT_MS)) {
    artnetActive = false;
    Serial.println("[ArtNet] Timeout — inactive");
  }
}
