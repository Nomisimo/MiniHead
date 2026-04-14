#pragma once

// ── Discovery (v4) ────────────────────────────────────────────────
// Handles: UDP beacon broadcast + receive, mode election,
//          peer table maintenance, on-the-fly mode switching.
//
// Mode logic (v4 — no ESP self-elects as leader):
//   PC sends beacon with role=LEADER → ESP becomes FOLLOWER
//   No PC leader heard for LEADER_TIMEOUT_MS → ESP becomes STANDALONE
//   STANDALONE → own HTTP server starts
//   FOLLOWER   → HTTP server stops (PC hosts it)
//
// Depends on: core.h (setLED), discovery_globals.h, wifi_control.h
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
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
static bool _webServerStarted = false;

// ── Preferences ───────────────────────────────────────────────────

void discovery_loadFixID() {
  Preferences p; p.begin("discovery", true);
  ownFixID = p.getInt("fixID", 0); p.end();
}

void discovery_saveFixID(int id) {
  ownFixID = id;
  Preferences p; p.begin("discovery", false);
  p.putInt("fixID", id); p.end();
}

void discovery_loadName() {
  Preferences p; p.begin("discovery", true);
  strlcpy(ownName, p.getString("name", "").c_str(), sizeof(ownName)); p.end();
}

void discovery_saveName(const char* name) {
  strlcpy(ownName, name, sizeof(ownName));
  Preferences p; p.begin("discovery", false);
  p.putString("name", ownName); p.end();
}

// ── Peer helpers ──────────────────────────────────────────────────

Peer* discovery_findPeer(const char* mac) {
  for (int i = 0; i < peerCount; i++)
    if (strcmp(peers[i].mac, mac) == 0) return &peers[i];
  return nullptr;
}

Peer* discovery_addOrUpdate(const char* mac, const char* ip, int fixID,
                             NodeRole role, const char* name = "") {
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

bool discovery_pcLeaderAlive() {
  unsigned long now = millis();
  for (int i = 0; i < peerCount; i++)
    if (peers[i].active && peers[i].role == ROLE_LEADER &&
        (now - peers[i].lastSeen < LEADER_TIMEOUT_MS)) return true;
  return false;
}

// ── Mode election ─────────────────────────────────────────────────
// v4: ESP never becomes LEADER.
//     FOLLOWER  if a PC leader is alive.
//     STANDALONE otherwise (runs own HTTP server).

void discovery_elect() {
  bool pcAlive = discovery_pcLeaderAlive();

  if (pcAlive && nodeRole != ROLE_FOLLOWER) {
    nodeRole = ROLE_FOLLOWER;
    if (_webServerStarted) { wifi_control_stop(); _webServerStarted = false; }
    Serial.println("[Discovery] PC Leader detected → FOLLOWER");

  } else if (!pcAlive && nodeRole != ROLE_STANDALONE) {
    nodeRole = ROLE_STANDALONE;
    if (!_webServerStarted) { wifi_control_setup(); _webServerStarted = true; }
    Serial.println("[Discovery] No PC Leader → STANDALONE");
  }
}

// ── Beacon ────────────────────────────────────────────────────────

void discovery_sendBeacon() {
  const char* roleStr = (nodeRole == ROLE_FOLLOWER) ? "FOLLOWER" : "STANDALONE";
  char pkt[160];
  snprintf(pkt, sizeof(pkt), "MINIHEAD|%s|%s|%d|%s|%s",
           ownMAC, ownIP, ownFixID, roleStr, ownName);
  _beaconUDP.beginPacket(IPAddress(255,255,255,255), BEACON_PORT);
  _beaconUDP.print(pkt); _beaconUDP.endPacket();
}

void discovery_parseBeacon(const char* data, int len) {
  char buf[192];
  if (len >= (int)sizeof(buf)) return;
  memcpy(buf, data, len); buf[len] = 0;
  if (strncmp(buf, "MINIHEAD|", 9) != 0) return;

  char* fields[6]; int fi = 0;
  char* tok = strtok(buf + 9, "|");
  while (tok && fi < 6) { fields[fi++] = tok; tok = strtok(nullptr, "|"); }
  if (fi < 4) return;

  const char* mac   = fields[0];
  const char* ip    = fields[1];
  int         fixID = atoi(fields[2]);
  NodeRole    role  = (strcmp(fields[3], "LEADER") == 0) ? ROLE_LEADER : ROLE_FOLLOWER;
  const char* name  = (fi >= 6) ? fields[5] : "";

  if (strcmp(mac, ownMAC) == 0) return;  // ignore own beacon

  discovery_addOrUpdate(mac, ip, fixID, role, name);
  Serial.printf("[Discovery] Heard: %s  IP:%s  Fix#%d  %s  \"%s\"\n",
                mac, ip, fixID, fields[3], name);

  // Immediately re-elect when we hear a PC leader
  if (role == ROLE_LEADER) discovery_elect();
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void discovery_setup() {
  discovery_loadFixID();
  discovery_loadName();

  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(ownMAC, sizeof(ownMAC), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  Serial.printf("[Discovery] MAC: %s  FixID: %d\n", ownMAC, ownFixID);
  _beaconUDP.begin(BEACON_PORT);

  // Listen 4 s for PC leader before deciding mode
  Serial.println("[Discovery] Listening 4s for PC leader...");
  unsigned long listenEnd = millis() + 4000;
  while (millis() < listenEnd) {
    int sz = _beaconUDP.parsePacket();
    if (sz > 0) {
      char buf[128]; int n = _beaconUDP.read(buf, sizeof(buf)-1);
      discovery_parseBeacon(buf, n);
    }
    // Teal blink while scanning
    bool ledOn = ((millis() / 250) % 2 == 0);
    setLED(0, ledOn ? 30 : 0, ledOn ? 30 : 0, 0);
    delay(10);
  }
  setLED(0, 0, 0, 0);

  discovery_elect();   // → STANDALONE or FOLLOWER

  strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
  discovery_sendBeacon();
  _lastBeaconSent = millis();
}

void discovery_loop() {
  // ── Receive beacons ──────────────────────────────────────────────
  int sz = _beaconUDP.parsePacket();
  if (sz > 0) {
    char buf[128]; int n = _beaconUDP.read(buf, sizeof(buf)-1);
    discovery_parseBeacon(buf, n);
  }

  // ── Prune + re-elect on every loop ───────────────────────────────
  discovery_pruneStale();
  discovery_elect();   // idempotent — only acts on actual mode changes

  // ── Send our beacon ──────────────────────────────────────────────
  unsigned long now = millis();
  if (now - _lastBeaconSent >= BEACON_INTERVAL_MS) {
    strncpy(ownIP, WiFi.localIP().toString().c_str(), 15);
    discovery_sendBeacon();
    _lastBeaconSent = now;
  }

  // ── Drive HTTP server + UDP when in STANDALONE mode ──────────────
  if (_webServerStarted)
    wifi_control_loop();
}
// Note: wifi.h owns REGISTER_PLUGIN — discovery is NOT separately registered.
