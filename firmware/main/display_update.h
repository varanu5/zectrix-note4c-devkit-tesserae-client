// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include "esp_err.h"
#include <stdint.h>

// Invalidate the stored image reference before touching the panel and latch failures.
esp_err_t display_present(const uint8_t *frame);
// Use the saved refresh mode for content and built-in screens.
esp_err_t display_present_selected(const uint8_t *frame);
