#pragma once

// ── BLE Provisioning — Config ─────────────────────────────────────
// UUIDs are fixed and unique to MiniHead. Security comes from the
// AES-128-CBC + HMAC-SHA256 encrypted payload, not from UUID obscurity.

#define BLE_PROV_SERVICE_UUID  "e5500000-0000-0000-0000-000000000001"
#define BLE_PROV_CHAR_UUID     "e5500000-0000-0000-0000-000000000002"

#define BLE_SCAN_SECONDS     5       // BLE scan window per attempt (s)
#define PROVISION_TIMEOUT_MS 120000  // SEEKER total wait before giving up (ms)
#define PROVISION_SENDER_MS  30000   // SENDER advertises for this long on boot (ms)
