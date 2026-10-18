// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "relay_crypto.h"
#include "psa/crypto.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_random.h"
#endif

static void wipe(void *data, size_t len) {
    volatile uint8_t *p = data;
    while (len--) *p++ = 0;
}

static psa_status_t import_private(const uint8_t priv[32], psa_key_id_t *key) {
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&attr, 255);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDH);
    psa_status_t rc = psa_import_key(&attr, priv, 32, key);
    psa_reset_key_attributes(&attr);
    return rc;
}

bool relay_public_key(uint8_t pub[32], const uint8_t priv[32]) {
    psa_key_id_t id = 0;
    size_t len = 0;
    psa_status_t rc = psa_crypto_init();
    if (rc == PSA_SUCCESS) rc = import_private(priv, &id);
    if (rc == PSA_SUCCESS) rc = psa_export_public_key(id, pub, 32, &len);
    psa_destroy_key(id);
    if (rc != PSA_SUCCESS || len != 32) { wipe(pub, 32); return false; }
    return true;
}

bool relay_keypair_from_seed(uint8_t priv[32], uint8_t pub[32], const uint8_t seed[32]) {
    memcpy(priv, seed, 32);
    return relay_public_key(pub, priv);
}

#ifdef ESP_PLATFORM
bool relay_keypair(uint8_t priv[32], uint8_t pub[32]) {
    uint8_t seed[32];
    esp_fill_random(seed, sizeof seed);
    bool ok = relay_keypair_from_seed(priv, pub, seed);
    wipe(seed, sizeof seed);
    return ok;
}
#endif

bool relay_derive_key(uint8_t key[32], const uint8_t priv[32], const uint8_t pub[32]) {
    static const uint8_t info[] = "tesserae-relay-frame-key-v1";
    uint8_t shared[32] = {0}; size_t len = 0;
    psa_key_id_t id = 0;
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t rc = psa_crypto_init();
    if (rc == PSA_SUCCESS) rc = import_private(priv, &id);
    if (rc == PSA_SUCCESS) rc = psa_raw_key_agreement(PSA_ALG_ECDH, id, pub, 32, shared, 32, &len);
    uint8_t any = 0;
    for (size_t i = 0; i < sizeof shared; i++) any |= shared[i];
    if (rc == PSA_SUCCESS && (len != 32 || !any)) rc = PSA_ERROR_INVALID_ARGUMENT;
    if (rc == PSA_SUCCESS) rc = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (rc == PSA_SUCCESS) rc = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, NULL, 0);
    if (rc == PSA_SUCCESS) rc = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, shared, 32);
    if (rc == PSA_SUCCESS) rc = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, info, sizeof info - 1);
    if (rc == PSA_SUCCESS) rc = psa_key_derivation_output_bytes(&op, key, 32);
    psa_key_derivation_abort(&op);
    psa_destroy_key(id);
    wipe(shared, sizeof shared);
    if (rc != PSA_SUCCESS) wipe(key, 32);
    return rc == PSA_SUCCESS;
}

bool relay_unseal(uint8_t *blob, size_t len, const uint8_t key[32], uint8_t **frame, size_t *frame_len) {
    if (frame) *frame = NULL;
    if (frame_len) *frame_len = 0;
    if (!blob || !key || len < RELAY_SEAL_OVERHEAD) return false;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t id = 0;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 256);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);
    psa_status_t rc = psa_crypto_init();
    if (rc == PSA_SUCCESS) rc = psa_import_key(&attr, key, 32, &id);
    size_t plain_len = 0;
    uint8_t *ct = blob + RELAY_NONCE_LEN;
    if (rc == PSA_SUCCESS) rc = psa_aead_decrypt(id, PSA_ALG_GCM, blob, RELAY_NONCE_LEN,
        NULL, 0, ct, len - RELAY_NONCE_LEN, ct, len - RELAY_NONCE_LEN, &plain_len);
    psa_destroy_key(id);
    psa_reset_key_attributes(&attr);
    if (rc != PSA_SUCCESS) return false;
    if (frame) *frame = ct;
    if (frame_len) *frame_len = plain_len;
    return true;
}

static const char B64URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

bool relay_b64url_encode(char *out, size_t out_cap,
                         const uint8_t *in, size_t len)
{
    if (out == NULL || (in == NULL && len)) return false;
    size_t need = (len + 2) / 3 * 4;
    if (len % 3 == 1)      need -= 2;
    else if (len % 3 == 2) need -= 1;
    if (out_cap < need + 1) return false;

    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        size_t rem = len - i;
        if (rem > 1) v |= (uint32_t)in[i + 1] << 8;
        if (rem > 2) v |= in[i + 2];
        out[o++] = B64URL[(v >> 18) & 0x3F];
        out[o++] = B64URL[(v >> 12) & 0x3F];
        if (rem > 1) out[o++] = B64URL[(v >> 6) & 0x3F];
        if (rem > 2) out[o++] = B64URL[v & 0x3F];
    }
    out[o] = '\0';
    return true;
}

static int b64url_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;

    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool relay_b64url_decode(uint8_t *out, size_t out_cap, size_t *out_len,
                         const char *in)
{
    if (out_len) *out_len = 0;
    if (out == NULL || in == NULL) return false;

    uint32_t acc = 0;
    int bits = 0;
    size_t o = 0;
    for (const char *p = in; *p; p++) {
        if (*p == '=') break;
        int v = b64url_val(*p);
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= out_cap) return false;
            out[o++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }

    if (bits >= 6) return false;
    if (bits && (acc & ((1u << bits) - 1)) != 0) return false;

    if (out_len) *out_len = o;
    return true;
}
