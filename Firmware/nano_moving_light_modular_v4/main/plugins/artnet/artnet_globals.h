#pragma once

// ── Art-Net / DMX512 Globals ──────────────────────────────────────
// Shared structs, constants, and extern declarations for all
// artnet sub-modules.
// ─────────────────────────────────────────────────────────────────

#include <Arduino.h>

#define ARTNET_PORT        6454
#define DMX_CHANNELS       512
#define DMX_FOOTPRINT      7        // channels per fixture
#define MAX_PATCHES        32
#define ARTNET_TIMEOUT_MS  3000
#define ARTNET_NVS_VERSION 1

// ── Per-fixture patch record ─────────────────────────────────────
struct ArtnetPatch {
  int      fixID;       // fixture ID (matches ownFixID or peers[i].fixID)
  uint16_t universe;    // Art-Net Port-Address 0–32767
  uint16_t startAddr;   // DMX start address 1–512 (1-based)
};

// ── DMX channel offsets within the 7-ch footprint ────────────────
#define CH_MASTER  0   // virtual master dimmer — scales R/G/B/W
#define CH_RED     1
#define CH_GREEN   2
#define CH_BLUE    3
#define CH_WHITE   4
#define CH_PAN     5   // 0-255 → 0-180°
#define CH_TILT    6   // 0-255 → 0-180°

// ── Global state — defined in artnet_receiver.h ───────────────────
extern ArtnetPatch   artnetPatches[MAX_PATCHES];
extern int           artnetPatchCount;
extern bool          artnetActive;
extern unsigned long artnetLastPacket;
extern uint8_t       artnetDmxBuf[DMX_CHANNELS]; // public for future dmx_output plugin
