// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>

#include "relay_wire.h"

#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include "cJSON.h"

static bool copy_field(char *dst, size_t cap, const cJSON *v)
{
    if (!cJSON_IsString(v) || !v->valuestring[0]) return false;
    size_t n = strlen(v->valuestring);
    if (n >= cap) return false;
    memcpy(dst, v->valuestring, n + 1);
    return true;
}

static bool path_id(const char *value) {
    if (!value || !value[0]) return false;
    return strspn(value,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == strlen(value);
}

relay_ready_t relay_parse_ready(const char *json, size_t len,
                                relay_pairing_t *out)
{
    if (!json || !out) return RELAY_READY_MALFORMED;
    memset(out, 0, sizeof *out);

    cJSON *r = cJSON_ParseWithLength(json, len);
    if (!r) return RELAY_READY_MALFORMED;
    relay_ready_t rc = RELAY_READY_MALFORMED;

    const cJSON *status = cJSON_GetObjectItemCaseSensitive(r, "status");
    if (!cJSON_IsString(status)) goto done;
    if (strcmp(status->valuestring, "ready") != 0) {

        if (!strcmp(status->valuestring, "pending")) rc = RELAY_READY_PENDING;
        goto done;
    }

    const cJSON *config = cJSON_GetObjectItemCaseSensitive(r, "config");
    const cJSON *inst   = cJSON_GetObjectItemCaseSensitive(r, "install_id");
    if (!cJSON_IsString(inst) && cJSON_IsObject(config))
        inst = cJSON_GetObjectItemCaseSensitive(config, "install_id");

    if (!copy_field(out->install_id, sizeof out->install_id, inst) ||
        !copy_field(out->device_id, sizeof out->device_id,
                    cJSON_GetObjectItemCaseSensitive(r, "device_id")) ||
        !copy_field(out->device_token, sizeof out->device_token,
                    cJSON_GetObjectItemCaseSensitive(r, "device_token")))
        goto done;

    if (!path_id(out->install_id) || !path_id(out->device_id)) goto done;
    for (const unsigned char *p = (const unsigned char *)out->device_token; *p; p++)
        if (*p <= 32 || *p == 127) goto done;
    const cJSON *hp = cJSON_GetObjectItemCaseSensitive(r, "home_pubkey");
    if (!cJSON_IsString(hp)) goto done;
    size_t n = 0;
    if (!relay_b64url_decode(out->home_pub, sizeof out->home_pub, &n,
                             hp->valuestring) || n != RELAY_PUB_LEN)
        goto done;

    rc = RELAY_READY_OK;

done:
    cJSON_Delete(r);
    if (rc != RELAY_READY_OK) memset(out, 0, sizeof *out);
    return rc;
}

bool relay_build_pair_body(char *out, size_t cap,
                           const char *code, const char *pubkey_b64,
                           int panel_w, int panel_h,
                           const char *model, const char *gamut)
{
    if (!out || !cap) return false;
    out[0] = '\0';
    if (!code || !code[0] || !pubkey_b64 || !pubkey_b64[0]) return false;

    cJSON *o = cJSON_CreateObject();
    if (!o) return false;
    bool ok = cJSON_AddStringToObject(o, "code", code) &&
              cJSON_AddStringToObject(o, "panel_pubkey", pubkey_b64);

    if (ok && panel_w > 0) ok = cJSON_AddNumberToObject(o, "panel_w", panel_w);
    if (ok && panel_h > 0) ok = cJSON_AddNumberToObject(o, "panel_h", panel_h);
    if (ok && model && model[0]) ok = cJSON_AddStringToObject(o, "model", model);
    if (ok && gamut && gamut[0]) ok = cJSON_AddStringToObject(o, "gamut", gamut);

    char *body = ok ? cJSON_PrintUnformatted(o) : NULL;
    cJSON_Delete(o);
    if (!body) return false;

    size_t n = strlen(body);
    if (n >= cap) { cJSON_free(body); return false; }
    memcpy(out, body, n + 1);
    cJSON_free(body);
    return true;
}

bool relay_build_status_body(char *out, size_t cap,
                             const char *device_id, const char *fw_version,
                             int panel_w, int panel_h,
                             const char *ip, int rssi,
                             int battery_mv, int battery_pct,
                             const char *button, uint64_t button_event_id)
{
    if (!out || !cap) return false;
    out[0] = '\0';

    cJSON *o = cJSON_CreateObject();
    if (!o) return false;

    bool ok = cJSON_AddStringToObject(o, "device_id", device_id ? device_id : "") &&
              cJSON_AddStringToObject(o, "fw_version", fw_version ? fw_version : "");
    if (ok && panel_w > 0) ok = cJSON_AddNumberToObject(o, "panel_w", panel_w);
    if (ok && panel_h > 0) ok = cJSON_AddNumberToObject(o, "panel_h", panel_h);
    if (ok && ip && ip[0]) ok = cJSON_AddStringToObject(o, "ip", ip);
    if (ok && rssi != 0)   ok = cJSON_AddNumberToObject(o, "rssi", rssi);
    if (ok && battery_mv > 0) {
        ok = cJSON_AddNumberToObject(o, "battery_mv", battery_mv) &&
             cJSON_AddNumberToObject(o, "battery_pct", battery_pct);
    }

    if (ok && button && button[0]) {
        ok = cJSON_AddStringToObject(o, "button", button) &&
             cJSON_AddNumberToObject(o, "button_event_id", (double)button_event_id);
    }

    char *body = ok ? cJSON_PrintUnformatted(o) : NULL;
    cJSON_Delete(o);
    if (!body) return false;

    size_t n = strlen(body);
    if (n >= cap) { cJSON_free(body); return false; }
    memcpy(out, body, n + 1);
    cJSON_free(body);
    return true;
}

bool relay_parse_config_etag(const char *json, size_t len,
                             char *out, size_t cap)
{
    if (!out || !cap) return false;
    out[0] = '\0';
    if (!json || !len) return false;

    cJSON *r = cJSON_ParseWithLength(json, len);
    if (!r) return false;
    bool ok = copy_field(out, cap,
                         cJSON_GetObjectItemCaseSensitive(r, "config_etag"));
    cJSON_Delete(r);
    if (!ok) out[0] = '\0';
    return ok;
}

static int32_t opt_int(const cJSON *o, const char *key, int32_t absent)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!v) return absent;
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < 0 ||
        v->valuedouble > INT32_MAX || floor(v->valuedouble) != v->valuedouble) return -2;
    return (int32_t)v->valuedouble;
}

bool relay_parse_config(const char *json, size_t len, relay_devcfg_t *out)
{
    if (!out) return false;
    out->sleep_interval_s = -1;
    out->button_wake_s    = -1;
    out->always_on        = -1;
    if (!json || !len) return false;

    cJSON *r = cJSON_ParseWithLength(json, len);
    if (!r) return false;
    if (!cJSON_IsObject(r)) { cJSON_Delete(r); return false; }

    out->sleep_interval_s = opt_int(r, "sleep_interval_s", -1);
    out->button_wake_s    = opt_int(r, "button_wake_s", -1);

    const cJSON *ao = cJSON_GetObjectItemCaseSensitive(r, "always_on");
    if (cJSON_IsBool(ao))        out->always_on = cJSON_IsTrue(ao) ? 1 : 0;
    else if (cJSON_IsNumber(ao)) out->always_on = ao->valueint ? 1 : 0;

    cJSON_Delete(r);
    return true;
}

bool relay_mailbox_url(char *out, size_t cap, const char *base,
                       const char *install_id, const char *device_id,
                       const char *leaf)
{
    if (!out || !cap) return false;
    out[0] = '\0';
    if (!base || !base[0] || !install_id || !install_id[0] ||
        !device_id || !device_id[0] || !leaf || !leaf[0]) return false;

    if (!path_id(install_id) || !path_id(device_id) || !path_id(leaf)) return false;
    int n = snprintf(out, cap, "%s/v1/i/%s/d/%s/%s",
                     base, install_id, device_id, leaf);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return false;
    }
    return true;
}
