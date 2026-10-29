// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#pragma once
#ifndef FW_VERSION
#ifdef ESP_PLATFORM
#error "FW_VERSION must be supplied by the firmware build"
#endif
#define FW_VERSION "host-test"
#endif
#define DEVICE_KIND "note4c_client"
#define SETUP_CLOUD_URL "https://cloud.tesserae.ink"
#define WIFI_HOSTNAME "tesserae-note4c"
#define WIFI_CONNECT_RETRIES 5
#define WIFI_FAST_CONNECT_RETRIES 1
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define SLEEP_INTERVAL_DEFAULT_S 900
#define REFRESH_HOLD_SECONDS 5
#define REFRESH_HOLD_END_SECONDS 10
#define SETUP_HOLD_SECONDS 20
#define SLEEP_INTERVAL_MIN_S 30
#define SLEEP_INTERVAL_MAX_S (7 * 24 * 60 * 60)
#define REST_DISCOVER_RETRY_S 900
#define REST_PAIR_REJECT_RETRY_S 3600
#define RELAY_BUTTON_WINDOW_S 45
#define RELAY_BUTTON_POLL_MS 5000
#define CLOCK_SANE_EPOCH 1750000000
#define PROVISION_AP_SSID "Note4c-Setup"
#define PROVISION_AP_PASS "tesserae"
#define PROVISION_PORTAL_TIMEOUT_S 600
