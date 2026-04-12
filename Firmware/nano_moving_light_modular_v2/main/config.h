// ── Module Registry ───────────────────────────────────────────────
// Add or remove modules here. Each entry = one .h file.
// The module must implement: NAME_setup() and NAME_loop()
// ─────────────────────────────────────────────────────────────────

#pragma once

// ── Core (always included, not a module) ─────────────────────────
#include "core.h"

// ── !! CHANGE THESE !! ───────────────────────────────────────────
#define WIFI_SSID     "TP-LINK_4A2CEC"
#define WIFI_PASSWORD "98764013"
// ─────────────────────────────────────────────────────────────────


// ── Module includes ───────────────────────────────────────────────
// ORDER IS CRITICAL:
// 1. wifi_control first — defines wifi_control_setup/loop
//    so discovery.h can forward-call them
#include "modules/discovery/discovery_globals.h"
#include "modules/wifi_control/wifi_control.h"
#include "modules/discovery/discovery.h" 
#include "modules/udp_control/udp_control.h"
#include "modules/startup_animation.h"
#include "modules/udp_control/udp_control.h"
#include "modules/startup_animation.h"

// ── Module table ─────────────────────────────────────────────────
// startup_animation: runs boot sequence
// udp_control:       listens for commands on EVERY node

// discovery:         beacons + election + starts wifi_control if leader
//
// wifi_control is NOT here — discovery.h starts it on-the-fly
// when this node wins the election

typedef void (*ModuleFn)();

struct Module {
  ModuleFn setup;
  ModuleFn loop;
};

Module modules[] = {
  { startup_animation_setup, startup_animation_loop },
  { udp_control_setup,       udp_control_loop       },
  { discovery_setup,         discovery_loop         },
};

const int MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);
