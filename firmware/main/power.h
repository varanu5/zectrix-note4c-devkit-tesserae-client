// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { BUTTON_NONE, BUTTON_NEXT, BUTTON_CHECK, BUTTON_SETUP, BUTTON_REFRESH } button_action_t;
void power_init(void);
void power_handle_charge_wakeup(void);
void power_log_wakeup(void);
void power_measure_battery(void);
int power_battery_mv(void);
int power_battery_pct(int mv);
bool power_battery_low(void);
button_action_t power_boot_action(void);
void power_sleep(uint32_t seconds, bool scheduled) __attribute__((noreturn));
