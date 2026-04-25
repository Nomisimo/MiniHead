#pragma once

// ── Art-Net Receiver ─────────────────────────────────────────────
// Receives ArtDmx packets via AsyncUDP (built into ESP32 Arduino Core).
// No external library required — replaces ArtnetWifi dependency.
// Callback runs on the WiFi task, so no loop polling is needed.
//
// Every node (leader AND follower) runs this independently:
//   - Applies channels that match its own ownFixID patch locally.
//   - Leader additionally fans out to followers via UDP CMD.
//
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

static WiFiUDP _artnetUdp;

// ── Forward declarations ──────────────────────────────────────────
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr);
static void artnet_onDmxFrame(uint16_t universe, uint16_t length,
                               uint8_t sequence, uint8_t* data);

// ── Apply DMX to own fixture ──────────────────────────────────────
static void artnet_applyOwnPatch(uint16_t universe, uint16_t length, uint8_t* data) {
  // Track previous output values — only log when something actually changes
  static uint8_t pM=0, pR=0, pG=0, pB=0, pW=0;
  static int     pPan=-1, pTilt=-1;

  for (int i = 0; i < artnetPatchCount; i++) {
    const ArtnetPatch& p = artnetPatches[i];
    if (p.fixID    != ownFixID) continue;
    if (p.universe != universe) continue;

    int base = (int)p.startAddr - 1;
    if (base < 0 || base + DMX_FOOTPRINT > (int)length) {
      Serial.printf("[ArtNet] SKIP Fix#%d base=%d len=%u\n", p.fixID, base, length);
      continue;
    }

    uint8_t master = data[base + CH_MASTER];
    uint8_t r = (uint16_t)data[base + CH_RED]   * master / 255;
    uint8_t g = (uint16_t)data[base + CH_GREEN]  * master / 255;
    uint8_t b = (uint16_t)data[base + CH_BLUE]   * master / 255;
    uint8_t w = (uint16_t)data[base + CH_WHITE]  * master / 255;
    int pan   = map(data[base + CH_PAN],  0, 255, 0, 180);
    int tilt  = map(data[base + CH_TILT], 0, 255, 0, 180);

    if (master!=pM || r!=pR || g!=pG || b!=pB || w!=pW || pan!=pPan || tilt!=pTilt) {
      Serial.printf("[ArtNet] Fix#%d  M=%u R=%u G=%u B=%u W=%u  PAN=%d TILT=%d\n",
                    p.fixID, master, r, g, b, w, pan, tilt);
      pM=master; pR=r; pG=g; pB=b; pW=w; pPan=pan; pTilt=tilt;
    }

    rainbowActive = false;
    setLED(r, g, b, w);
    setPan(pan);
    setTilt(tilt);
    return;
  }
}

// ── Leader relay: fan Art-Net data to followers ───────────────────
static void artnet_relayToFollowers(uint16_t universe, uint16_t length, uint8_t* data) {
  for (int pi = 0; pi < artnetPatchCount; pi++) {
    const ArtnetPatch& p = artnetPatches[pi];
    if (p.universe != universe) continue;
    if (p.fixID    == ownFixID) continue;

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
    snprintf(cmd, sizeof(cmd), "R:%d,G:%d,B:%d,W:%d,PAN:%d,TILT:%d", r, g, b, w, pan, tilt);

    for (int j = 0; j < peerCount; j++) {
      if (peers[j].active && peers[j].fixID == p.fixID) {
        udp_sendCommand(peers[j].ip, peers[j].mac, cmd);
        break;
      }
    }
  }
}

// ── ArtDmx callback ───────────────────────────────────────────────
static void artnet_onDmxFrame(uint16_t universe, uint16_t length,
                               uint8_t sequence, uint8_t* data) {
  artnetActive     = true;
  artnetLastPacket = millis();
  artnet_applyOwnPatch(universe, length, data);
  if (nodeRole == ROLE_LEADER)
    artnet_relayToFollowers(universe, length, data);
}

// ── Art-Net ArtDmx packet parser ──────────────────────────────────
// Spec: http://www.artisticlicence.com/ArtNetSpec.pdf  §Table 1
static void artnet_parsePacket(uint8_t* data, size_t len) {
  if (len < 20) return;
  // ID: "Art-Net\0"
  if (memcmp(data, "Art-Net\0", 8) != 0) return;
  // OpCode: ArtDmx = 0x5000, transmitted LE
  uint16_t opcode = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
  if (opcode != 0x5000) return;
  uint8_t  sequence = data[12];
  // Universe: LE, 15-bit (bytes 14-15)
  uint16_t universe = (uint16_t)data[14] | ((uint16_t)data[15] << 8);
  // Length: BE (bytes 16-17), 2–512, always even
  uint16_t length   = ((uint16_t)data[16] << 8) | (uint16_t)data[17];
  if (len < (size_t)(18 + length)) return;
  artnet_onDmxFrame(universe, length, sequence, data + 18);
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void artnet_receiver_setup() {
  // Use WiFiUDP polling — same pattern as udp_control.h (port 4211),
  // confirmed working on ESP32-C3 / Arduino Core 3.x / IDF 5.x.
  if (_artnetUdp.begin(ARTNET_PORT)) {
    Serial.printf("[ArtNet] Listening on port %d\n", ARTNET_PORT);
  } else {
    Serial.printf("[ArtNet] ERROR: failed to bind port %d\n", ARTNET_PORT);
  }
}

void artnet_receiver_loop() {
  int sz = _artnetUdp.parsePacket();
  if (sz > 0) {
    static uint8_t buf[530];   // 18-byte Art-Net header + 512 DMX channels
    int n = _artnetUdp.read(buf, sizeof(buf));
    if (n > 0) artnet_parsePacket(buf, (size_t)n);
  }
  if (artnetActive && (millis() - artnetLastPacket > ARTNET_TIMEOUT_MS)) {
    artnetActive = false;
    Serial.println("[ArtNet] Timeout — inactive");
  }
}
