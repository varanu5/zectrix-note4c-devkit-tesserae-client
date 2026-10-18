// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#include "esp_err.h"
#include "display_mode.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
esp_err_t config_init(void);
bool config_get_wifi(char *ssid, size_t ss, char *pass, size_t ps);
void config_get_server_url(char *out, size_t cap);
void config_get_device_id(char *out, size_t cap);
void config_get_device_token(char *out, size_t cap);
void config_get_pairing_code(char *out, size_t cap);
void config_get_etag(char *out, size_t cap);
void config_set_device_id(const char *value);
void config_set_device_token(const char *value);
void config_set_pairing_code(const char *value);
esp_err_t config_save_pairing(const char *id, const char *token, bool consume_code);
esp_err_t config_set_etag(const char *value);
esp_err_t config_clear_frame_ref(void);
display_mode_t config_get_display_mode(void);
esp_err_t config_save_display_mode(display_mode_t mode);
bool config_setup_check_pending(void);
esp_err_t config_finish_setup_check(void);
uint32_t config_get_sleep_s(uint32_t fallback);
void config_set_sleep_s(uint32_t value);
bool config_get_ap_hint(uint8_t bssid[6], uint8_t *channel);
void config_set_ap_hint(const uint8_t bssid[6], uint8_t channel);
void config_clear_ap_hint(void);
esp_err_t config_save_setup(const char *ssid, const char *pass, const char *server,
                            const char *id, const char *pair);
esp_err_t config_save_setup_with_mode(const char *ssid, const char *pass, const char *server,
                                     const char *id, const char *pair, display_mode_t mode);

bool config_relay_configured(void);
bool config_relay_ready(void);
bool config_relay_revoked(void);
void config_get_relay_url(char *, size_t);
void config_get_relay_code(char *, size_t);
void config_get_relay_install(char *, size_t);
void config_get_relay_device(char *, size_t);
void config_get_relay_token(char *, size_t);
void config_get_relay_etag(char *, size_t);
void config_get_relay_config_etag(char *, size_t);
bool config_get_relay_priv(uint8_t [32]);
bool config_get_relay_key(uint8_t [32]);
esp_err_t config_set_relay_priv(const uint8_t [32]);
esp_err_t config_set_relay_paired(const char *, const char *, const char *, const uint8_t [32]);
esp_err_t config_set_relay_etag(const char *);
esp_err_t config_set_relay_config_etag(const char *);
esp_err_t config_mark_relay_revoked(void);
esp_err_t config_save_setup_transport(const char *, const char *, const char *, const char *, const char *,
                                      display_mode_t, bool, const char *, const char *);
