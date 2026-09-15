#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "mbedtls/md.h"
#include "mbedtls/aes.h"

// AES-128-CBC + HMAC-SHA256 wrappers around mbedtls.
// Implementations are inline here so Arduino compiles them as part of
// the single sketch TU — Arduino does not compile .cpp files in subdirectories.

static inline bool crypto_hmac_sha256(const uint8_t* key,  size_t key_len,
                                       const uint8_t* data, size_t data_len,
                                       uint8_t* out_hmac) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) { mbedtls_md_free(&ctx); return false; }
    if (mbedtls_md_setup(&ctx, info, 1)                    != 0) { mbedtls_md_free(&ctx); return false; }
    if (mbedtls_md_hmac_starts(&ctx, key, key_len)         != 0) { mbedtls_md_free(&ctx); return false; }
    if (mbedtls_md_hmac_update(&ctx, data, data_len)       != 0) { mbedtls_md_free(&ctx); return false; }
    int rc = mbedtls_md_hmac_finish(&ctx, out_hmac);
    mbedtls_md_free(&ctx);
    return rc == 0;
}

static inline size_t crypto_aes128_cbc_encrypt(const uint8_t* key16, const uint8_t* iv16,
                                                 const uint8_t* plain,  size_t plain_len,
                                                 uint8_t* cipher) {
    static uint8_t padded[160];
    uint8_t pad = (uint8_t)(16 - (plain_len % 16));
    size_t  padded_len = plain_len + pad;
    if (padded_len > sizeof(padded)) return 0;
    memcpy(padded, plain, plain_len);
    memset(padded + plain_len, pad, pad);

    uint8_t iv[16];
    memcpy(iv, iv16, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int rc = mbedtls_aes_setkey_enc(&aes, key16, 128);
    if (rc == 0)
        rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv, padded, cipher);
    mbedtls_aes_free(&aes);
    return (rc == 0) ? padded_len : 0;
}

static inline size_t crypto_aes128_cbc_decrypt(const uint8_t* key16, const uint8_t* iv16,
                                                 const uint8_t* cipher, size_t cipher_len,
                                                 uint8_t* plain) {
    if (cipher_len == 0 || cipher_len % 16 != 0) return 0;

    uint8_t iv[16];
    memcpy(iv, iv16, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int rc = mbedtls_aes_setkey_dec(&aes, key16, 128);
    if (rc == 0)
        rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipher_len, iv, cipher, plain);
    mbedtls_aes_free(&aes);
    if (rc != 0) return 0;

    uint8_t pad_byte = plain[cipher_len - 1];
    if (pad_byte == 0 || pad_byte > 16) return 0;
    for (size_t i = cipher_len - pad_byte; i < cipher_len; i++)
        if (plain[i] != pad_byte) return 0;
    return cipher_len - pad_byte;
}

// Parses a 32-char hex PROVISION_KEY into a 16-byte AES key and 32-byte HMAC key
// (HMAC = AES key repeated twice). Caches the result via the parsed flag.
static inline bool crypto_parse_provision_key(const char* hex,
                                               uint8_t keyAes[16],
                                               uint8_t keyHmac[32],
                                               bool&   parsed) {
    if (parsed) return true;
    if (strlen(hex) != 32) return false;
    for (int i = 0; i < 16; i++) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(hex[i * 2]), lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        keyAes[i] = (uint8_t)((hi << 4) | lo);
    }
    memcpy(keyHmac,      keyAes, 16);
    memcpy(keyHmac + 16, keyAes, 16);
    parsed = true;
    return true;
}
