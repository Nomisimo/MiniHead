#pragma once

// ── Art-Net stubs ─────────────────────────────────────────────────
// Included by config.h when plugins/artnet/artnet.h is NOT active.
// Provides no-op stubs for symbols forward-declared in udp_commands.h.
// ─────────────────────────────────────────────────────────────────

#include <stdint.h>

void artnet_upsertPatch(int fixID, uint16_t universe, uint16_t startAddr) {}
