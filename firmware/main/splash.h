// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t splash_setup(const char *ssid);
esp_err_t splash_waiting(bool registered);
void splash_low_battery(bool low, int mv);

void splash_unpaired(void);
void splash_unpaired_clear(void);
