#pragma once

// ── Discovery Module ──────────────────────────────────────────────
// Handles: UDP beacon broadcast + receive, leader election,
//          peer table maintenance, on-the-fly role promotion
// Depends on: core.h (setLED), discovery_globals.h
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "discovery_globals.h"
#include "../wifi_control/wifi_control.h"  // ← add this

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

// Forward declaration — wifi_control will define this when it's the leader
#include "../wifi_control/wifi_control.h"
static bool _webServerStarted = false;

// ── Preferences ───────────────────────────────────────────────────

void discovery_loadFixID() {
  Preferences p;
  p.begin("discovery", true);
  ownFixID = p.getInt("fixID", 0);
  p.end();
}

void discovery_saveFixID(int id) {
  ownFixID = id;
  Preferences p;
  p.begin("discovery", false);
  p.putInt("fixID", id);
  p.end();
}

void discovery_loadName() {
  Preferences p;
  p.begin("discovery", true);
  strlcpy(ownName, p.getString("name", "").c_str(), sizeof(ownName));
  p.end();
}

void discovery_saveName(const char* name) {
  strlcpy(ownName, name, sizeof(ownName));
  Preferences p;
  p.begin("discovery", false);
  p.putString("name", ownName);
  p.end();
}

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
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && (now - peers[i].lastSeen > LEADER_TIMEOUT_MS))
      peers[i].active = false;
  }
}

bool discovery_leaderAlive() {
  unsigned long now = millis();
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active &&
        peers[i].role == ROLE_LEADER &&
        strcmp(peers[i].mac, ownMAC) != 0 &&
        (now - peers[i].lastSeen < LEADER_TIMEOUT_MS))
      return true;
  }
  return false;
}

// ── Election ──────────────────────────────────────────────────────
// Winner = lowest FixID among active peers (0/unassigned excluded).
// Tiebreaker or all-unassigned: lowest MAC string (RDM-style).

void discovery_elect() {
  // Winner = lowest MAC string (RDM-style).
  // PC always wins because its fake MAC "00:00:00:00:00:PC" is lexicographically
  // smaller than any real ESP32 MAC.
  struct Candidate { char mac[18]; };
  Candidate candidates[MAX_PEERS + 1];
  int n = 0;

  // Self
  strncpy(candidates[n].mac, ownMAC, 17);
  candidates[n].mac[17] = 0;
  n++;

  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].active) continue;
    strncpy(candidates[n].mac, peers[i].mac, 17);
    candidates[n].mac[17] = 0;
    n++;
  }

  // Find winner: lowest MAC
  int winIdx = 0;
  for (int i = 1; i < n; i++) {
    if (strcmp(candidates[i].mac, candidates[winIdx].mac) < 0) winIdx = i;
  }

  bool iAmLeader = (strcmp(candidates[winIdx].mac, ownMAC) == 0);

  if (iAmLeader && nodeRole != ROLE_LEADER) {
    nodeRole = ROLE_LEADER;
    Serial.println("[Discovery] ** I am the LEADER **");
    if (!_webServerStarted) {
      wifi_control_setup();
      _webServerStarted = true;
    }
  } else if (!iAmLeader && nodeRole != ROLE_FOLLOWER) {
    nodeRole = ROLE_FOLLOWER;
    if (_webServerStarted) {
      wifi_control_stop();
      _webServerStarted = false;  // allow restart if we become leader again
    }
    Serial.printf("[Discovery] I am a FOLLOWER. Leader: %s\n", candidates[winIdx].mac);
  }
}

// ── Beacon ────────────────────────────────────────────────────────

void discovery_sendBeacon() {
  // Format: MINIHEAD|<MAC>|<IP>|<FixID>|<ROLE>|<NAME>
  char pkt[160];
  const char* roleStr = (nodeRole == ROLE_LEADER) ? "LEADER" : "FOLLOWER";
  snprintf(pkt, sizeof(pkt), "MINIHEAD|%s|%s|%d|%s|%s", ownMAC, ownIP, ownFixID, roleStr, ownName);
  _beaconUDP.beginPacket(IPAddress(255,255,255,255), BEACON_PORT);
  _beaconUDP.print(pkt);
  _beaconUDP.endPacket();
}

void discovery_parseBeacon(const char* data, int len) {
  // MINIHEAD|MAC|IP|FixID|ROLE|NAME  (NAME field optional — backwards compat)
  char buf[192];
  if (len >= (int)sizeof(buf)) return;
  memcpy(buf, data, len); buf[len] = 0;

  if (strncmp(buf, "MINIHEAD|", 9) != 0) return;

  char* fields[6];
  int   fi = 0;
  char* tok = strtok(buf + 9, "|");
  while (tok && fi < 6) { fields[fi++] = tok; tok = strtok(nullptr, "|"); }
  if (fi < 4) return;

  const char* mac   = fields[0];
  const char* ip    = fields[1];
  int         fixID = atoi(fields[2]);
  NodeRole    role  = (strcmp(fields[3], "LEADER") == 0) ? ROLE_LEADER : ROLE_FOLLOWER;
  const char* name  = (fi >= 6) ? fields[5] : "";

  // Ignore own beacon
  if (strcmp(mac, ownMAC) == 0) return;

  discovery_addOrUpdate(mac, ip, fixID, role, name);
  Serial.printf("[Discovery] Heard: %s  IP:%s  Fix#%d  %s  \"%s\"\n", mac, ip, fixID, fields[3], name);

  // If we are LEADER and heard another LEADER, re-elect immediately
  // (step-down is beacon-driven, not timer-driven)
  if (nodeRole == ROLE_LEADER && role == ROLE_LEADER) {
    discovery_elect();
  }
}

// ── Module lifecycle ──────────────────────────────────────────────

void discovery_setup() {
  discovery_loadFixID();
  discovery_loadName();

  // Get MAC
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(ownMAC, sizeof(ownMAC), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  // Get IP (WiFi already connected by wifi_control or directly)
 // strncpy(ownIP, WiFi.localIP().toString().c_str(), 15); --> mby delet later, seems to cause issues if called before WiFi is connected. Will refresh IP before sending first beacon instead.

  Serial.printf("[Discovery] MAC: %s  FixID: %d\n", ownMAC, ownFixID);

  _beaconUDP.begin(BEACON_PORT);

  // ── Startup listen window (4 seconds) ─────────────────────────
  Serial.println("[Discovery] Listening for 4s to find existing nodes...");
  unsigned long listenEnd = millis() + 4000;
  while (millis() < listenEnd) {
    int sz = _beaconUDP.parsePacket();
    if (sz > 0) {
      char buf[128];
      int n = _beaconUDP.read(buf, sizeof(buf)-1);
      discovery_parseBeacon(buf, n);
    }
    // Flash cyan during listen
    bool ledOn = ((millis() / 250) % 2 == 0);
    setLED(ledOn?0:0, ledOn?30:0, ledOn?30:0, 0);
    delay(10);
  }
  setLED(0,0,0,0);

  // ── Initial election ──────────────────────────────────────────
  discovery_elect();
  _electionDone = true;

  // Send first beacon (with IP)
  strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
  discovery_sendBeacon();
  _lastBeaconSent = millis();
}

void discovery_loop() {
  // ── Receive beacons ──────────────────────────────────────────
  int sz = _beaconUDP.parsePacket();
  if (sz > 0) {
    char buf[128];
    int n = _beaconUDP.read(buf, sizeof(buf)-1);
    discovery_parseBeacon(buf, n);
  }

  // ── Prune stale peers & step-up with hold ────────────────────
  // Step-DOWN : beacon-driven (discovery_parseBeacon).
  // Step-UP   : only after peer stale (LEADER_TIMEOUT_MS) +
  //             10s hold where ESP keeps last state, then re-elects.
  static unsigned long _leaderGoneAt = 0;
  static bool          _inHold       = false;
  discovery_pruneStale();

  if (discovery_leaderAlive()) {
    _leaderGoneAt = 0;
    _inHold       = false;
  } else if (nodeRole == ROLE_FOLLOWER) {
    if (_leaderGoneAt == 0) {
      _leaderGoneAt = millis();
      _inHold       = true;
      Serial.println("[Discovery] Leader signal lost — holding...");
    }
    if (_inHold && millis() - _leaderGoneAt > HOLD_DURATION_MS) {
      _inHold = false;
      Serial.println("[Discovery] Hold expired — re-electing...");
      _leaderGoneAt = 0;
      discovery_elect();
    }
  }

  // ── Broadcast own beacon ──────────────────────────────────────
  unsigned long now = millis();
  if (now - _lastBeaconSent >= BEACON_INTERVAL_MS) {
    // Refresh own IP in case it changed
    strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
    discovery_sendBeacon();
    _lastBeaconSent = now;
  }

  // ── If leader, run web server loop ───────────────────────────
  if (nodeRole == ROLE_LEADER && _webServerStarted) {
    wifi_control_loop();
  }
}
