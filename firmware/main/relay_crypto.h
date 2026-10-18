// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RELAY_PRIV_LEN   32
#define RELAY_PUB_LEN    32
#define RELAY_KEY_LEN    32
#define RELAY_NONCE_LEN  12
#define RELAY_TAG_LEN    16

#define RELAY_SEAL_OVERHEAD (RELAY_NONCE_LEN + RELAY_TAG_LEN)

#define RELAY_B64_KEY_CAP 44

bool relay_public_key(uint8_t pub[RELAY_PUB_LEN],
                      const uint8_t priv[RELAY_PRIV_LEN]);

bool relay_keypair_from_seed(uint8_t priv[RELAY_PRIV_LEN],
                             uint8_t pub[RELAY_PUB_LEN],
                             const uint8_t seed[RELAY_PRIV_LEN]);

#ifdef ESP_PLATFORM
bool relay_keypair(uint8_t priv[RELAY_PRIV_LEN], uint8_t pub[RELAY_PUB_LEN]);
#endif

bool relay_derive_key(uint8_t key[RELAY_KEY_LEN],
                      const uint8_t our_priv[RELAY_PRIV_LEN],
                      const uint8_t their_pub[RELAY_PUB_LEN]);

bool relay_unseal(uint8_t *blob, size_t blob_len,
                  const uint8_t key[RELAY_KEY_LEN],
                  uint8_t **frame, size_t *frame_len);

bool relay_b64url_encode(char *out, size_t out_cap,
                         const uint8_t *in, size_t len);

bool relay_b64url_decode(uint8_t *out, size_t out_cap, size_t *out_len,
                         const char *in);
