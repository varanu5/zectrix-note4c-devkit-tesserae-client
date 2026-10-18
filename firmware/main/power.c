// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
// Battery conversion follows the supplied MIT ZecTrix NOTE4C Devkit reference; see THIRD_PARTY_NOTICES.md.
#include "power.h"
#include "board.h"
#include "defaults.h"
#include "wake_align.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "soc/gpio_reg.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "wake_cause.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_system.h"
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static int s_mv = -1;
RTC_DATA_ATTR static bool s_low;
RTC_DATA_ATTR static unsigned s_low_count;
static const char *TAG = "power";
RTC_DATA_ATTR static int64_t s_poll_deadline_us;
RTC_DATA_ATTR static int64_t s_charge_wake_us;
RTC_DATA_ATTR static unsigned s_charge_wakes;
RTC_DATA_ATTR static bool s_sleep_led_on;
RTC_DATA_ATTR static uint64_t s_last_sleep_us;
RTC_DATA_ATTR static uint32_t s_charge_only_count;
RTC_DATA_ATTR static uint64_t s_charge_only_awake_ms;
static portMUX_TYPE s_led_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t s_charge_timer;
static int s_charge_sample = -1;
static int64_t s_charge_sample_us;
static bool s_charging, s_led_override, s_led_sleeping, s_feedback_charging;

static int64_t wall_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
static int charge_inputs(void) {
    return gpio_get_level(PIN_CHARGE_ACTIVE) | (gpio_get_level(PIN_CHARGE_FULL) << 1);
}
static void charge_tick(void *arg) {
    (void)arg;
    portENTER_CRITICAL(&s_led_lock);
    if (!s_led_sleeping) {
        int sample = charge_inputs();
        int64_t now = esp_timer_get_time();
        if (sample != s_charge_sample) {
            s_charge_sample = sample;
            s_charge_sample_us = now;
        }
        if (now - s_charge_sample_us >= 400000) {
            s_charging = !(sample & 1) && !(sample & 2);
            if (!s_led_override) gpio_set_level(PIN_LED, s_charging ? 0 : 1);
        }
    }
    portEXIT_CRITICAL(&s_led_lock);
}
static void led_override(bool enabled) {
    portENTER_CRITICAL(&s_led_lock);
    s_led_override = enabled;
    if (enabled) s_feedback_charging = s_charging;
    if (!enabled) gpio_set_level(PIN_LED, s_charging ? 0 : 1);
    portEXIT_CRITICAL(&s_led_lock);
}
static bool charge_settle(void) {
    int64_t until = esp_timer_get_time() + 600000;
    bool stable;
    do {
        charge_tick(NULL);
        portENTER_CRITICAL(&s_led_lock);
        stable = esp_timer_get_time() - s_charge_sample_us >= 400000;
        portEXIT_CRITICAL(&s_led_lock);
        if (stable) return true;
        vTaskDelay(pdMS_TO_TICKS(20));
    } while (esp_timer_get_time() < until);
    return false;
}

static void output(int pin, int level) {
    gpio_set_level(pin, level);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_hold_dis(pin);
}
void power_init(void) {
    output(PIN_POWER_HOLD, 1);
    output(PIN_EPD_POWER, 0);
    output(PIN_AUDIO_AMP, 0);
    output(PIN_AUDIO_POWER, 0);
    output(PIN_NFC_POWER, 0);
    s_charging = esp_reset_reason() == ESP_RST_DEEPSLEEP && s_sleep_led_on;
    output(PIN_LED, s_charging ? 0 : 1);
    gpio_deep_sleep_hold_dis();
    rtc_gpio_deinit(PIN_BUTTON_FRONT);
    rtc_gpio_deinit(PIN_BUTTON_DOWN);
    // Keep the unused upper button biased as an input.
    gpio_config_t input = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_FRONT) | (1ULL << PIN_BUTTON_DOWN) |
                        (1ULL << PIN_BUTTON_UP),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input));
    rtc_gpio_deinit(PIN_CHARGE_ACTIVE);
    rtc_gpio_deinit(PIN_CHARGE_FULL);
    gpio_config_t charge = {
        .pin_bit_mask = (1ULL << PIN_CHARGE_ACTIVE) | (1ULL << PIN_CHARGE_FULL),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&charge));
    charge_tick(NULL);
    const esp_timer_create_args_t timer = {.callback = charge_tick, .name = "charge_led"};
    ESP_ERROR_CHECK(esp_timer_create(&timer, &s_charge_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_charge_timer, 100000));
}
void power_measure_battery(void) {
    adc_oneshot_unit_handle_t adc = NULL;
    adc_cali_handle_t cal = NULL;
    adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    adc_oneshot_chan_cfg_t ch = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    adc_cali_curve_fitting_config_t calibration = {
        .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_3,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit, &adc);
    if (err == ESP_OK) err = adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &ch);
    if (err == ESP_OK) err = adc_cali_create_scheme_curve_fitting(&calibration, &cal);
    int total = 0;
    for (int i = 0; err == ESP_OK && i < 10; i++) {
        int raw, mv;
        err = adc_oneshot_read(adc, ADC_CHANNEL_3, &raw);
        if (err == ESP_OK) err = adc_cali_raw_to_voltage(cal, raw, &mv);
        if (err == ESP_OK) total += mv * 2;
    }
    if (cal) adc_cali_delete_scheme_curve_fitting(cal);
    if (adc) adc_oneshot_del_unit(adc);
    if (err == ESP_OK && total / 10 >= 2000 && total / 10 <= 4500) s_mv = total / 10;
    else ESP_LOGW(TAG, "battery measurement unavailable: %s", esp_err_to_name(err));
    if (s_mv >= 0) ESP_LOGI(TAG, "battery %d mV (%d%% estimate)", s_mv, power_battery_pct(s_mv));
}
int power_battery_mv(void) { return s_mv; }
int power_battery_pct(int mv) {
    if (mv < 0) return -1;
    int pct = (-mv * mv + 9016 * mv - 19189000) / 10000;
    return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}
bool power_battery_low(void) {
    if (s_mv < 0) return s_low;
    // Low-battery hysteresis: lock below 3400 mV twice; release at 3550 mV.
    if (s_mv >= 3550) { s_low = false; s_low_count = 0; }
    else if (s_mv < 3400) { if (s_low_count < 2) s_low_count++; if (s_low_count >= 2) s_low = true; }
    else s_low_count = 0;
    return s_low;
}
static void led_flash(unsigned count) {
    for (unsigned i = 0; i < count; i++) {
        gpio_set_level(PIN_LED, s_feedback_charging ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(80));
        gpio_set_level(PIN_LED, s_feedback_charging ? 0 : 1);
        if (i + 1 < count) vTaskDelay(pdMS_TO_TICKS(80));
    }
}
static button_action_t classify(bool front, bool down) {
    if (front) {
        int64_t start = esp_timer_get_time();
        led_flash(1);
        while (!gpio_get_level(PIN_BUTTON_FRONT)) {
            int64_t held_us = esp_timer_get_time() - start;
            if (held_us >= (int64_t)SETUP_HOLD_SECONDS * 1000000) {
                led_flash(3);
                return BUTTON_SETUP;
            }
            if (held_us >= (int64_t)REFRESH_HOLD_SECONDS * 1000000 &&
                held_us < (int64_t)REFRESH_HOLD_END_SECONDS * 1000000) {
                gpio_set_level(PIN_LED, 0);
            } else {
                // Invert the charging indication briefly once per second.
                bool pulse = held_us % 1000000 < 80000;
                gpio_set_level(PIN_LED, pulse == s_feedback_charging ? 1 : 0);
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        int64_t held_us = esp_timer_get_time() - start;
        if (held_us >= (int64_t)SETUP_HOLD_SECONDS * 1000000) {
            led_flash(3);
            return BUTTON_SETUP;
        }
        gpio_set_level(PIN_LED, s_feedback_charging ? 0 : 1);
        if (held_us >= (int64_t)REFRESH_HOLD_SECONDS * 1000000 &&
            held_us < (int64_t)REFRESH_HOLD_END_SECONDS * 1000000) return BUTTON_REFRESH;
        return BUTTON_CHECK;
    }
    if (down) led_flash(1);
    return down ? BUTTON_NEXT : BUTTON_NONE;
}
button_action_t power_boot_action(void) {
    uint64_t wake = note4c_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1
                    ? esp_sleep_get_ext1_wakeup_status() : 0;
    led_override(true);
    button_action_t action = classify(!gpio_get_level(PIN_BUTTON_FRONT) || (wake & (1ULL << PIN_BUTTON_FRONT)),
                                     !gpio_get_level(PIN_BUTTON_DOWN) || (wake & (1ULL << PIN_BUTTON_DOWN)));
    led_override(false);
    return action;
}
static uint64_t sleep_inputs(void) {
    int64_t deadline = esp_timer_get_time() + 2000000;
    while ((!gpio_get_level(PIN_BUTTON_FRONT) || !gpio_get_level(PIN_BUTTON_DOWN)) &&
           esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(20));
    bool stable = charge_settle();
    ESP_ERROR_CHECK(esp_timer_stop(s_charge_timer));
    portENTER_CRITICAL(&s_led_lock);
    s_led_sleeping = true;
    int sample = charge_inputs();
    stable = stable && sample == s_charge_sample;
    s_sleep_led_on = stable && s_charging;
    gpio_set_level(PIN_LED, s_sleep_led_on ? 0 : 1);
    portEXIT_CRITICAL(&s_led_lock);
    uint64_t mask = 0;
    const int buttons[] = {PIN_BUTTON_FRONT, PIN_BUTTON_DOWN};
    for (unsigned i = 0; i < 2; i++) {
        if (gpio_get_level(buttons[i])) {
            mask |= 1ULL << buttons[i];
            rtc_gpio_pullup_en(buttons[i]);
            rtc_gpio_pulldown_dis(buttons[i]);
        }
    }
    // Limit rapid charger transitions until the next ordinary wake.
    if (stable && s_charge_wakes < 4) {
        rtc_gpio_pullup_dis(PIN_CHARGE_ACTIVE);
        rtc_gpio_pulldown_dis(PIN_CHARGE_ACTIVE);
        if (sample & 1) mask |= 1ULL << PIN_CHARGE_ACTIVE;
        else ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PIN_CHARGE_ACTIVE, 1));
    } else {
        s_sleep_led_on = false;
        gpio_set_level(PIN_LED, 1);
        ESP_LOGW(TAG, "charge input unstable; status wake paused until next check");
    }
    if (mask) ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW));
    return mask;
}
static void enter_sleep(uint64_t us, uint64_t mask) __attribute__((noreturn));
static void log_sleep_gpio(int pin, const char *name, esp_err_t hold) {
    gpio_io_config_t io;
    esp_err_t err = gpio_get_io_config(pin, &io);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GPIO%d %s: readback failed: %s", pin, name, esp_err_to_name(err));
        return;
    }
    // Read the output latch; input sampling returns zero on output-only pins.
    unsigned level = (REG_READ(pin < 32 ? GPIO_OUT_REG : GPIO_OUT1_REG) >> (pin % 32)) & 1U;
    ESP_LOGI(TAG, "GPIO%d %s: out=%u in=%u latch=%u pu=%u pd=%u od=%u hold=%s",
             pin, name, (unsigned)io.oe, (unsigned)io.ie, level,
             (unsigned)io.pu, (unsigned)io.pd, (unsigned)io.od, esp_err_to_name(hold));
}
void power_log_wakeup(void) {
    esp_reset_reason_t reset = esp_reset_reason();
    uint32_t causes = esp_sleep_get_wakeup_causes();
    uint64_t pins = (causes & (1UL << ESP_SLEEP_WAKEUP_EXT1))
                    ? esp_sleep_get_ext1_wakeup_status() : 0;
    ESP_LOGI(TAG, "wake: reset=%d deep_sleep=%s causes=0x%lx ext1=0x%llx",
             (int)reset, reset == ESP_RST_DEEPSLEEP ? "yes" : "no",
             (unsigned long)causes, (unsigned long long)pins);
    if (reset == ESP_RST_DEEPSLEEP) {
        ESP_LOGI(TAG, "previous sleep requested=%.3f s; charger-only wakes=%lu awake=%llu ms",
                 (double)s_last_sleep_us / 1000000, (unsigned long)s_charge_only_count,
                 (unsigned long long)s_charge_only_awake_ms);
    }
    s_charge_only_count = 0;
    s_charge_only_awake_ms = 0;
}
static void enter_sleep(uint64_t us, uint64_t mask) {
    (void)mask;
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(us));
    output(PIN_POWER_HOLD, 1);
    output(PIN_EPD_POWER, 0);
    const int held[] = {PIN_POWER_HOLD, PIN_EPD_POWER, PIN_LED, PIN_AUDIO_POWER,
                        PIN_AUDIO_AMP, PIN_NFC_POWER};
    const char *names[] = {"latch", "panel", "LED", "audio", "amplifier", "NFC"};
    esp_err_t holds[sizeof(held) / sizeof(held[0])];
    for (unsigned i = 0; i < sizeof(held) / sizeof(held[0]); i++) holds[i] = gpio_hold_en(held[i]);
    gpio_deep_sleep_hold_en();
    for (unsigned i = 0; i < sizeof(held) / sizeof(held[0]); i++) log_sleep_gpio(held[i], names[i], holds[i]);
    s_last_sleep_us = us;
    ESP_LOGI(TAG, "deep-sleep entry; awake=%lld ms; digital hold enabled; charging LED=%s",
             (long long)(esp_timer_get_time() / 1000), s_sleep_led_on ? "on" : "off");
    ESP_LOGI(TAG, "sleep %.3f s; latch high; wake mask 0x%llx", (double)us / 1000000, mask);
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    // The USB VFS drains pending output with a bounded wait before power-off.
    (void)fflush(stdout);
    (void)fsync(STDOUT_FILENO);
#endif
    esp_deep_sleep_start();
    __builtin_unreachable();
}
void power_handle_charge_wakeup(void) {
    uint32_t causes = esp_sleep_get_wakeup_causes();
    uint64_t pins = (causes & (1UL << ESP_SLEEP_WAKEUP_EXT1))
                    ? esp_sleep_get_ext1_wakeup_status() : 0;
    bool charge = (causes & (1UL << ESP_SLEEP_WAKEUP_EXT0)) ||
                  (pins & (1ULL << PIN_CHARGE_ACTIVE));
    bool button = (pins & ((1ULL << PIN_BUTTON_FRONT) | (1ULL << PIN_BUTTON_DOWN))) ||
                  !gpio_get_level(PIN_BUTTON_FRONT) || !gpio_get_level(PIN_BUTTON_DOWN);
    int64_t now = wall_us();
    if (esp_reset_reason() != ESP_RST_DEEPSLEEP || !charge || button ||
        (causes & (1UL << ESP_SLEEP_WAKEUP_TIMER)) || s_poll_deadline_us <= now ||
        s_poll_deadline_us - now > (int64_t)SLEEP_INTERVAL_MAX_S * 1000000) {
        s_charge_wakes = 0;
        return;
    }
    if (now - s_charge_wake_us > 10000000 || now < s_charge_wake_us) s_charge_wakes = 0;
    s_charge_wake_us = now;
    if (s_charge_wakes < 4) s_charge_wakes++;
    charge_settle();
    if (!gpio_get_level(PIN_BUTTON_FRONT) || !gpio_get_level(PIN_BUTTON_DOWN) ||
        s_poll_deadline_us <= wall_us()) {
        s_charge_wakes = 0;
        return;
    }
    uint64_t mask = sleep_inputs();
    int64_t remaining = s_poll_deadline_us - wall_us();
    ESP_LOGI(TAG, "charge status wake; LED %s; resuming scheduled sleep", s_sleep_led_on ? "on" : "off");
    s_charge_only_count++;
    s_charge_only_awake_ms += (uint64_t)(esp_timer_get_time() / 1000);
    enter_sleep(remaining > 1000 ? (uint64_t)remaining : 1000, mask);
}
void power_sleep(uint32_t seconds, bool scheduled) {
    if (seconds < SLEEP_INTERVAL_MIN_S) seconds = SLEEP_INTERVAL_MIN_S;
    if (seconds > SLEEP_INTERVAL_MAX_S) seconds = SLEEP_INTERVAL_MAX_S;
    uint64_t mask = sleep_inputs();
    uint64_t us = scheduled ? wake_align_timer_us(seconds) : (uint64_t)seconds * 1000000;
    s_poll_deadline_us = wall_us() + (int64_t)us;
    enter_sleep(us, mask);
}
