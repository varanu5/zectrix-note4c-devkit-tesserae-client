// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "framebuf.h"
#include "board.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
static uint8_t *s_frame;
uint8_t *framebuf(void) {
    if (!s_frame) {
        s_frame = heap_caps_malloc(EPD_FB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_ERROR_CHECK(s_frame ? ESP_OK : ESP_ERR_NO_MEM);
    }
    return s_frame;
}
