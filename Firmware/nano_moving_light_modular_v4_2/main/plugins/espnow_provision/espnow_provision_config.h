#pragma once

// ── ESP-NOW Provisioning — Timing & Channel Config ────────────────
//
// PROVISION_CHANNEL must match the WiFi channel of the AP the SENDER
// device is connected to. Most home routers default to channel 1 or 6.
// Check your router's settings if provisioning doesn't work.
// All SEEKER devices send on this channel.
//
// Set PROVISION_KEY in config.h (32 hex chars = 16 bytes).

#define PROVISION_CHANNEL      1      // WiFi channel for ESP-NOW (must match AP)
#define PROVISION_BEACON_MS    2000   // how often SEEKER sends a beacon (ms)
#define PROVISION_TIMEOUT_MS   30000  // SEEKER/SENDER gives up after this (ms)
#define PROVISION_NONCE_CACHE  64     // replay protection ring-buffer size
