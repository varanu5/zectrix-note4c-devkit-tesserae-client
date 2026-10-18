// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "splash.h"
#include "screen.h"
#include "framebuf.h"
#include "display_update.h"
#include "esp_attr.h"
#include "esp_log.h"

RTC_DATA_ATTR static bool s_low_painted;
RTC_DATA_ATTR static bool s_removed_painted;
esp_err_t splash_setup(const char *ssid) {
    s_removed_painted = false;
    if (!screen_setup(framebuf(),ssid)) return ESP_ERR_INVALID_SIZE;
    return display_present_selected(framebuf());
}
esp_err_t splash_waiting(bool registered) {
    if (!screen_waiting(framebuf(),registered)) return ESP_ERR_INVALID_SIZE;
    return display_present_selected(framebuf());
}
void splash_low_battery(bool low,int mv) {
    if (!low) { s_low_painted=false; return; }
    s_removed_painted = false;
    if (s_low_painted || mv<2500 || mv>4500) return;
    if (screen_low_battery(framebuf())) s_low_painted=display_present_selected(framebuf())==ESP_OK;
    if (!s_low_painted) ESP_LOGW("splash","charge screen pending; retry on a later battery check");
}

void splash_relay_removed(void) {
    if (s_removed_painted) return;
    if (screen_relay_removed(framebuf()))
        s_removed_painted = display_present_selected(framebuf()) == ESP_OK;
    if (!s_removed_painted) ESP_LOGW("splash", "unpaired screen pending; retry next wake");
}
