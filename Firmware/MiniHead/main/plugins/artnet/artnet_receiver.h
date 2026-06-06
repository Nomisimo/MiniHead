#pragma once

// ── Art-Net Receiver ─────────────────────────────────────────────
// Receives ArtDmx packets via WiFiUDP (built into ESP32 Arduino Core).
// No external library required.
//
// Each ESP handles ArtNet entirely on its own:
//   - Listens on UDP port 6454 for all incoming packets.
//   - Applies only packets that match its own universe / startAddr.
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
String        artnetSenderIP    = "";

static WiFiUDP _artnetUdp;

// Pre-ArtNet state — saved when ArtNet first becomes active so we can
// restore the light to what it was before ArtNet took over.
static bool    _artnetHadPre  = false;
static uint8_t _preArtR=0, _preArtG=0, _preArtB=0, _preArtW=0;
static int     _preArtPan=90, _preArtTilt=90;

// ── Forward declarations ──────────────────────────────────────────
void artnet_upsertPatch(uint16_t universe, uint16_t startAddr);
static bool artnet_applyOwnPatch(uint16_t universe, uint16_t length, uint8_t* data);
static void artnet_onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);
static void artnet_parsePacket(uint8_t* data, size_t len, IPAddress sender);

// ── Patch persistence (must live here so every node can load/save) ─
void artnet_savePatches() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < artnetPatchCount; i++) {
    JsonObject o = arr.add<JsonObject>();
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
    uint16_t uni  = o["universe"]  | 0;
    uint16_t addr = o["startAddr"] | 1;
    if (uni > 32767) { Serial.println("[ArtNet] Skipping stored patch with universe > 32767 — invalid"); continue; }
    artnetPatches[artnetPatchCount].universe  = uni;
    artnetPatches[artnetPatchCount].startAddr = addr;
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
    if (p.universe != universe) continue;

    int base = (int)p.startAddr - 1;
    if (base < 0 || base + DMX_FOOTPRINT > (int)length) {
      Serial.printf("[ArtNet] SKIP base=%d len=%u\n", base, length);
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
        Serial.printf("[ArtNet] U%u Addr%u  M=%u R=%u G=%u B=%u W=%u  PAN=%d TILT=%d\n",
                      p.universe, p.startAddr, master, r, g, b, w, pan, tilt);
      pM=master; pR=r; pG=g; pB=b; pW=w; pPan=pan; pTilt=tilt;
    }

    // Local animations (rainbow / demo) take priority over incoming ArtNet.
    // When a standalone effect is active the ESP ignores ArtNet values so
    // the show can run without a lighting console attached.
    if (!rainbowActive && !demoActive) setLED(r, g, b, w);
    if (!demoActive) { setPan(pan); setTilt(tilt); }
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
    Serial.printf("[ArtNet] Packet received — universe %u  (own patch: U%d Addr%d)\n",
                  universe,
                  artnetPatchCount > 0 ? (int)artnetPatches[0].universe  : -1,
                  artnetPatchCount > 0 ? (int)artnetPatches[0].startAddr : -1);
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

// ── ArtPollReply ──────────────────────────────────────────────────
// Sent unicast to the ArtPoll originator. One reply per configured patch
// (or one reply with universe 0 if no patches exist).
static void artnet_sendPollReply(IPAddress dest) {
  IPAddress localIP = WiFi.localIP();
  Serial.printf("[ArtNet] ArtPollReply → %s  uni=%u\n",
                dest.toString().c_str(),
                artnetPatchCount > 0 ? artnetPatches[0].universe : 0);

  uint8_t mac[6] = {};
  sscanf(ownMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

  int count = max(1, artnetPatchCount);
  for (int pi = 0; pi < count; pi++) {
    uint16_t uni  = (pi < artnetPatchCount) ? artnetPatches[pi].universe : 0;
    uint8_t  net  = (uni >> 8) & 0x7F;
    uint8_t  sub  = (uni >> 4) & 0x0F;
    uint8_t  sw   = uni & 0x0F;

    uint8_t reply[239] = {};
    memcpy(reply, "Art-Net\0", 8);
    reply[8]  = 0x00; reply[9]  = 0x21;               // OpCode 0x2100 LE
    reply[10] = localIP[0]; reply[11] = localIP[1];    // IP address
    reply[12] = localIP[2]; reply[13] = localIP[3];
    reply[14] = 0x36; reply[15] = 0x19;               // Port 6454 LE
    reply[16] = 0;    reply[17] = 14;                  // VersInfo = 14 (Art-Net 4)
    reply[18] = net;  reply[19] = sub;                  // NetSwitch / SubSwitch
    reply[20] = 0xFF; reply[21] = 0xFF;                // Oem = 0xFFFF (general)
    // [22] UbeaVersion = 0, [23] Status1 = 0
    // [24-25] EstaMan = 0x0000
    char _shortName[18] = {};
    if (ownName[0]) snprintf(_shortName, 18, "%s", ownName);
    else            snprintf(_shortName, 18, "MiniHead-%d", ownFixID);
    strncpy((char*)(reply + 26), _shortName, 17);                         // ShortName
    char _longName[64] = {};
    snprintf(_longName, 64, "MiniHead Fix#%d U%u", ownFixID, uni);
    strncpy((char*)(reply + 44), _longName, 63);                          // LongName
    snprintf((char*)(reply + 108), 64, "#0001 [0000] OK");                // NodeReport
    reply[173] = 1;                                    // NumPorts = 1
    reply[174] = 0x80;                                 // PortTypes[0] = DMX output
    reply[182] = 0x80;                                 // GoodOutputA[0] = transmitting
    reply[186] = sw;                                   // SwIn[0]
    reply[190] = sw;                                   // SwOut[0]
    reply[194] = 100;                                  // AcnPriority
    reply[200] = 0x00;                                 // Style = StNode
    memcpy(reply + 201, mac, 6);                       // MAC[6]
    reply[207] = localIP[0]; reply[208] = localIP[1]; // BindIp[4]
    reply[209] = localIP[2]; reply[210] = localIP[3];
    reply[211] = 1;                                    // BindIndex = 1
    reply[212] = 0x08;                                 // Status2 = DHCP capable

    _artnetUdp.beginPacket(dest, ARTNET_PORT);
    _artnetUdp.write(reply, 239);
    _artnetUdp.endPacket();
  }
}

// ── Art-Net packet parser ─────────────────────────────────────────
// Spec: http://www.artisticlicence.com/ArtNetSpec.pdf  §Table 1
static void artnet_parsePacket(uint8_t* data, size_t len, IPAddress sender) {
  if (len < 12) return;
  // ID: "Art-Net\0"
  if (memcmp(data, "Art-Net\0", 8) != 0) return;
  // OpCode: transmitted LE (bytes 8-9)
  uint16_t opcode = (uint16_t)data[8] | ((uint16_t)data[9] << 8);

  if (opcode == 0x2000) {                              // ArtPoll — reply and return
    Serial.printf("[ArtNet] ArtPoll from %s\n", sender.toString().c_str());
    artnet_sendPollReply(sender);
    return;
  }
  if (opcode != 0x5000) return;                        // not ArtDmx — ignore
  if (len < 20) return;

  // ProtVer (bytes 10-11, BE): MUST be ≥ 14
  uint16_t protVer = ((uint16_t)data[10] << 8) | data[11];
  if (protVer < 14) return;

  uint8_t  sequence = data[12];
  // Universe: LE, 15-bit (bytes 14-15)
  uint16_t universe = (uint16_t)data[14] | ((uint16_t)data[15] << 8);
  // Length: BE (bytes 16-17) — MUST be even and ≥ 2
  uint16_t length   = ((uint16_t)data[16] << 8) | data[17];
  if (length < 2 || (length & 1) != 0) return;
  if (len < (size_t)(18 + length)) return;

  artnet_onDmxFrame(universe, length, sequence, data + 18);
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void artnet_receiver_setup() {
  artnet_loadPatches();

  if (artnetPatchCount > 0)
    Serial.printf("[ArtNet] Own patch: U%d Addr%d\n",
      artnetPatches[0].universe, artnetPatches[0].startAddr);
  else
    Serial.println("[ArtNet] No patch configured yet");

  if (_artnetUdp.begin(ARTNET_PORT)) {
    Serial.printf("[ArtNet] Listening on port %d\n", ARTNET_PORT);
  } else {
    Serial.printf("[ArtNet] ERROR: failed to bind port %d\n", ARTNET_PORT);
  }
}

void artnet_receiver_loop() {
  int sz = _artnetUdp.parsePacket();
  if (sz > 0) {
    // Capture sender before read() — remoteIP() may change after the buffer is consumed
    IPAddress sender = _artnetUdp.remoteIP();
    artnetSenderIP = sender.toString();
    static uint8_t buf[530];   // 18-byte Art-Net header + 512 DMX channels
    int n = _artnetUdp.read(buf, sizeof(buf));
    if (n > 0) artnet_parsePacket(buf, (size_t)n, sender);
  }

  // ── Packet rate monitor — logs once per second ────────────────
  static unsigned long _rateWindowStart = 0;
  static uint32_t      _rateCount       = 0;
  if (sz > 0) _rateCount++;
  {
    unsigned long now = millis();
    if (now - _rateWindowStart >= 1000) {
      if (logCfg.artnetEvents)
        Serial.printf("[ArtNet] %u pkt/s\n", _rateCount);
      _rateCount       = 0;
      _rateWindowStart = now;
    }
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
