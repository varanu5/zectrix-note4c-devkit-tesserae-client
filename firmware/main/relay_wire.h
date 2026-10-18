// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "relay_crypto.h"

typedef enum {
    RELAY_READY_PENDING = 0,
    RELAY_READY_OK,
    RELAY_READY_MALFORMED,
} relay_ready_t;

typedef struct {
    char    install_id[64];
    char    device_id[64];
    char    device_token[256];
    uint8_t home_pub[RELAY_PUB_LEN];
} relay_pairing_t;

relay_ready_t relay_parse_ready(const char *json, size_t len,
                                relay_pairing_t *out);

bool relay_build_pair_body(char *out, size_t cap,
                           const char *code, const char *pubkey_b64,
                           int panel_w, int panel_h,
                           const char *model, const char *gamut);

bool relay_build_status_body(char *out, size_t cap,
                             const char *device_id, const char *fw_version,
                             int panel_w, int panel_h,
                             const char *ip, int rssi,
                             int battery_mv, int battery_pct,
                             const char *button, uint64_t button_event_id);

bool relay_parse_config_etag(const char *json, size_t len,
                             char *out, size_t cap);

typedef struct {
    int32_t sleep_interval_s;
    int32_t button_wake_s;
    int8_t  always_on;
} relay_devcfg_t;

bool relay_parse_config(const char *json, size_t len, relay_devcfg_t *out);

bool relay_mailbox_url(char *out, size_t cap, const char *base,
                       const char *install_id, const char *device_id,
                       const char *leaf);
