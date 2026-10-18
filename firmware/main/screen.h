// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool screen_setup(uint8_t *fb,const char *ssid);
bool screen_low_battery(uint8_t *fb);
bool screen_waiting(uint8_t *fb,bool registered);

bool screen_relay_removed(uint8_t *fb);
