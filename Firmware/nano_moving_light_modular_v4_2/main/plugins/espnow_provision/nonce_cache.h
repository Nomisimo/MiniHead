#pragma once
#include <stdint.h>
#include <string.h>
#include "espnow_provision_config.h"

// 64-entry ring buffer for 4-byte SEEKER nonces.
// Prevents replay attacks: a captured beacon cannot be re-sent.
// Single-threaded — caller (SENDER task or SEEKER loop) owns it.
struct NonceCache {
    uint32_t slots[PROVISION_NONCE_CACHE];
    uint8_t  head;
    bool     full;

    void init() {
        memset(slots, 0, sizeof(slots));
        head = 0;
        full = false;
    }

    bool contains(uint32_t nonce) const {
        uint8_t limit = full ? (uint8_t)PROVISION_NONCE_CACHE : head;
        for (uint8_t i = 0; i < limit; i++)
            if (slots[i] == nonce) return true;
        return false;
    }

    void insert(uint32_t nonce) {
        slots[head] = nonce;
        if (++head >= PROVISION_NONCE_CACHE) { head = 0; full = true; }
    }
};
