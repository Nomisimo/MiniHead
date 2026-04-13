#pragma once

// ── UDP Control Plugin ────────────────────────────────────────────
// Follower: listens on port 4211 for CMD / IDENTIFY packets
// Leader:   sends CMD / IDENTIFY packets to follower IPs
// Depends on: core.h (setLED, applyCommand), discovery_globals.h
// ─────────────────────────────────────────────────────────────────

#include <WiFiUdp.h>
#include "discovery_globals.h"

// Forward decl: artnet_upsertPatch is defined in artnet_control.h,
// which is included after this file in wifi_control.h.
void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr);

static WiFiUDP _cmdUDP;

// ── Identify watchdog ─────────────────────────────────────────────
static bool          _identifyActive  = false;
static unsigned long _identifyLastMsg = 0;
static uint8_t       _preIdentR=0, _preIdentG=0, _preIdentB=0, _preIdentW=0;
#define IDENTIFY_TIMEOUT_MS 2000

// ── Sender (called from leader / wifi_control handlers) ───────────

void udp_sendCommand(const char* ip, const char* targetMAC, const char* command) {
  char pkt[160];
  snprintf(pkt, sizeof(pkt), "CMD|%s|%s", targetMAC, command);
  _cmdUDP.beginPacket(ip, CMD_PORT);
  _cmdUDP.print(pkt);
  _cmdUDP.endPacket();
}

void udp_sendIdentifyOn(const char* ip, const char* targetMAC) {
  char pkt[64];
  snprintf(pkt, sizeof(pkt), "IDENTIFY_ON|%s", targetMAC);
  _cmdUDP.beginPacket(ip, CMD_PORT);
  _cmdUDP.print(pkt);
  _cmdUDP.endPacket();
}

void udp_sendIdentifyOff(const char* ip, const char* targetMAC) {
  char pkt[64];
  snprintf(pkt, sizeof(pkt), "IDENTIFY_OFF|%s", targetMAC);
  _cmdUDP.beginPacket(ip, CMD_PORT);
  _cmdUDP.print(pkt);
  _cmdUDP.endPacket();
}

void udp_sendSetName(const char* ip, const char* targetMAC, const char* name) {
  char pkt[96];
  snprintf(pkt, sizeof(pkt), "CMD|%s|SETNAME:%s", targetMAC, name);
  _cmdUDP.beginPacket(ip, CMD_PORT);
  _cmdUDP.print(pkt);
  _cmdUDP.endPacket();
}

// ── Broadcast to all active peers ────────────────────────────────

void udp_broadcastCommand(const char* command) {
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active)
      udp_sendCommand(peers[i].ip, peers[i].mac, command);
  }
}

// ── Receiver (runs on every node) ────────────────────────────────

void udp_handlePacket(const char* data, int len) {
  char buf[160];
  if (len >= (int)sizeof(buf)) return;
  memcpy(buf, data, len); buf[len] = 0;

  // ── IDENTIFY_ON ──────────────────────────────────────────────
  if (strncmp(buf, "IDENTIFY_ON|", 12) == 0) {
    const char* mac = buf + 12;
    if (strcmp(mac, ownMAC) == 0) {
      if (!_identifyActive) {
        Serial.println("[UDP] IDENTIFY ON");
        _identifyActive = true;
        _preIdentR = curR; _preIdentG = curG; _preIdentB = curB; _preIdentW = curW;
      }
      _identifyLastMsg = millis();
      setLED(255, 255, 255, 255);
    }
    return;
  }

  // ── IDENTIFY_OFF ─────────────────────────────────────────────
  if (strncmp(buf, "IDENTIFY_OFF|", 13) == 0) {
    const char* mac = buf + 13;
    if (strcmp(mac, ownMAC) == 0 && _identifyActive) {
      Serial.println("[UDP] IDENTIFY OFF");
      _identifyActive = false;
      setLED(_preIdentR, _preIdentG, _preIdentB, _preIdentW);
    }
    return;
  }

  // ── CMD ──────────────────────────────────────────────────────
  if (strncmp(buf, "CMD|", 4) == 0) {
    char* rest = buf + 4;
    char* pipe = strchr(rest, '|');
    if (!pipe) return;
    *pipe = 0;
    const char* mac = rest;
    const char* cmd = pipe + 1;

    // Accept if addressed to us or broadcast "*"
    if (strcmp(mac, ownMAC) == 0 || strcmp(mac, "*") == 0) {
      if (strncmp(cmd, "SETFIXID:", 9) == 0) {
        int newID = atoi(cmd + 9);
        discovery_saveFixID(newID);
        Serial.printf("[UDP] FixID set to %d\n", newID);
        return;
      }
      if (strncmp(cmd, "SETNAME:", 8) == 0) {
        discovery_saveName(cmd + 8);
        Serial.printf("[UDP] Name set to \"%s\"\n", ownName);
        return;
      }
      if (strncmp(cmd, "SETPATCH:", 9) == 0) {
        int fixID = 0, uni = 0, addr = 1;
        sscanf(cmd + 9, "%d,%d,%d", &fixID, &uni, &addr);
        artnet_upsertPatch(fixID, (uint16_t)uni, (uint16_t)addr);
        Serial.printf("[UDP] SETPATCH: Fix#%d U%d @%d\n", fixID, uni, addr);
        return;
      }
      Serial.printf("[UDP] CMD for us: %s\n", cmd);
      applyCommand(String(cmd));
    }
    return;
  }
}

// ── Plugin lifecycle ──────────────────────────────────────────────

void udp_control_setup() {
  _cmdUDP.begin(CMD_PORT);
  Serial.printf("[UDP] Listening for commands on port %d\n", CMD_PORT);
}

void udp_control_loop() {
  // Receive
  int sz = _cmdUDP.parsePacket();
  if (sz > 0) {
    char buf[160];
    int n = _cmdUDP.read(buf, sizeof(buf)-1);
    udp_handlePacket(buf, n);
  }

  // Identify watchdog
  if (_identifyActive && (millis() - _identifyLastMsg > IDENTIFY_TIMEOUT_MS)) {
    Serial.println("[UDP] Identify watchdog timeout — restoring LED");
    _identifyActive = false;
    setLED(_preIdentR, _preIdentG, _preIdentB, _preIdentW);
  }
}

REGISTER_PLUGIN(udp_control);
