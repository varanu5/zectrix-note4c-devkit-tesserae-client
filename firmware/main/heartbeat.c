// heartbeat.c — build the device status JSON.
// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "heartbeat.h"
#include "defaults.h"
#include "board.h"
#include "power.h"
#include "wifi_manager.h"

#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "wake_cause.h"

static const char *TAG = "heartbeat";

static const char *wake_reason_str(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "poweron";
        case ESP_RST_SW:        return "sw";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "int_wdt";
        case ESP_RST_TASK_WDT:  return "task_wdt";
        case ESP_RST_WDT:       return "wdt";
        case ESP_RST_DEEPSLEEP: return note4c_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER ? "timer" : "button";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_USB:       return "usb";
        default:                return "unknown";
    }
}

void heartbeat_json(char *dst, size_t dst_sz, int sleep_interval_s,
                    esp_reset_reason_t reset_reason,
                    const char *button, uint32_t button_event_id) {
    if (!dst || dst_sz < 3) return;   // need room for at least "{}" + NUL
    int mv = power_battery_mv();
    int pct = power_battery_pct(mv);   // reuse the one reading (single ADC read + log)
    int rssi = wifi_rssi();
    char ip[16] = {0};
    wifi_get_sta_ip(ip, sizeof(ip));
    char battery[80] = "";
    if (mv >= 0 && pct >= 0)
        snprintf(battery, sizeof battery, "\"battery_mv\":%d,\"battery_pct\":%d,", mv, pct);

    // Object left open (no closing brace) so the optional button report can be
    // appended before it is closed below.
    int n = snprintf(dst, dst_sz,
        "{%s\"rssi\":%d,\"ip\":\"%s\","
        "\"fw_version\":\"%s\",\"kind\":\"%s\",\"panel_w\":%d,\"panel_h\":%d,"
        "\"sleep_interval_s\":%d,\"next_sleep_s\":%d,\"wake_reason\":\"%s\"",
        battery, rssi, ip, FW_VERSION, DEVICE_KIND, EPD_W, EPD_H,
        sleep_interval_s, sleep_interval_s, wake_reason_str(reset_reason));
    if (n < 0 || (size_t)n >= dst_sz) goto truncated;
    // Absolute epoch the device intends to wake next. The
    // server cross-checks it against next_sleep_s and prefers next_sleep_s on
    // disagreement, so only send it with a real clock — the REST Date header
    // sets the RTC each wake, so this normally holds after the first response.
    time_t now = time(NULL);
    if (now > CLOCK_SANE_EPOCH && sleep_interval_s > 0)
        n += snprintf(dst + n, dst_sz - n, ",\"sleep_until\":%lld",
                      (long long)(now + sleep_interval_s));
    if (n < 0 || (size_t)n >= dst_sz) goto truncated;
    if (button && button[0])
        n += snprintf(dst + n, dst_sz - n, ",\"button\":\"%s\",\"button_event_id\":%u",
                      button, (unsigned)button_event_id);
    if (n < 0 || (size_t)n >= dst_sz - 1) goto truncated;   // need room for the closing '}'
    snprintf(dst + n, dst_sz - n, "}");
    return;

truncated:
    // Never POST a fragment: an object left unterminated by a truncated append is
    // rejected wholesale by the server, losing the battery/RSSI/button telemetry
    // that would otherwise have fit. Emit a minimal valid object instead so the
    // server still sees the device is alive, and log it so the condition is findable
    // (currently unreachable — ~250 B into a 512 B buffer — but the append list keeps
    // growing and the buffer is the kind of thing that gets tightened).
    ESP_LOGE(TAG, "heartbeat body did not fit in %u bytes; sending minimal",
             (unsigned)dst_sz);
    snprintf(dst, dst_sz, "{\"fw_version\":\"%s\"}", FW_VERSION);
}
