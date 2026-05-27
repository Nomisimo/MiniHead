#pragma once

// ── Discovery stubs ───────────────────────────────────────────────
// Included by config.h when plugins/udp_control/udp_control.h is
// NOT active. Provides default definitions for all symbols declared
// as extern in discovery_globals.h and forward-declared in
// wifi_control.h — so the firmware compiles without UDP/discovery.
// ─────────────────────────────────────────────────────────────────

#include "../wifi/discovery_globals.h"

// Global definitions (normally in udp_control/discovery.h)
NodeRole nodeRole   = ROLE_UNDECIDED;
char     ownMAC[18] = "";
char     ownIP[16]  = "";
char     ownMode[8] = "UDP";
int      ownFixID   = 0;
char     ownName[32]= "";
Peer     peers[MAX_PEERS];
int      peerCount  = 0;

// No-op save helpers (normally in udp_control/discovery.h)
void discovery_saveFixID(int id)          { ownFixID = id; }
void discovery_saveName(const char* name) { strlcpy(ownName, name, sizeof(ownName)); }

// No-op UDP senders (normally in udp_control/udp_commands.h)
void udp_sendCommand(const char* ip, const char* mac, const char* cmd) {}
void udp_broadcastCommand(const char* cmd) {}
void udp_sendSetName(const char* ip, const char* mac, const char* name) {}
void udp_sendIdentifyOn(const char* ip, const char* mac) {}
void udp_sendIdentifyOff(const char* ip, const char* mac) {}
