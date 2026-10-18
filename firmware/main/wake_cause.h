// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include "esp_sleep.h"

static inline esp_sleep_wakeup_cause_t note4c_wakeup_cause(void) {
    uint32_t causes = esp_sleep_get_wakeup_causes();
    // Keep the original timer-first priority when wake sources coincide.
    if (causes & (1UL << ESP_SLEEP_WAKEUP_TIMER)) return ESP_SLEEP_WAKEUP_TIMER;
    if (causes & (1UL << ESP_SLEEP_WAKEUP_GPIO)) return ESP_SLEEP_WAKEUP_GPIO;
    if (causes & (1UL << ESP_SLEEP_WAKEUP_EXT1)) return ESP_SLEEP_WAKEUP_EXT1;
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}
