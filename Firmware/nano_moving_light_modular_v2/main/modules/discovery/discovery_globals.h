#pragma once

#include <Arduino.h>

// ── Types & constants shared across all modules ───────────────────

enum NodeRole { ROLE_UNDECIDED, ROLE_LEADER, ROLE_FOLLOWER };

#define MAX_PEERS          16
#define BEACON_PORT        4210
#define CMD_PORT           4211
#define BEACON_INTERVAL_MS 2000
#define LEADER_TIMEOUT_MS  20000   // peer goes stale after 20s (tolerates brief WiFi gaps)
#define HOLD_DURATION_MS   10000   // hold current state 10s before re-electing as leader

struct Peer {
  char mac[18];
  char ip[16];
  char name[32];
  int  fixID;
  NodeRole role;
  unsigned long lastSeen;
  bool active;
};

// Globals defined in discovery.h, declared here for other modules
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