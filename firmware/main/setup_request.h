// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include <stdbool.h>
#include "display_mode.h"
typedef struct {
    char ssid[33], pass[65], server[160], id[33], pair[7];
    char relay_url[160], relay_code[40];
    bool relay;
    bool has_display_mode;
    display_mode_t display_mode;
} setup_request_t;
// Returns a fixed user-facing error, or NULL after validation.
const char *setup_request_parse(const char *body, const char *nonce,
                               const char *old_ssid, const char *old_pass,
                               const char *old_id, setup_request_t *out);
const char *setup_display_mode_parse(const char *body, const char *nonce, display_mode_t *out);
