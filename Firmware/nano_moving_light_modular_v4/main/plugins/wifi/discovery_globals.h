#pragma once

#include <Arduino.h>

// ── Types & constants shared across wifi plugin ───────────────────

enum NodeRole {
  ROLE_UNDECIDED,
  ROLE_AP_PORTAL,   // WiFi failed → running captive-portal AP
  ROLE_STANDALONE,  // WiFi OK, no PC leader found → own web server
  ROLE_FOLLOWER,    // PC leader alive → receiving UDP commands
  ROLE_LEADER       // reserved (no ESP ever self-elects in v4)
};

#define MAX_PEERS          16
#define BEACON_PORT        4210
#define CMD_PORT           4211
#define BEACON_INTERVAL_MS 2000
#define LEADER_TIMEOUT_MS  8000    // (was 20s) — switch to standalone faster
#define HOLD_DURATION_MS   5000    // (was 10s)

struct Peer {
  char mac[18];
  char ip[16];
  char name[32];
  int  fixID;
  NodeRole role;
  unsigned long lastSeen;
  bool active;
};

// Globals defined in discovery.h, declared here for other files
extern NodeRole nodeRole;
extern char     ownMAC[18];
extern char     ownIP[16];
extern int      ownFixID;
extern char     ownName[32];
extern Peer     peers[MAX_PEERS];
extern int      peerCount;

// Defined in discovery.h
extern void discovery_saveFixID(int id);
extern void discovery_saveName(const char* name);
