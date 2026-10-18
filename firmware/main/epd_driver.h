// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "diag.h"
#include "display_mode.h"
// Optional failure output is NONE on success or invalid input.
esp_err_t epd_present(const uint8_t *frame, diag_paint_t *failure);
esp_err_t epd_present_mode(const uint8_t *frame, display_mode_t mode, diag_paint_t *failure);
