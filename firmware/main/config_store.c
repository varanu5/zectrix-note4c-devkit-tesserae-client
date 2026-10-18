// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "config_store.h"
#include "defaults.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static nvs_handle_t s_nvs;
typedef struct {
    uint32_t version;
    char ssid[33], pass[65], server[160], id[64], pair[16], token[80], etag[80];
} settings_v1_t;
typedef struct {
    uint32_t version;
    char ssid[33], pass[65], server[160], id[64], pair[16], token[80], etag[80];
    uint32_t relay_enabled, relay_revoked, relay_has_priv, relay_has_key;
    char relay_url[160], relay_code[40], relay_install[64], relay_device[64];
    char relay_token[256], relay_etag[80], relay_config_etag[80];
    uint8_t relay_priv[32], relay_key[32];
} settings_t;
static settings_t s_settings;
static esp_err_t persist(const settings_t *next) {
    // REST keeps the public release's on-flash layout for compatible rollback.
    settings_v1_t legacy;
    memcpy(&legacy, next, sizeof legacy);
    legacy.version = 1;
    const void *data = next->relay_enabled ? (const void *)next : (const void *)&legacy;
    size_t size = next->relay_enabled ? sizeof(*next) : sizeof legacy;
    esp_err_t err = nvs_set_blob(s_nvs, "settings", data, size);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) s_settings = *next;
    return err;
}
static const char *TAG = "config";
esp_err_t config_init(void) {
    esp_err_t err = nvs_flash_init();
    // Do not erase a previous firmware's settings automatically.
    if (err != ESP_OK) return err;
    err = nvs_open("note4c", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    memset(&s_settings, 0, sizeof(s_settings));
    size_t size = sizeof(s_settings);
    err = nvs_get_blob(s_nvs, "settings", &s_settings, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_settings, 0, sizeof(s_settings));
        s_settings.version = 2;
        return ESP_OK;
    }
    if (err != ESP_OK) return err;
    if (size == sizeof(settings_v1_t) && s_settings.version == 1) s_settings.version = 2;
    else if (size != sizeof(s_settings) || s_settings.version != 2) return ESP_ERR_INVALID_STATE;
    if (s_settings.relay_enabled > 1 || s_settings.relay_revoked > 1 ||
        s_settings.relay_has_priv > 1 || s_settings.relay_has_key > 1 ||
        !memchr(s_settings.relay_url, 0, sizeof s_settings.relay_url) ||
        !memchr(s_settings.relay_code, 0, sizeof s_settings.relay_code) ||
        !memchr(s_settings.relay_install, 0, sizeof s_settings.relay_install) ||
        !memchr(s_settings.relay_device, 0, sizeof s_settings.relay_device) ||
        !memchr(s_settings.relay_token, 0, sizeof s_settings.relay_token) ||
        !memchr(s_settings.relay_etag, 0, sizeof s_settings.relay_etag) ||
        !memchr(s_settings.relay_config_etag, 0, sizeof s_settings.relay_config_etag)) return ESP_ERR_INVALID_STATE;
    if (!memchr(s_settings.ssid, 0, sizeof(s_settings.ssid)) ||
        !memchr(s_settings.pass, 0, sizeof(s_settings.pass)) ||
        !memchr(s_settings.server, 0, sizeof(s_settings.server)) ||
        !memchr(s_settings.id, 0, sizeof(s_settings.id)) ||
        !memchr(s_settings.pair, 0, sizeof(s_settings.pair)) ||
        !memchr(s_settings.token, 0, sizeof(s_settings.token)) ||
        !memchr(s_settings.etag, 0, sizeof(s_settings.etag))) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}
#define GETTER(name, field) \
    void name(char *out, size_t cap) { if (cap) strlcpy(out, s_settings.field, cap); }
#define SETTER(name, field) \
    void name(const char *value) { \
        if (strlen(value) >= sizeof(s_settings.field)) { ESP_LOGE(TAG, "value too long: " #field); return; } \
        if (!strcmp(value, s_settings.field)) return; \
        settings_t next = s_settings; strlcpy(next.field, value, sizeof(next.field)); \
        esp_err_t err = persist(&next); \
        if (err != ESP_OK) ESP_LOGE(TAG, "save " #field ": %s", esp_err_to_name(err)); \
    }
bool config_get_wifi(char *ssid, size_t ss, char *pass, size_t ps) {
    if (!ss || !ps) return false;
    strlcpy(ssid, s_settings.ssid, ss); strlcpy(pass, s_settings.pass, ps);
    return ssid[0] != 0;
}
GETTER(config_get_server_url, server)
GETTER(config_get_device_id, id)
GETTER(config_get_device_token, token)
GETTER(config_get_pairing_code, pair)
GETTER(config_get_etag, etag)
SETTER(config_set_device_id, id)
SETTER(config_set_device_token, token)
SETTER(config_set_pairing_code, pair)
esp_err_t config_save_pairing(const char *id, const char *token, bool consume_code) {
    if (!id || !id[0] || strlen(id) >= sizeof(s_settings.id) ||
        !token || !token[0] || strlen(token) >= sizeof(s_settings.token))
        return ESP_ERR_INVALID_ARG;
    settings_t next = s_settings;
    strlcpy(next.id, id, sizeof(next.id));
    strlcpy(next.token, token, sizeof(next.token));
    if (consume_code) next.pair[0] = '\0';
    return persist(&next);
}
esp_err_t config_set_etag(const char *value) {
    if (strlen(value) >= sizeof(s_settings.etag)) return ESP_ERR_INVALID_ARG;
    if (!strcmp(value, s_settings.etag)) return ESP_OK;
    settings_t next = s_settings;
    strlcpy(next.etag, value, sizeof(next.etag));
    esp_err_t err = persist(&next);
    if (err != ESP_OK) ESP_LOGE(TAG, "save etag: %s", esp_err_to_name(err));
    return err;
}
esp_err_t config_clear_frame_ref(void) {
    if (!s_settings.etag[0] && !s_settings.relay_etag[0]) return ESP_OK;
    settings_t next = s_settings;
    next.etag[0] = next.relay_etag[0] = 0;
    return persist(&next);
}
display_mode_t config_get_display_mode(void) {
    uint32_t mode;
    // Unsupported modes use Native without rewriting saved settings.
    if (nvs_get_u32(s_nvs, "waveform", &mode) != ESP_OK ||
        mode > DISPLAY_FAST_10S || !display_mode_valid((display_mode_t)mode))
        return DISPLAY_NATIVE;
    return (display_mode_t)mode;
}
esp_err_t config_save_display_mode(display_mode_t mode) {
    if (!display_mode_valid(mode)) return ESP_ERR_INVALID_ARG;
    // Keep the settings blob compatible with earlier firmware.
    esp_err_t err = config_clear_frame_ref();
    if (err == ESP_OK) err = nvs_set_u32(s_nvs, "waveform", (uint32_t)mode);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}
bool config_setup_check_pending(void) {
    uint32_t pending = 0;
    return nvs_get_u32(s_nvs, "setup_check", &pending) == ESP_OK && pending == 1;
}
esp_err_t config_finish_setup_check(void) {
    if (!config_setup_check_pending()) return ESP_OK;
    esp_err_t err = nvs_set_u32(s_nvs, "setup_check", 0);
    return err == ESP_OK ? nvs_commit(s_nvs) : err;
}
uint32_t config_get_sleep_s(uint32_t fallback) {
    uint32_t v;
    if (nvs_get_u32(s_nvs, "sleep", &v) != ESP_OK ||
        v < SLEEP_INTERVAL_MIN_S || v > SLEEP_INTERVAL_MAX_S) return fallback;
    return v;
}
void config_set_sleep_s(uint32_t v) {
    if (v < SLEEP_INTERVAL_MIN_S || v > SLEEP_INTERVAL_MAX_S ||
        v == config_get_sleep_s(0)) return;
    esp_err_t err = nvs_set_u32(s_nvs, "sleep", v);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err != ESP_OK) ESP_LOGE(TAG, "save sleep: %s", esp_err_to_name(err));
}
bool config_get_ap_hint(uint8_t bssid[6], uint8_t *channel) {
    uint8_t data[7]; size_t n = sizeof(data);
    if (nvs_get_blob(s_nvs, "ap", data, &n) != ESP_OK || n != 7 || data[6] < 1 || data[6] > 14)
        return false;
    memcpy(bssid, data, 6); *channel = data[6]; return true;
}
void config_set_ap_hint(const uint8_t bssid[6], uint8_t channel) {
    uint8_t old[6], c;
    if (config_get_ap_hint(old, &c) && c == channel && !memcmp(old, bssid, 6)) return;
    uint8_t data[7]; memcpy(data, bssid, 6); data[6] = channel;
    if (nvs_set_blob(s_nvs, "ap", data, sizeof(data)) == ESP_OK) nvs_commit(s_nvs);
}
void config_clear_ap_hint(void) { nvs_erase_key(s_nvs, "ap"); nvs_commit(s_nvs); }
esp_err_t config_save_setup(const char *ssid, const char *pass, const char *server,
                            const char *id, const char *pair) {
    return config_save_setup_with_mode(ssid, pass, server, id, pair, config_get_display_mode());
}
esp_err_t config_save_setup_with_mode(const char *ssid, const char *pass, const char *server,
                                     const char *id, const char *pair, display_mode_t mode) {
    return config_save_setup_transport(ssid, pass, server, id, pair, mode, false, "", "");
}
esp_err_t config_save_setup_transport(const char *ssid, const char *pass, const char *server,
    const char *id, const char *pair, display_mode_t mode, bool relay, const char *url, const char *code) {
    if (!display_mode_valid(mode)) return ESP_ERR_INVALID_ARG;
    if (strlen(url) >= sizeof(s_settings.relay_url) || strlen(code) >= sizeof(s_settings.relay_code))
        return ESP_ERR_INVALID_ARG;
    bool keep_relay = relay && s_settings.relay_enabled && !s_settings.relay_revoked &&
        !strcmp(url, s_settings.relay_url) && !code[0] && (s_settings.relay_has_key || s_settings.relay_code[0]);
    if (relay && !keep_relay && !code[0]) return ESP_ERR_INVALID_ARG;
    settings_t next = keep_relay ? s_settings : (settings_t){.version = 2};
    next.etag[0] = next.relay_etag[0] = 0;
    next.relay_enabled = relay;
    if (relay) {
        strlcpy(next.relay_url, url, sizeof next.relay_url);
        if (!keep_relay) strlcpy(next.relay_code, code, sizeof next.relay_code);
    }
    if (strlen(ssid) >= sizeof(next.ssid) || strlen(pass) >= sizeof(next.pass) ||
        strlen(server) >= sizeof(next.server) || strlen(id) >= sizeof(next.id) ||
        strlen(pair) >= sizeof(next.pair)) return ESP_ERR_INVALID_ARG;
    strlcpy(next.ssid, ssid, sizeof(next.ssid));
    strlcpy(next.pass, pass, sizeof(next.pass));
    strlcpy(next.server, server, sizeof(next.server));
    strlcpy(next.id, id, sizeof(next.id));
    strlcpy(next.pair, pair, sizeof(next.pair));
    if (!strcmp(server, s_settings.server) && !strcmp(id, s_settings.id) && !pair[0])
        strlcpy(next.token, s_settings.token, sizeof(next.token));
    esp_err_t err = nvs_set_u32(s_nvs, "waveform", (uint32_t)mode);
    if (err == ESP_OK) err = nvs_set_u32(s_nvs, "setup_check", 1);
    if (err == ESP_OK) err = persist(&next);
    if (err == ESP_OK) config_clear_ap_hint();
    return err;
}

bool config_relay_configured(void) { return s_settings.relay_enabled != 0; }
bool config_relay_revoked(void) { return s_settings.relay_revoked != 0; }
bool config_relay_ready(void) {
    return config_relay_configured() && !config_relay_revoked() && s_settings.relay_has_key &&
        s_settings.relay_install[0] && s_settings.relay_device[0] && s_settings.relay_token[0];
}
GETTER(config_get_relay_url, relay_url)
GETTER(config_get_relay_code, relay_code)
GETTER(config_get_relay_install, relay_install)
GETTER(config_get_relay_device, relay_device)
GETTER(config_get_relay_token, relay_token)
GETTER(config_get_relay_etag, relay_etag)
GETTER(config_get_relay_config_etag, relay_config_etag)
bool config_get_relay_priv(uint8_t out[32]) {
    if (!s_settings.relay_has_priv) return false;
    memcpy(out, s_settings.relay_priv, 32); return true;
}
bool config_get_relay_key(uint8_t out[32]) {
    if (!config_relay_ready()) return false;
    memcpy(out, s_settings.relay_key, 32); return true;
}
esp_err_t config_set_relay_priv(const uint8_t value[32]) {
    settings_t next=s_settings; memcpy(next.relay_priv,value,32); next.relay_has_priv=1;
    return persist(&next);
}
esp_err_t config_set_relay_paired(const char *install, const char *device, const char *token, const uint8_t key[32]) {
    if (!install[0] || strlen(install)>=sizeof s_settings.relay_install ||
        !device[0] || strlen(device)>=sizeof s_settings.relay_device ||
        !token[0] || strlen(token)>=sizeof s_settings.relay_token) return ESP_ERR_INVALID_ARG;
    settings_t next=s_settings;
    strlcpy(next.relay_install,install,sizeof next.relay_install);
    strlcpy(next.relay_device,device,sizeof next.relay_device);
    strlcpy(next.relay_token,token,sizeof next.relay_token);
    memcpy(next.relay_key,key,32); next.relay_has_key=1; next.relay_revoked=0;
    next.relay_code[0]=next.relay_etag[0]=next.relay_config_etag[0]=0;
    memset(next.relay_priv,0,32); next.relay_has_priv=0;
    return persist(&next);
}
#define RELAY_ETAG_SETTER(name,field) \
    esp_err_t name(const char *v) { \
        if(strlen(v)>=sizeof s_settings.field) return ESP_ERR_INVALID_ARG; \
        if(!strcmp(v,s_settings.field)) return ESP_OK; \
        settings_t next=s_settings; strlcpy(next.field,v,sizeof next.field); return persist(&next); \
    }
RELAY_ETAG_SETTER(config_set_relay_etag,relay_etag)
RELAY_ETAG_SETTER(config_set_relay_config_etag,relay_config_etag)
esp_err_t config_mark_relay_revoked(void) {
    settings_t next=s_settings;
    next.relay_revoked=1; next.relay_has_priv=next.relay_has_key=0;
    memset(next.relay_priv,0,32); memset(next.relay_key,0,32);
    next.relay_code[0]=next.relay_token[0]=next.relay_etag[0]=next.relay_config_etag[0]=0;
    return persist(&next);
}
