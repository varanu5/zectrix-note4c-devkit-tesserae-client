// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include "esp_system.h"   // esp_reset_reason_t
#include <stdbool.h>
#include <stdint.h>

bool relay_ready(void);            // holds a complete mailbox identity + frame key
bool relay_pairing_pending(void);  // configured but not yet paired
bool relay_connection_ok(void);

typedef enum {
    RELAY_PAIR_IDLE = 0,   // nothing to do (no code, or already paired)
    RELAY_PAIR_WAITING,    // submitted; home has not completed it yet
    RELAY_PAIR_DONE,       // key derived and persisted; polling can start
    RELAY_PAIR_EXPIRED,    // code unknown/expired
    RELAY_PAIR_ERROR,      // transient (network/parse); safe to retry
} relay_pair_result_t;

relay_pair_result_t relay_pair_step(void);

int relay_run_loop(const char *button, uint32_t button_event_id);

void relay_end_wake(void);
void relay_begin_wake(bool deep_sleep);
const char *relay_setup_error(void);

bool relay_poll_frame(void);

const uint8_t *relay_pending_frame(void);   // framebuf() if new this wake, else NULL
void relay_frame_painted(void);             // commit the relay ETag after a successful paint

bool relay_pairing_revoked(void);

void relay_forget_revoked_pairing(void);
