// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "config_store.h"
#include "defaults.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

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
typedef struct {
    char server[160], id[64], pair[16], token[80], etag[80];
    uint32_t ever_paired, unpaired;
} rest_profile_t;
typedef struct {
    uint32_t version;
    settings_t current;
    rest_profile_t rest[2];
    uint32_t sleep[3], waveform, setup_check;
} profiles_t;
static settings_t s_settings;
static profiles_t s_profiles;
static const char *TAG = "config";
static config_profile_t profile_of(const settings_t *v) {
    return v->relay_enabled ? CONFIG_PROFILE_RELAY :
        !strcmp(v->server, SETUP_CLOUD_URL) ? CONFIG_PROFILE_CLOUD : CONFIG_PROFILE_LOCAL;
}
config_profile_t config_active_profile(void) { return profile_of(&s_settings); }
static void snapshot_rest(profiles_t *v) {
    config_profile_t mode = profile_of(&v->current);
    if (mode == CONFIG_PROFILE_RELAY) return;
    rest_profile_t *r = &v->rest[mode];
    memcpy(r->server, v->current.server, sizeof r->server);
    memcpy(r->id, v->current.id, sizeof r->id);
    memcpy(r->pair, v->current.pair, sizeof r->pair);
    memcpy(r->token, v->current.token, sizeof r->token);
    memcpy(r->etag, v->current.etag, sizeof r->etag);
    if (r->token[0]) { r->ever_paired = 1; r->unpaired = 0; }
}
static esp_err_t persist_profiles(profiles_t *next) {
    snapshot_rest(next);
    esp_err_t err = nvs_set_blob(s_nvs, "profiles", next, sizeof *next);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) { s_profiles = *next; s_settings = next->current; }
    return err;
}
static esp_err_t persist(const settings_t *next) {
    profiles_t *v = malloc(sizeof *v);
    if (!v) return ESP_ERR_NO_MEM;
    *v = s_profiles;
    v->current = *next;
    esp_err_t err = persist_profiles(v);
    free(v);
    return err;
}
static bool valid_settings(const settings_t *v) {
    if (v->relay_enabled > 1 || v->relay_revoked > 1 ||
        v->relay_has_priv > 1 || v->relay_has_key > 1 ||
        !memchr(v->relay_url, 0, sizeof v->relay_url) ||
        !memchr(v->relay_code, 0, sizeof v->relay_code) ||
        !memchr(v->relay_install, 0, sizeof v->relay_install) ||
        !memchr(v->relay_device, 0, sizeof v->relay_device) ||
        !memchr(v->relay_token, 0, sizeof v->relay_token) ||
        !memchr(v->relay_etag, 0, sizeof v->relay_etag) ||
        !memchr(v->relay_config_etag, 0, sizeof v->relay_config_etag)) return false;
    if (!memchr(v->ssid, 0, sizeof(v->ssid)) ||
        !memchr(v->pass, 0, sizeof(v->pass)) ||
        !memchr(v->server, 0, sizeof(v->server)) ||
        !memchr(v->id, 0, sizeof(v->id)) ||
        !memchr(v->pair, 0, sizeof(v->pair)) ||
        !memchr(v->token, 0, sizeof(v->token)) ||
        !memchr(v->etag, 0, sizeof(v->etag))) return false;
    return true;
}
static bool valid_rest(const rest_profile_t *v) {
    return memchr(v->server, 0, sizeof v->server) && memchr(v->id, 0, sizeof v->id) &&
        memchr(v->pair, 0, sizeof v->pair) && memchr(v->token, 0, sizeof v->token) &&
        memchr(v->etag, 0, sizeof v->etag) && v->ever_paired <= 1 && v->unpaired <= 1;
}
esp_err_t config_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    err = nvs_open("note4c", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    memset(&s_profiles, 0, sizeof s_profiles);
    size_t size = sizeof s_profiles;
    err = nvs_get_blob(s_nvs, "profiles", &s_profiles, &size);
    if (err == ESP_OK) {
        if (size != sizeof s_profiles || s_profiles.version != 3 ||
            s_profiles.current.version != 2 || !valid_settings(&s_profiles.current) ||
            !valid_rest(&s_profiles.rest[0]) || !valid_rest(&s_profiles.rest[1]) ||
            s_profiles.setup_check > 1) return ESP_ERR_INVALID_STATE;
        s_settings = s_profiles.current;
        return ESP_OK;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    memset(&s_settings, 0, sizeof s_settings);
    size = sizeof s_settings;
    err = nvs_get_blob(s_nvs, "settings", &s_settings, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_settings, 0, sizeof s_settings);
        s_settings.version = 2;
    } else {
        if (err != ESP_OK) return err;
        if (size == sizeof(settings_v1_t) && s_settings.version == 1) s_settings.version = 2;
        else if (size != sizeof s_settings || s_settings.version != 2) return ESP_ERR_INVALID_STATE;
        if (!valid_settings(&s_settings)) return ESP_ERR_INVALID_STATE;
    }
    s_profiles.version = 3;
    s_profiles.current = s_settings;
    nvs_get_u32(s_nvs, "waveform", &s_profiles.waveform);
    nvs_get_u32(s_nvs, "setup_check", &s_profiles.setup_check);
    nvs_get_u32(s_nvs, "sleep", &s_profiles.sleep[config_active_profile()]);
    strlcpy(s_profiles.rest[CONFIG_PROFILE_CLOUD].server, SETUP_CLOUD_URL,
            sizeof s_profiles.rest[CONFIG_PROFILE_CLOUD].server);
    snapshot_rest(&s_profiles);
    return ESP_OK;
}
void config_get_profile(config_profile_t mode, config_profile_view_t *out) {
    memset(out, 0, sizeof *out);
    if (mode == CONFIG_PROFILE_RELAY) {
        strlcpy(out->server, s_settings.relay_url, sizeof out->server);
        strlcpy(out->id, s_settings.relay_device, sizeof out->id);
        out->paired = !s_settings.relay_revoked && s_settings.relay_has_key &&
            s_settings.relay_install[0] && s_settings.relay_device[0] && s_settings.relay_token[0];
        out->pending = !s_settings.relay_revoked && s_settings.relay_code[0];
    } else if (mode == CONFIG_PROFILE_LOCAL || mode == CONFIG_PROFILE_CLOUD) {
        const rest_profile_t *r = &s_profiles.rest[mode];
        strlcpy(out->server, r->server, sizeof out->server);
        strlcpy(out->id, r->id, sizeof out->id);
        out->paired = r->token[0] != 0;
        out->pending = r->pair[0] != 0;
    }
}
bool config_rest_unpaired(void) {
    return !config_relay_configured() && s_profiles.rest[config_active_profile()].unpaired;
}
esp_err_t config_note_rest_unregistered(void) {
    if (config_relay_configured()) return ESP_ERR_INVALID_STATE;
    const rest_profile_t *r = &s_profiles.rest[config_active_profile()];
    if (!r->ever_paired || r->unpaired || r->token[0]) return ESP_OK;
    profiles_t *v = malloc(sizeof *v);
    if (!v) return ESP_ERR_NO_MEM;
    *v = s_profiles;
    v->rest[config_active_profile()].unpaired = 1;
    esp_err_t err = persist_profiles(v);
    free(v);
    return err;
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
    uint32_t mode = s_profiles.waveform;
    return mode <= DISPLAY_FAST_10S && display_mode_valid((display_mode_t)mode)
        ? (display_mode_t)mode : DISPLAY_NATIVE;
}
esp_err_t config_save_display_mode(display_mode_t mode) {
    if (!display_mode_valid(mode)) return ESP_ERR_INVALID_ARG;
    profiles_t *v = malloc(sizeof *v);
    if (!v) return ESP_ERR_NO_MEM;
    *v = s_profiles;
    v->current.etag[0] = v->current.relay_etag[0] = 0;
    v->waveform = mode;
    esp_err_t err = persist_profiles(v);
    free(v);
    return err;
}
bool config_setup_check_pending(void) { return s_profiles.setup_check == 1; }
esp_err_t config_finish_setup_check(void) {
    if (!config_setup_check_pending()) return ESP_OK;
    profiles_t *v = malloc(sizeof *v);
    if (!v) return ESP_ERR_NO_MEM;
    *v = s_profiles; v->setup_check = 0;
    esp_err_t err = persist_profiles(v);
    free(v);
    return err;
}
uint32_t config_get_sleep_s(uint32_t fallback) {
    uint32_t v = s_profiles.sleep[config_active_profile()];
    return v >= SLEEP_INTERVAL_MIN_S && v <= SLEEP_INTERVAL_MAX_S ? v : fallback;
}
void config_set_sleep_s(uint32_t value) {
    if (value < SLEEP_INTERVAL_MIN_S || value > SLEEP_INTERVAL_MAX_S ||
        value == config_get_sleep_s(0)) return;
    profiles_t *v = malloc(sizeof *v);
    if (!v) return;
    *v = s_profiles; v->sleep[config_active_profile()] = value;
    esp_err_t err = persist_profiles(v);
    free(v);
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
    if (!display_mode_valid(mode) || strlen(url) >= sizeof s_settings.relay_url ||
        strlen(code) >= sizeof s_settings.relay_code || strlen(ssid) >= sizeof s_settings.ssid ||
        strlen(pass) >= sizeof s_settings.pass || strlen(server) >= sizeof s_settings.server ||
        strlen(id) >= sizeof s_settings.id || strlen(pair) >= sizeof s_settings.pair)
        return ESP_ERR_INVALID_ARG;
    config_profile_t mode_id = relay ? CONFIG_PROFILE_RELAY :
        !strcmp(server, SETUP_CLOUD_URL) ? CONFIG_PROFILE_CLOUD : CONFIG_PROFILE_LOCAL;
    bool keep_relay = relay && !s_settings.relay_revoked && !strcmp(url, s_settings.relay_url) &&
        !code[0] && (s_settings.relay_has_key || s_settings.relay_code[0]);
    if (relay && !keep_relay && !code[0]) return ESP_ERR_INVALID_ARG;
    profiles_t *v = malloc(sizeof *v);
    if (!v) return ESP_ERR_NO_MEM;
    *v = s_profiles;
    settings_t *next = &v->current;
    next->relay_enabled = relay;
    next->etag[0] = next->relay_etag[0] = 0;
    if (relay) {
        if (!keep_relay) {
            memset((char *)next + offsetof(settings_t, relay_revoked), 0,
                   sizeof *next - offsetof(settings_t, relay_revoked));
            strlcpy(next->relay_url, url, sizeof next->relay_url);
            strlcpy(next->relay_code, code, sizeof next->relay_code);
            v->sleep[mode_id] = 0;
        }
        next->server[0] = next->id[0] = next->pair[0] = next->token[0] = 0;
    } else {
        rest_profile_t *r = &v->rest[mode_id];
        bool keep = !strcmp(server, r->server) && !strcmp(id, r->id) && !pair[0];
        if (!keep) { memset(r, 0, sizeof *r); v->sleep[mode_id] = 0; }
        strlcpy(next->server, server, sizeof next->server);
        strlcpy(next->id, id, sizeof next->id);
        strlcpy(next->pair, keep ? r->pair : pair, sizeof next->pair);
        strlcpy(next->token, r->token, sizeof next->token);
    }
    strlcpy(next->ssid, ssid, sizeof next->ssid);
    strlcpy(next->pass, pass, sizeof next->pass);
    v->waveform = mode;
    v->setup_check = 1;
    esp_err_t err = persist_profiles(v);
    free(v);
    if (err == ESP_OK) config_clear_ap_hint();
    return err;
}

bool config_relay_configured(void) { return s_settings.relay_enabled != 0; }
bool config_relay_revoked(void) { return config_relay_configured() && s_settings.relay_revoked != 0; }
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
