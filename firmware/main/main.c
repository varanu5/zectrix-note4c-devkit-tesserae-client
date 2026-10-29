// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "board.h"
#include "button_event.h"
#include "defaults.h"
#include "config_store.h"
#include "connection_retry.h"
#include "power.h"
#include "display_update.h"
#include "epd_driver.h"
#include "framebuf.h"
#include "splash.h"
#include "provisioning.h"
#include "rest_handler.h"
#include "relay.h"
#include "rest_button.h"
#include "wifi_manager.h"
#include "wifi_credentials.h"
#include "wake_align.h"
#include "log_capture.h"
#include "esp_system.h"
#include "wake_cause.h"
#include "esp_random.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>
#include <time.h>

static const char *TAG = "main";
RTC_DATA_ATTR static connection_retry_t s_connection_retry;
RTC_DATA_ATTR static uint32_t s_relay_button_sequence;

static void connection_sleep(uint32_t next_s, bool connected) __attribute__((noreturn));
static void connection_sleep(uint32_t next_s, bool connected) {
    if (config_rest_unpaired()) {
        splash_unpaired();
        power_sleep(86400, false);
    }
    uint8_t previous_failures = s_connection_retry.failures;
    uint32_t seconds = connection_retry_sleep(&s_connection_retry, connected, next_s,
                                             config_get_sleep_s(SLEEP_INTERVAL_DEFAULT_S));
    if (!connected) {
        ESP_LOGW(TAG, "connection unavailable; failed wakes %u%s; retry in %lu s",
                 s_connection_retry.failures,
                 s_connection_retry.failures == CONNECTION_FAILURE_LIMIT ? " or more" : "",
                 (unsigned long)seconds);
    } else if (previous_failures) {
        ESP_LOGI(TAG, "connection restored; normal wake schedule resumed");
    }
    power_sleep(seconds, connected);
}

void app_main(void) {
    power_init();
    power_handle_charge_wakeup();
    log_capture_init();
    power_log_wakeup();
    ESP_LOGI(TAG, "ZecTrix NOTE4C Devkit (c) 2026 varanu5 - "
                  "https://github.com/varanu5/zectrix-note4c-devkit-tesserae-client");
    ESP_LOGI(TAG, "ZecTrix NOTE4C Devkit %s; PSRAM %u bytes", FW_VERSION, (unsigned)esp_psram_get_size());
    esp_err_t init = config_init();
    if (init != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable: %s; settings preserved; USB remains awake", esp_err_to_name(init));
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    power_measure_battery();
    bool low = power_battery_low();
    splash_low_battery(low, power_battery_mv());
    if (low) {
        ESP_LOGW(TAG, "low battery; charge screen and daily battery check");
        power_sleep(86400, false);
    }
    esp_reset_reason_t reset = esp_reset_reason();
    wake_align_begin(reset == ESP_RST_DEEPSLEEP, note4c_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    connection_retry_begin(&s_connection_retry, reset == ESP_RST_DEEPSLEEP);
    button_action_t action = power_boot_action();
    char ssid[33], password[65], server[160];
    bool configured = config_get_wifi(ssid, sizeof(ssid), password, sizeof(password)) &&
                      wifi_credentials_valid(ssid, password);
    memset(password, 0, sizeof(password));
    bool relay = config_relay_configured();
    if (relay) config_get_relay_url(server, sizeof server);
    else config_get_server_url(server, sizeof server);
    relay_begin_wake(reset == ESP_RST_DEEPSLEEP);
    if (!configured || !server[0] || action == BUTTON_SETUP) {
        provisioning_run(NULL);
    }
    if (relay && relay_pairing_revoked()) {
        relay_forget_revoked_pairing();
        splash_unpaired();
        power_sleep(86400, false);
    }
    bool check_setup = config_setup_check_pending();
    bool setup_result = check_setup;
    if (check_setup && config_finish_setup_check() != ESP_OK) {
        ESP_LOGE(TAG, "setup check state unavailable; retry next wake");
        power_sleep(SLEEP_INTERVAL_DEFAULT_S, false);
    }
    // Leave a USB recovery window on cold boots.
    if (reset != ESP_RST_DEEPSLEEP) vTaskDelay(pdMS_TO_TICKS(3000));
    uint32_t sleep = config_get_sleep_s(SLEEP_INTERVAL_DEFAULT_S);
    bool painted = false;
    uint32_t button_event = 0;
    const char *button = rest_button_action(action);
    if (wifi_start_sta() != ESP_OK) {
        wifi_stop();
        if (check_setup) {
            const char *note = wifi_fail_looks_like_bad_password()
                ? "Could not join Wi-Fi. Check the password and try again."
                : wifi_fail_looks_like_no_ap()
                ? "Could not find the network. Check its name and that 2.4 GHz Wi-Fi is enabled."
                : "Could not connect to Wi-Fi. Check the network and password, then try again.";
            provisioning_run(note);
        }
        ESP_LOGW(TAG, "Wi-Fi failed; hold front %ds on a wake to change setup", SETUP_HOLD_SECONDS);
        connection_sleep(sleep, false);
    }
    if (!strncmp(server, "https://", 8) && time(NULL) < CLOCK_SANE_EPOCH && wifi_sync_ntp() != ESP_OK) {
        wifi_stop();
        if (check_setup) provisioning_run("Wi-Fi connected, but the clock could not be set for HTTPS. Check internet access and try again.");
        connection_sleep(sleep, false);
    }
    bool was_unpaired = config_rest_unpaired();
    bool paired = false, connected = false;
    const uint8_t *frame = NULL;
    uint32_t event = button ? button_event_next(relay ? &s_relay_button_sequence : &button_event, esp_random()) : 0;
    if (relay) {
        if (!relay_ready()) {
            relay_pair_result_t result = relay_pair_step();
            if (result == RELAY_PAIR_EXPIRED || (check_setup && result == RELAY_PAIR_ERROR)) {
                relay_end_wake(); wifi_stop();
                provisioning_run(relay_setup_error());
            }
        }
        paired = relay_ready();
        if (paired) sleep = relay_run_loop(button, event);
        else { sleep = 30; relay_end_wake(); }
        connected = relay_connection_ok();
        if (relay_pairing_revoked()) {
            relay_end_wake(); wifi_stop();
            relay_forget_revoked_pairing();
            splash_unpaired();
            power_sleep(86400, false);
        }
        frame = relay_pending_frame();
    } else {
        sleep = rest_run_loop(reset, button, event);
        connected = rest_connection_ok();
        frame = rest_pending_frame();
        char token[80]; config_get_device_token(token, sizeof token);
        paired = token[0] != 0;
    }
    if (was_unpaired && paired) setup_result = true;
    if (check_setup && !connected) {
        wifi_stop();
        if (relay) provisioning_run(relay_setup_error());
        provisioning_run("Wi-Fi connected, but the server or pairing could not be confirmed. Check the server address and pairing code.");
    }
    wifi_stop();
    if (config_rest_unpaired()) connection_sleep(86400, connected);
    if (paired) splash_unpaired_clear();
    bool waiting_attempted = false;
    if (setup_result && connected && paired) {
        uint8_t *saved_frame = frame
            ? heap_caps_malloc(EPD_FB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
        if (!frame || saved_frame) {
            // The splash shares the downloaded frame's staging buffer.
            if (frame) memcpy(saved_frame, frame, EPD_FB_BYTES);
            splash_waiting(true);
            waiting_attempted = true;
            if (saved_frame) {
                memcpy(framebuf(), saved_frame, EPD_FB_BYTES);
                heap_caps_free(saved_frame);
            }
        } else {
            ESP_LOGW(TAG, "waiting screen skipped: no frame backup memory");
        }
    }
    if (frame) {
        esp_err_t err = display_present_selected(frame);
        if (err == ESP_OK) {
            if (relay) relay_frame_painted(); else rest_frame_painted();
            painted = true;
        }
    }
    if (!waiting_attempted && connected && !painted && !frame &&
        (setup_result || (!paired && reset != ESP_RST_DEEPSLEEP)))
        splash_waiting(relay || paired);
    connection_sleep(sleep, connected);
}
