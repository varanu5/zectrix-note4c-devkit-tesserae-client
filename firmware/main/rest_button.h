// rest_button.h — pure helpers for the /frame button query (host-testable).
// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "power.h"

// Short front presses check conditionally; a refresh hold requests rendering.
static inline const char *rest_button_action(button_action_t action) {
    if (action == BUTTON_REFRESH) return "refresh";
    return action == BUTTON_NEXT ? "right" : NULL;
}

// True when this button asks the server to re-render the CURRENT page, so the
// firmware must drop If-None-Match to force a 200 repaint. Only "refresh" does.
// Navigation buttons ("right", ...) keep the ETag: a page change returns 200
// naturally and an unchanged frame 304s (battery-friendly, like a plain wake).
static inline bool rest_button_clears_etag(const char *button) {
    return button != NULL && strcmp(button, "refresh") == 0;
}

// Append "?button=<button>&button_event_id=<event_id>" to the /frame URL already
// in buf (current length len, buffer capacity cap). Returns the new length, or
// len unchanged when button is NULL/empty or the append would not fit; on a
// no-fit the buffer is left NUL-terminated at len.
static inline size_t rest_button_query(char *buf, size_t len, size_t cap,
                                       const char *button, uint32_t event_id) {
    if (button == NULL || button[0] == '\0' || len >= cap) return len;
    int n = snprintf(buf + len, cap - len, "?button=%s&button_event_id=%u",
                     button, (unsigned)event_id);
    if (n <= 0 || (size_t)n >= cap - len) { buf[len] = '\0'; return len; }
    return len + (size_t)n;
}
