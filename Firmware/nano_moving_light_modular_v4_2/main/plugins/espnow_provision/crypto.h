#pragma once
#include <stdint.h>
#include <stddef.h>

// AES-128-CBC + HMAC-SHA256 wrappers around mbedtls (bundled with ESP32 Arduino core).
// All buffers are caller-allocated. No dynamic memory.

// Compute HMAC-SHA256. out_hmac must be 32 bytes.
// key_len should be 32 (the full PROVISION_KEY repeated to 32 bytes).
bool crypto_hmac_sha256(const uint8_t* key,  size_t key_len,
                         const uint8_t* data, size_t data_len,
                         uint8_t* out_hmac);

// Encrypt with AES-128-CBC + PKCS#7 padding.
// cipher must hold at least ((plain_len / 16) + 1) * 16 bytes.
// Returns ciphertext length (always a multiple of 16), or 0 on error.
size_t crypto_aes128_cbc_encrypt(const uint8_t* key16, const uint8_t* iv16,
                                  const uint8_t* plain,  size_t plain_len,
                                  uint8_t* cipher);

// Decrypt AES-128-CBC and strip PKCS#7 padding.
// plain must hold at least cipher_len bytes.
// Returns plaintext length (padding stripped), or 0 on error.
size_t crypto_aes128_cbc_decrypt(const uint8_t* key16, const uint8_t* iv16,
                                  const uint8_t* cipher, size_t cipher_len,
                                  uint8_t* plain);
