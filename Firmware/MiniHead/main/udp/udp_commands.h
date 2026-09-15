#pragma once

// ── UDP Commands ──────────────────────────────────────────────────
// UDP sender (leader→follower) and receiver (every node).
// Included by udp_control.h — do not include directly.
// ─────────────────────────────────────────────────────────────────

#include <WiFiUdp.h>
#include "../wifi/log_config.h"
#include "../wifi/discovery_globals.h"

static WiFiUDP _cmdUDP;

// ── Identify watchdog ─────────────────────────────────────────────
static bool          _identifyActive  = false;
static unsigned long _identifyLastMsg = 0;
static uint8_t       _preIdentR=0, _preIdentG=0, _preIdentB=0, _preIdentW=0;
#define IDENTIFY_TIMEOUT_MS 2000

// ── Senders ───────────────────────────────────────────────────────

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


void udp_broadcastCommand(const char* command) {
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active)
      udp_sendCommand(peers[i].ip, peers[i].mac, command);
  }
}

// ── Receiver ─────────────────────────────────────────────────────

void udp_handlePacket(const char* data, int len) {
  char buf[160];
  if (len >= (int)sizeof(buf)) return;
  memcpy(buf, data, len); buf[len] = 0;

  if (strncmp(buf, "IDENTIFY_ON|", 12) == 0) {
    const char* mac = buf + 12;
    if (strcmp(mac, ownMAC) == 0) {
      if (!_identifyActive) {
        if (logCfg.udpVerbose) Serial.println("[UDP] IDENTIFY ON");
        _identifyActive = true;
        _preIdentR = curR; _preIdentG = curG; _preIdentB = curB; _preIdentW = curW;
      }
      _identifyLastMsg = millis();
      setLED(255, 255, 255, 255);
    }
    return;
  }

  if (strncmp(buf, "IDENTIFY_OFF|", 13) == 0) {
    const char* mac = buf + 13;
    if (strcmp(mac, ownMAC) == 0 && _identifyActive) {
      if (logCfg.udpVerbose) Serial.println("[UDP] IDENTIFY OFF");
      _identifyActive = false;
      setLED(_preIdentR, _preIdentG, _preIdentB, _preIdentW);
    }
    return;
  }

  if (strncmp(buf, "CMD|", 4) == 0) {
    char* rest = buf + 4;
    char* pipe = strchr(rest, '|');
    if (!pipe) return;
    *pipe = 0;
    const char* mac = rest;
    const char* cmd = pipe + 1;

    if (strcmp(mac, ownMAC) == 0 || strcmp(mac, "*") == 0) {
      // SETFIXID / SETNAME / SETPATCH are now handled via HTTP /api/config/*
      // Control commands (R:, G:, B:, W:, PAN:, TILT:, etc.) fall through:
      if (logCfg.udpVerbose) Serial.printf("[UDP] CMD for us: %s\n", cmd);
      applyCommand(String(cmd));
    }
    return;
  }
}

// ── Internal lifecycle (called by udp_control.h) ──────────────────

static void _udp_cmds_setup() {
  _cmdUDP.begin(CMD_PORT);
  Serial.printf("[UDP] Listening for commands on port %d\n", CMD_PORT);
}

static void _udp_cmds_loop() {
  int sz = _cmdUDP.parsePacket();
  if (sz > 0) {
    char buf[160];
    int n = _cmdUDP.read(buf, sizeof(buf)-1);
    udp_handlePacket(buf, n);
  }
  if (_identifyActive && (millis() - _identifyLastMsg > IDENTIFY_TIMEOUT_MS)) {
    Serial.println("[UDP] Identify watchdog timeout — restoring LED");
    _identifyActive = false;
    setLED(_preIdentR, _preIdentG, _preIdentB, _preIdentW);
  }
}
