#pragma once

// ── Discovery Plugin ──────────────────────────────────────────────
// Handles: UDP beacon broadcast + receive, leader election,
//          peer table maintenance, on-the-fly role promotion
// Depends on: core.h (setLED), discovery_globals.h, wifi_control.h
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WiFiUdp.h>
#include "discovery_globals.h"
#include "wifi_control.h"

// ── Globals (definitions) ─────────────────────────────────────────
NodeRole nodeRole   = ROLE_UNDECIDED;
char ownMAC[18]     = "";
char ownIP[16]      = "";
int  ownFixID       = 0;
char ownName[32]    = "";
Peer peers[MAX_PEERS];
int  peerCount      = 0;

// ── Internal ──────────────────────────────────────────────────────
static WiFiUDP _beaconUDP;
static unsigned long _lastBeaconSent = 0;
static bool _electionDone = false;
static bool _webServerStarted = false;

// ── Storage (/discovery.json) ─────────────────────────────────────
// Schema: {"fixID": 1, "name": "Head 1"}

void discovery_saveSettings() {
  JsonDocument doc;
  doc["fixID"] = ownFixID;
  doc["name"]  = ownName;
  storage_writeJson("/discovery.json", doc);
}

void discovery_loadSettings() {
  JsonDocument doc;
  if (storage_readJson("/discovery.json", doc)) {
    ownFixID = doc["fixID"] | 0;
    strlcpy(ownName, doc["name"] | "", sizeof(ownName));
  }
  // No NVS fallback — clean v4.2 start
}

// Thin wrappers so callers in wifi_control.h + udp_control.h are unchanged:
void discovery_saveFixID(int id)          { ownFixID = id; discovery_saveSettings(); }
void discovery_saveName(const char* name) { strlcpy(ownName, name, sizeof(ownName)); discovery_saveSettings(); }

// ── Peer helpers ──────────────────────────────────────────────────

Peer* discovery_findPeer(const char* mac) {
  for (int i = 0; i < peerCount; i++)
    if (strcmp(peers[i].mac, mac) == 0) return &peers[i];
  return nullptr;
}

Peer* discovery_addOrUpdate(const char* mac, const char* ip, int fixID, NodeRole role, const char* name = "") {
  Peer* p = discovery_findPeer(mac);
  if (!p) {
    if (peerCount >= MAX_PEERS) return nullptr;
    p = &peers[peerCount++];
    strncpy(p->mac, mac, 17); p->mac[17] = 0;
    p->name[0] = 0;
  }
  strncpy(p->ip, ip, 15); p->ip[15] = 0;
  if (name && name[0]) strlcpy(p->name, name, sizeof(p->name));
  p->fixID    = fixID;
  p->role     = role;
  p->lastSeen = millis();
  p->active   = true;
  return p;
}

void discovery_pruneStale() {
  unsigned long now = millis();
  for (int i = 0; i < peerCount; i++)
    if (peers[i].active && (now - peers[i].lastSeen > LEADER_TIMEOUT_MS))
      peers[i].active = false;
}

bool discovery_leaderAlive() {
  unsigned long now = millis();
  for (int i = 0; i < peerCount; i++)
    if (peers[i].active && peers[i].role == ROLE_LEADER &&
        strcmp(peers[i].mac, ownMAC) != 0 &&
        (now - peers[i].lastSeen < LEADER_TIMEOUT_MS)) return true;
  return false;
}

// ── Election ──────────────────────────────────────────────────────
// Winner = lowest MAC string (lexicographic).
// PC always wins — its fake MAC "00:00:00:00:00:PC" sorts below
// any real ESP32 MAC (digits < letters in ASCII).

void discovery_elect() {
  struct Candidate { char mac[18]; };
  Candidate candidates[MAX_PEERS + 1];
  int n = 0;

  strncpy(candidates[n].mac, ownMAC, 17); candidates[n].mac[17] = 0; n++;
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active) continue;
    strncpy(candidates[n].mac, peers[i].mac, 17); candidates[n].mac[17] = 0; n++;
  }

  int winIdx = 0;
  for (int i = 1; i < n; i++)
    if (strcmp(candidates[i].mac, candidates[winIdx].mac) < 0) winIdx = i;

  bool iAmLeader = (strcmp(candidates[winIdx].mac, ownMAC) == 0);

  if (iAmLeader && nodeRole != ROLE_LEADER) {
    nodeRole = ROLE_LEADER;
    Serial.println("[Discovery] ** I am the LEADER **");
    // Always ensure the server is started — it may have been running in follower mode already.
    if (!_webServerStarted) { wifi_control_setup(); _webServerStarted = true; }
    else                    { wifi_control_promote(); }   // already started as follower → activate APIs
  } else if (!iAmLeader && nodeRole != ROLE_FOLLOWER) {
    nodeRole = ROLE_FOLLOWER;
    Serial.printf("[Discovery] I am a FOLLOWER. Leader: %s\n", candidates[winIdx].mac);
    // Start server even as a follower so browsers get a redirect instead of
    // "connection refused" — prevents "Network heads plugin not available" errors.
    if (!_webServerStarted) { wifi_control_setup_follower(); _webServerStarted = true; }
    else                    { wifi_control_stop(); }
  }
}

// ── Beacon ────────────────────────────────────────────────────────

void discovery_sendBeacon() {
  char pkt[160];
  const char* roleStr = (nodeRole == ROLE_LEADER) ? "LEADER" : "FOLLOWER";
  snprintf(pkt, sizeof(pkt), "MINIHEAD|%s|%s|%d|%s|%s|%s", ownMAC, ownIP, ownFixID, roleStr, ownName, ownMode);
  // Use directed subnet broadcast (e.g. 192.168.178.255) instead of
  // 255.255.255.255 — Fritz!Box and similar routers don't reliably
  // forward limited broadcasts across WiFi→Ethernet boundaries.
  IPAddress _ip = WiFi.localIP(), _mask = WiFi.subnetMask();
  IPAddress _bcast(_ip[0]|(uint8_t)~_mask[0], _ip[1]|(uint8_t)~_mask[1],
                   _ip[2]|(uint8_t)~_mask[2], _ip[3]|(uint8_t)~_mask[3]);
  _beaconUDP.beginPacket(_bcast, BEACON_PORT);
  _beaconUDP.print(pkt); _beaconUDP.endPacket();
}

void discovery_parseBeacon(const char* data, int len) {
  char buf[192];
  if (len >= (int)sizeof(buf)) return;
  memcpy(buf, data, len); buf[len] = 0;
  if (strncmp(buf, "MINIHEAD|", 9) != 0) return;

  // Beacon fields after "MINIHEAD|": MAC|IP|fixID|ROLE|NAME|MODE
  char* fields[7]; int fi = 0;
  char* tok = strtok(buf + 9, "|");
  while (tok && fi < 7) { fields[fi++] = tok; tok = strtok(nullptr, "|"); }
  if (fi < 4) return;

  const char* mac   = fields[0];
  const char* ip    = fields[1];
  int         fixID = atoi(fields[2]);
  NodeRole    role  = (strcmp(fields[3], "LEADER") == 0) ? ROLE_LEADER : ROLE_FOLLOWER;
  const char* name  = (fi >= 5) ? fields[4] : "";
  const char* mode  = (fi >= 6) ? fields[5] : "";

  if (strcmp(mac, ownMAC) == 0) return;

  discovery_addOrUpdate(mac, ip, fixID, role, name, mode);
  if (logCfg.discoveryBeacons)
    Serial.printf("[Discovery] Heard: %s  IP:%s  Fix#%d  %s  \"%s\"  mode:%s\n", mac, ip, fixID, fields[3], name, mode);

  if (nodeRole == ROLE_LEADER && role == ROLE_LEADER)
    discovery_elect();
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void discovery_setup() {
  discovery_loadSettings();

  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(ownMAC, sizeof(ownMAC), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  Serial.printf("[Discovery] MAC: %s  FixID: %d\n", ownMAC, ownFixID);
  _beaconUDP.begin(BEACON_PORT);

  Serial.println("[Discovery] Listening for 4s to find existing nodes...");
  unsigned long listenEnd = millis() + 4000;
  while (millis() < listenEnd) {
    int sz = _beaconUDP.parsePacket();
    if (sz > 0) {
      char buf[128]; int n = _beaconUDP.read(buf, sizeof(buf)-1);
      discovery_parseBeacon(buf, n);
    }
    bool ledOn = ((millis() / 250) % 2 == 0);
    setLED(ledOn?0:0, ledOn?30:0, ledOn?30:0, 0);
    delay(10);
  }
  setLED(0,0,0,0);

  discovery_elect();
  _electionDone = true;

  strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
  discovery_sendBeacon();
  _lastBeaconSent = millis();
}

void discovery_loop() {
  int sz = _beaconUDP.parsePacket();
  if (sz > 0) {
    char buf[128]; int n = _beaconUDP.read(buf, sizeof(buf)-1);
    discovery_parseBeacon(buf, n);
  }

  static unsigned long _leaderGoneAt = 0;
  static bool          _inHold       = false;
  discovery_pruneStale();

  if (discovery_leaderAlive()) {
    _leaderGoneAt = 0; _inHold = false;
  } else if (nodeRole == ROLE_FOLLOWER) {
    if (_leaderGoneAt == 0) {
      _leaderGoneAt = millis(); _inHold = true;
      if (logCfg.discoveryEvents) Serial.println("[Discovery] Leader signal lost — holding...");
    }
    if (_inHold && millis() - _leaderGoneAt > HOLD_DURATION_MS) {
      _inHold = false; _leaderGoneAt = 0;
      if (logCfg.discoveryEvents) Serial.println("[Discovery] Hold expired — re-electing...");
      discovery_elect();
    }
  }

  // Safety net: if we think we're LEADER but a peer with lower MAC is also
  // broadcasting LEADER, yield immediately (handles missed beacon edge cases).
  if (nodeRole == ROLE_LEADER) {
    for (int i = 0; i < peerCount; i++) {
      if (peers[i].active && peers[i].role == ROLE_LEADER &&
          strcmp(peers[i].mac, ownMAC) < 0) {
        if (logCfg.discoveryEvents) Serial.printf("[Discovery] Better leader %s active — yielding\n", peers[i].mac);
        discovery_elect();
        break;
      }
    }
  }

  unsigned long now = millis();
  if (now - _lastBeaconSent >= BEACON_INTERVAL_MS) {
    strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
    discovery_sendBeacon();
    _lastBeaconSent = now;
  }

  if (nodeRole == ROLE_LEADER && _webServerStarted)
    wifi_control_loop();
}

REGISTER_PLUGIN(discovery);
