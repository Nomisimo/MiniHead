#pragma once

// ── Art-Net Receiver ─────────────────────────────────────────────
// Receives ArtDmx packets via WiFiUDP (built into ESP32 Arduino Core).
// No external library required.
//
// Each ESP handles ArtNet entirely on its own:
//   - Listens on UDP port 6454 for all incoming packets.
//   - Applies only packets that match its own fixID / universe / startAddr.
//   - No relay to other nodes — the ArtNet source addresses each fixture directly.
//
// Depends on: artnet_globals.h, core.h, discovery_globals.h
// ─────────────────────────────────────────────────────────────────

#include <WiFiUdp.h>
#include "artnet_globals.h"
#include "../../core.h"
#include "../wifi/log_config.h"
#include "../wifi/discovery_globals.h"
#include "../storage/storage.h"

// ── Global definitions ────────────────────────────────────────────
ArtnetPatch   artnetPatches[MAX_PATCHES];
int           artnetPatchCount  = 0;
bool          artnetActive      = false;
unsigned long artnetLastPacket  = 0;

static WiFiUDP _artnetUdp;

// Pre-ArtNet state — saved when ArtNet first becomes active so we can
// restore the light to what it was before ArtNet took over.
static bool    _artnetHadPre  = false;
static uint8_t _preArtR=0, _preArtG=0, _preArtB=0, _preArtW=0;
static int     _preArtPan=90, _preArtTilt=90;

// ── Forward declarations ──────────────────────────────────────────
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr);
static bool artnet_applyOwnPatch(uint16_t universe, uint16_t length, uint8_t* data);
static void artnet_onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);

// ── Patch persistence (must live here so every node can load/save) ─
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

// ── Apply DMX to own fixture ──────────────────────────────────────
// Returns true if a patch matched and values were applied — used by
// artnet_onDmxFrame to decide whether to set artnetActive.
static bool artnet_applyOwnPatch(uint16_t universe, uint16_t length, uint8_t* data) {
  // Identify is held — do not let Art-Net override the white flash
  if (_identifyActive) return false;

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
    int pan   = map(data[base + CH_PAN],  0, 255, 0, 270);
    int tilt  = map(data[base + CH_TILT], 0, 255, 0, 270);

    if (master!=pM || r!=pR || g!=pG || b!=pB || w!=pW || pan!=pPan || tilt!=pTilt) {
      if (logCfg.artnetFrames)
        Serial.printf("[ArtNet] Fix#%d  M=%u R=%u G=%u B=%u W=%u  PAN=%d TILT=%d\n",
                      p.fixID, master, r, g, b, w, pan, tilt);
      pM=master; pR=r; pG=g; pB=b; pW=w; pPan=pan; pTilt=tilt;
    }

    // ArtNet is pure DMX delivery — it does not own local effect modes.
    // If rainbow is running, ArtNet still drives the servos but does not
    // write to the LED; the rainbow loop already owns the strip.
    if (!rainbowActive) setLED(r, g, b, w);
    setPan(pan);
    setTilt(tilt);
    return true;   // matched — artnet_onDmxFrame will set artnetActive
  }
  return false;    // no patch matched this universe → artnetActive unchanged
}

// ── ArtDmx callback ───────────────────────────────────────────────
// Every ESP handles its own ArtNet reception independently.
// No relay to other nodes — each fixture is patched and receives directly.
static void artnet_onDmxFrame(uint16_t universe, uint16_t length,
                               uint8_t sequence, uint8_t* data) {
  // Always log packet arrival (gated) so the user can distinguish
  // "no packets arriving" from "packets arriving but no patch match".
  static uint16_t _lastLoggedUni = 0xFFFF;
  if (logCfg.artnetEvents && universe != _lastLoggedUni) {
    Serial.printf("[ArtNet] Packet received — universe %d  (own patch: U%d Fix#%d)\n",
                  universe,
                  artnetPatchCount > 0 ? artnetPatches[0].universe : -1,
                  artnetPatchCount > 0 ? artnetPatches[0].fixID    : -1);
    _lastLoggedUni = universe;
  }

  bool matched = artnet_applyOwnPatch(universe, length, data);

  if (matched) {
    if (!artnetActive) {
      // First matching packet of a new session — save state for later restore.
      if (!_artnetHadPre) {
        _preArtR    = curR;  _preArtG    = curG;
        _preArtB    = curB;  _preArtW    = curW;
        _preArtPan  = curPan; _preArtTilt = curTilt;
        _artnetHadPre = true;
      }
      if (logCfg.artnetEvents)
        Serial.printf("[ArtNet] Active — universe %d\n", universe);
    }
    artnetActive     = true;
    artnetLastPacket = millis();
  }
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
  artnet_loadPatches();

  // Every ESP handles ArtNet independently — keep only its own fixID's patch.
  // There is no relay; each fixture is addressed directly by the ArtNet source.
  {
    int keep = 0;
    for (int i = 0; i < artnetPatchCount; i++) {
      if (artnetPatches[i].fixID == ownFixID)
        artnetPatches[keep++] = artnetPatches[i];
    }
    artnetPatchCount = keep;
    if (keep > 0)
      Serial.printf("[ArtNet] Own patch: Fix#%d U%d Addr%d\n",
        artnetPatches[0].fixID, artnetPatches[0].universe, artnetPatches[0].startAddr);
    else
      Serial.println("[ArtNet] No patch for own fixID yet");
  }

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
    artnetActive  = false;
    _artnetHadPre = false;   // reset so next session saves a fresh pre-state
    if (logCfg.artnetEvents) Serial.println("[ArtNet] Timeout — restoring pre-ArtNet state");
    // Restore the light to whatever it was before ArtNet took over.
    // Without this the LED stays black (M=0 from the last ArtNet frame) forever.
    if (!_identifyActive) {
      setLED(_preArtR, _preArtG, _preArtB, _preArtW);
      setPan(_preArtPan); setTilt(_preArtTilt);
    }
  }
}
