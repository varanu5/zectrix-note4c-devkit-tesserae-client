// fb2bpp.c — pure 2bpp framebuffer helpers (host-testable, no ESP deps)
// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "fb2bpp.h"
#include "board.h"

// 2 bits/pixel, 4 pixels packed per byte. A solid colour c fills every
// 2-bit field: byte = c*0x55  (0x00,0x55,0xAA,0xFF). Order-independent.
void fb_fill(uint8_t *fb, uint8_t color) {
    uint8_t b = (uint8_t)((color & 3) * 0x55);
    for (int i = 0; i < EPD_FB_BYTES; i++) fb[i] = b;
}
