// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "display_update.h"
#include "config_store.h"
#include "epd_driver.h"
#include "log_capture.h"
#include "esp_log.h"

static esp_err_t present(const uint8_t *frame, display_mode_t mode) {
    if (!frame) return ESP_ERR_INVALID_ARG;
    esp_err_t err = config_clear_frame_ref();
    if (err != ESP_OK) {
        ESP_LOGE("display", "storage/invalidate failed: %s", esp_err_to_name(err));
        log_capture_paint_error(DIAG_PAINT_DISPLAY_FAILED);
        return err;
    }
    diag_paint_t failure;
    err = epd_present_mode(frame, mode, &failure);
    if (err != ESP_OK) log_capture_paint_error(failure);
    return err;
}

esp_err_t display_present(const uint8_t *frame) { return present(frame, DISPLAY_NATIVE); }
esp_err_t display_present_selected(const uint8_t *frame) {
    return present(frame, config_get_display_mode());
}
