// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
// SSD2683 sequence adapted from the supplied MIT ZecTrix NOTE4C Devkit reference; see THIRD_PARTY_NOTICES.md.
#include "epd_driver.h"
#include "epd_lut_5s.h"
#include "epd_lut_10s.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "epd";
static spi_device_handle_t s_spi;
static bool s_bus;
static bool s_control_gpio_ready;

static esp_err_t ready(unsigned timeout_ms) {
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (gpio_get_level(PIN_EPD_BUSY) == 0) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGE(TAG, "SSD2683 BUSY timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ESP_OK;
}

static esp_err_t send(bool data, const uint8_t *bytes, size_t count) {
    gpio_set_level(PIN_EPD_DC, data);
    spi_transaction_t t = {.length = count * 8, .tx_buffer = bytes};
    return spi_device_polling_transmit(s_spi, &t);
}
static esp_err_t command(uint8_t value) { return send(false, &value, 1); }
static esp_err_t data(uint8_t value) { return send(true, &value, 1); }
static esp_err_t reg(uint8_t cmd, const uint8_t *bytes, size_t n) {
    esp_err_t err = command(cmd);
    return err == ESP_OK ? send(true, bytes, n) : err;
}

static esp_err_t refresh_wait(unsigned timeout_ms) {
    int64_t deadline = esp_timer_get_time() + 1000000;
    while (gpio_get_level(PIN_EPD_BUSY) != 0) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGE(TAG, "refresh BUSY did not assert");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ready(timeout_ms);
}

static void stage_start(bool fast, const char *stage, int64_t start) {
    if (fast) ESP_LOGI(TAG, "%s start at %lld ms", stage,
                      (long long)((esp_timer_get_time() - start) / 1000));
}

#define TRY(expr, category, stage) do { \
    err = (expr); \
    if (err != ESP_OK) { \
        reason = (category); \
        ESP_LOGE(TAG, "%s failed: %s", stage, esp_err_to_name(err)); \
        goto done; \
    } \
} while (0)
#define CLEANUP(expr, stage) do { \
    esp_err_t cleanup = (expr); \
    if (cleanup != ESP_OK) { \
        ESP_LOGE(TAG, "%s failed: %s", stage, esp_err_to_name(cleanup)); \
        if (err == ESP_OK) { err = cleanup; reason = DIAG_PAINT_DISPLAY_FAILED; } \
    } \
} while (0)

esp_err_t epd_present(const uint8_t *frame, diag_paint_t *failure) {
    return epd_present_mode(frame, DISPLAY_NATIVE, failure);
}

esp_err_t epd_present_mode(const uint8_t *frame, display_mode_t mode, diag_paint_t *failure) {
    if (failure) *failure = DIAG_PAINT_NONE;
    if (!frame || !display_mode_valid(mode)) return ESP_ERR_INVALID_ARG;
    const bool fast = mode == DISPLAY_FAST_5S || mode == DISPLAY_FAST_10S;
    const int spi_hz = fast ? 1000000 : EPD_SPI_HZ;
    const unsigned ready_ms = fast ? 5000 : 120000;
    const int64_t start = esp_timer_get_time();
    const char *mode_name = mode == DISPLAY_NATIVE ? "native" : mode == DISPLAY_FAST_5S ? "experimental-5s" :
                            "experimental-10s";
    ESP_LOGI(TAG, "mode=%s SPI=%d Hz", mode_name, spi_hz);
    stage_start(fast, "reset", start);
    esp_err_t err = ESP_OK;
    bool sleep_sent = false;
    diag_paint_t reason = DIAG_PAINT_NONE;
    gpio_config_t output = {
        .pin_bit_mask = (1ULL << PIN_EPD_POWER) | (1ULL << PIN_EPD_RST) |
                        (1ULL << PIN_EPD_DC),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_hold_dis(PIN_EPD_POWER);
    // Reuse control outputs across paints in the same wake.
    if (!s_control_gpio_ready) {
        TRY(gpio_config(&output), DIAG_PAINT_INIT_FAILED, "init/gpio");
        s_control_gpio_ready = true;
    }
    gpio_config_t input = {.pin_bit_mask = 1ULL << PIN_EPD_BUSY, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    TRY(gpio_config(&input), DIAG_PAINT_INIT_FAILED, "init/busy");
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_EPD_MOSI, .miso_io_num = -1,
        .sclk_io_num = PIN_EPD_SCLK, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = EPD_W / 4,
    };
    TRY(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO), DIAG_PAINT_INIT_FAILED, "init/spi-bus");
    s_bus = true;
    spi_device_interface_config_t device = {
        .clock_speed_hz = spi_hz, .mode = 0,
        .spics_io_num = PIN_EPD_CS, .queue_size = 1,
    };
    TRY(spi_bus_add_device(SPI3_HOST, &device, &s_spi), DIAG_PAINT_INIT_FAILED, "init/spi-device");
    gpio_set_level(PIN_EPD_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/reset");
    stage_start(fast, "init", start);
    if (fast) {
        const uint8_t *lut = (mode == DISPLAY_FAST_10S) ? EPD_LUT10S : EPD_LUT5S;
        uint8_t timing[7]; memcpy(timing, lut + 528, sizeof timing);
        TRY(reg(0x06, (const uint8_t[]){0x0F, 0x8B, 0x9C, 0x96}, 4), DIAG_PAINT_INIT_FAILED, "init/booster");
        TRY(reg(0x50, (const uint8_t[]){0x37}, 1), DIAG_PAINT_INIT_FAILED, "init/border");
        // Match native gate/source directions: UD=1, SHL=1.
        TRY(reg(0x00, (const uint8_t[]){0x0F, 0xA9}, 2), DIAG_PAINT_INIT_FAILED, "init/external");
        TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/external");
        uint8_t pwr[] = {0x07, timing[0], timing[1], timing[3], timing[2], timing[4]};
        TRY(reg(0x01, pwr, sizeof pwr), DIAG_PAINT_INIT_FAILED, "init/drive");
        uint8_t vcom = (uint8_t)(timing[5] - 0x80);
        TRY(reg(0x82, &vcom, 1), DIAG_PAINT_INIT_FAILED, "init/vcom");
        TRY(reg(0x30, timing + 6, 1), DIAG_PAINT_INIT_FAILED, "init/pll");
        TRY(reg(0x61, (const uint8_t[]){0x01, 0x90, 0x01, 0x2C}, 4), DIAG_PAINT_INIT_FAILED, "init/resolution");
        TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/resolution");
        TRY(reg(0x62, (const uint8_t[]){0x62, 0x51}, 2), DIAG_PAINT_INIT_FAILED, "init/gate");
        TRY(reg(0x65, (const uint8_t[]){0, 0, 0, 0}, 4), DIAG_PAINT_INIT_FAILED, "init/timing");
        TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/timing");
        TRY(reg(0xE7, (const uint8_t[]){0x96}, 1), DIAG_PAINT_INIT_FAILED, "init/temperature");
        TRY(reg(0xE9, (const uint8_t[]){0x01}, 1), DIAG_PAINT_INIT_FAILED, "init/controller");
        stage_start(fast, "lut", start);
        TRY(command(0x20), DIAG_PAINT_INIT_FAILED, "init/lut");
        TRY(send(true, timing, sizeof timing), DIAG_PAINT_INIT_FAILED, "init/lut");
        for (unsigned y = 0; y < 48; y++) {
            uint8_t line[11];
            for (unsigned x = 0; x < 11; x++) line[x] = lut[y + 48 * x];
            TRY(send(true, line, sizeof line), DIAG_PAINT_INIT_FAILED, "init/lut");
        }
    } else {
        TRY(command(0xE9), DIAG_PAINT_INIT_FAILED, "init/controller");
        TRY(data(0x01), DIAG_PAINT_INIT_FAILED, "init/controller");
    }
    TRY(command(0x10), DIAG_PAINT_INIT_FAILED, "init/write");
    TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/write");
    stage_start(fast, "transfer", start);
    // Copy PSRAM rows into an internal buffer for SPI DMA.
    uint8_t row[EPD_W / 4] __attribute__((aligned(4)));
    for (int y = 0; y < EPD_H; y++) {
        memcpy(row, frame + y * sizeof(row), sizeof(row));
        TRY(send(true, row, sizeof(row)), DIAG_PAINT_DISPLAY_FAILED, "transfer");
        if ((y % 16) == 15) vTaskDelay(1);
    }
    stage_start(fast, "power-on", start);
    TRY(command(0x04), DIAG_PAINT_INIT_FAILED, "init/power-on");
    if (fast) vTaskDelay(pdMS_TO_TICKS(20));
    TRY(ready(ready_ms), DIAG_PAINT_READY_TIMEOUT, "ready/power-on");
    vTaskDelay(pdMS_TO_TICKS(10));
    stage_start(fast, "refresh", start);
    TRY(command(0x12), DIAG_PAINT_DISPLAY_FAILED, "refresh/command");
    TRY(data(0x00), DIAG_PAINT_DISPLAY_FAILED, "refresh/command");
    TRY(refresh_wait(fast ? 50000 : ready_ms), DIAG_PAINT_REFRESH_TIMEOUT, "refresh/wait");
    stage_start(fast, "shutdown", start);
    TRY(command(0x02), DIAG_PAINT_DISPLAY_FAILED, "shutdown/power-off");
    TRY(data(0x00), DIAG_PAINT_DISPLAY_FAILED, "shutdown/power-off");
    if (fast) vTaskDelay(pdMS_TO_TICKS(20));
    TRY(ready(ready_ms), DIAG_PAINT_DISPLAY_FAILED, "shutdown/wait");
    vTaskDelay(pdMS_TO_TICKS(20));
    TRY(command(0x07), DIAG_PAINT_DISPLAY_FAILED, "shutdown/sleep");
    TRY(data(0xA5), DIAG_PAINT_DISPLAY_FAILED, "shutdown/sleep");
    sleep_sent = true;
done:
    stage_start(fast, "cleanup", start);
    gpio_set_level(PIN_EPD_POWER, 0);
    gpio_hold_en(PIN_EPD_POWER);
    // Disconnect BUSY from the unpowered panel; restore it at the next refresh.
    gpio_config_t idle = {
        .pin_bit_mask = 1ULL << PIN_EPD_BUSY,
        .mode = GPIO_MODE_DISABLE,
    };
    CLEANUP(gpio_config(&idle), "cleanup/busy");
    if (s_spi) {
        CLEANUP(spi_bus_remove_device(s_spi), "cleanup/spi-device");
        s_spi = NULL;
    }
    if (s_bus) {
        CLEANUP(spi_bus_free(SPI3_HOST), "cleanup/spi-bus");
        s_bus = false;
    }
    // Prevent signalling into an unpowered panel.
    const int pins[] = {PIN_EPD_RST, PIN_EPD_DC, PIN_EPD_CS, PIN_EPD_SCLK, PIN_EPD_MOSI};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }
    if (failure) *failure = reason;
    ESP_LOGI(TAG, "mode=%s total=%lld ms result=%s sleep_cmd=%s", mode_name,
             (long long)((esp_timer_get_time() - start) / 1000), esp_err_to_name(err),
             sleep_sent ? "sent" : "not-sent");
    return err;
}
