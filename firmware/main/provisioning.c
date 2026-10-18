// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "provisioning.h"
#include "provision_form.h"
#include "setup_request.h"
#include "relay.h"
#include "portal_view.h"
#include "config_store.h"
#include "setup_identity.h"
#include "defaults.h"
#include "power.h"
#include "splash.h"
#include "wifi_manager.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>

static const char k_portal_html[] =
"<!doctype html>\n"
"<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->\n"
"<!-- Copyright (C) 2026 varanu5 <https://github.com/varanu5> -->\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">\n"
"<title>ZecTrix NOTE4C Devkit setup</title>\n"
"<style>\n"
":root{color-scheme:light;--bg:#f1f0ec;--surface:#fff;--ink:#18181b;--muted:#62625f;--accent:#0d8c7e;--accent-hover:#0a6f63;--soft:#e6f3f1;--border:#deded8}\n"
"*{box-sizing:border-box}\n"
"body{margin:0;padding:24px 16px max(28px,env(safe-area-inset-bottom));background:var(--bg);color:var(--ink);font:15px/1.45 -apple-system,BlinkMacSystemFont,\"Segoe UI\",system-ui,sans-serif;-webkit-text-size-adjust:100%}\n"
"main{max-width:480px;margin:auto}\n"
".brand{display:flex;gap:10px;align-items:center;font-size:21px;font-weight:700;letter-spacing:-.02em}\n"
".brand svg{flex:none;width:34px;height:34px}\n"
"h1{font-size:14px;font-weight:400;color:var(--muted);margin:6px 0 18px}\n"
"p{margin:0}.hint,.muted{color:var(--muted)}.hint{font-size:12px;line-height:1.5;margin-top:6px}\n"
".status{display:grid;grid-template-columns:auto 1fr;gap:7px 14px;font-size:12px;border:1px solid var(--border);border-radius:10px;background:var(--surface);padding:14px 16px;margin:0 0 14px}\n"
".status dt{font-weight:600}.status dd{margin:0;color:var(--muted);overflow-wrap:anywhere}.status code{font-size:12px}\n"
".card{border:1px solid var(--border);border-radius:12px;background:var(--surface);padding:18px 16px;margin-bottom:14px}\n"
"[hidden]{display:none!important}\n"
".card h2{font-size:11px;font-weight:600;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);margin:0 0 14px}\n"
".field{margin-bottom:16px}.field:last-child{margin-bottom:0}\n"
"label{display:block;font-weight:600;font-size:13px;margin-bottom:6px}\n"
"input:not([type=checkbox]):not([type=radio]),select{width:100%;min-height:44px;border:1px solid var(--border);border-radius:7px;background:var(--surface);padding:10px 12px;color:var(--ink);font:inherit;font-size:16px;line-height:1.3}\n"
"input:focus-visible,select:focus-visible,button:focus-visible{outline:3px solid #a8d8d0;outline-offset:2px}\n"
"#relay-url::placeholder{color:var(--muted);opacity:1}\n"
"input:disabled{background:#f5f5f2;color:var(--muted)}\n"
".password{position:relative}.password input{padding-right:66px!important}.password button{position:absolute;right:3px;top:3px;min-height:38px;padding:0 12px;border:0;border-radius:5px;background:transparent;color:var(--accent);font:inherit;font-size:13px;font-weight:600;cursor:pointer}\n"
".password button:hover{background:var(--soft)}\n"
".check{display:flex;align-items:center;gap:8px;font-size:12px;font-weight:400;margin:10px 0 0}.check input{flex:none;width:18px;height:18px;margin:0;accent-color:var(--accent)}\n"
"fieldset{border:0;margin:0;padding:0;min-width:0}\n"
".radio{display:flex;align-items:flex-start;gap:12px;padding:12px;border:1px solid var(--border);border-radius:8px;margin:0 0 9px;cursor:pointer;font-weight:400}\n"
".radio:last-child{margin-bottom:0}.radio input{flex:none;width:18px;height:18px;margin:2px 0 0;accent-color:var(--accent)}\n"
".radio:has(input:checked){background:var(--soft);border-color:var(--accent)}\n"
".radio strong{font-size:14px;font-weight:600}.radio .hint{display:block;margin-top:3px}\n"
".badge{display:inline-block;vertical-align:1px;font-size:10px;font-weight:500;color:var(--muted);border:1px solid var(--border);border-radius:4px;padding:1px 5px;margin-left:5px}\n"
".refresh-help{margin-top:12px}.refresh-help p+p{margin-top:6px}\n"
".submit{width:100%;min-height:48px;border:0;border-radius:8px;padding:12px 16px;background:var(--accent);color:white;font:inherit;font-size:15px;font-weight:600;cursor:pointer}\n"
".submit:hover{background:var(--accent-hover)}button:disabled{opacity:.6;cursor:wait}\n"
".notice{padding:12px 14px;background:#fff8dd;border:1px solid #dec781;border-radius:10px;font-size:13px;margin-bottom:14px;overflow-wrap:anywhere}.notice:empty{display:none}\n"
"#progress{padding:2px 4px}footer{text-align:center;color:var(--muted);font-size:11px;line-height:1.8;margin:22px 0 0}footer span{font-family:ui-monospace,SFMono-Regular,Menlo,monospace}code{overflow-wrap:anywhere}\n"
"</style>\n"
"</head>\n"
"<body><main>\n"
"<header>\n"
"<div class=\"brand\">\n"
"<svg viewBox=\"0 0 256 256\" width=\"34\" height=\"34\" aria-hidden=\"true\">\n"
"<defs><linearGradient id=\"brand-bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\"><stop offset=\"0\" stop-color=\"#0d8c7e\"/><stop offset=\"1\" stop-color=\"#0a6f63\"/></linearGradient><clipPath id=\"brand-inner\"><rect x=\"55\" y=\"55\" width=\"146\" height=\"146\" rx=\"27\"/></clipPath></defs>\n"
"<rect width=\"256\" height=\"256\" rx=\"72\" fill=\"url(#brand-bg)\"/>\n"
"<g clip-path=\"url(#brand-inner)\" fill=\"white\" fill-opacity=\".85\"><rect x=\"128\" y=\"55\" width=\"73\" height=\"73\"/><rect x=\"55\" y=\"128\" width=\"73\" height=\"73\"/></g>\n"
"</svg><span>Tesserae</span>\n"
"</div><h1>Device setup</h1>\n"
"</header>\n"
"<dl class=\"status\"><dt>Device</dt><dd>ZecTrix NOTE4C Devkit</dd><dt>Firmware</dt><dd><code>{{version}}</code></dd><dt>Connection</dt><dd>REST or Cloud relay</dd></dl>\n"
"<div class=\"notice\" role=\"alert\">{{error}}</div>\n"
"<form method=\"post\" action=\"/save\" id=\"setup-form\">\n"
"<input type=\"hidden\" name=\"nonce\" value=\"{{nonce}}\">\n"
"<section class=\"card\"><h2>Wi-Fi network</h2>\n"
"<div class=\"field\"><label for=\"ssid\">Network name (SSID)</label><input id=\"ssid\" name=\"ssid\" value=\"{{ssid}}\" maxlength=\"32\" required placeholder=\"my-home-wifi\" autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\"></div>\n"
"<div class=\"field\"><label for=\"network\">Or pick a nearby network</label><select id=\"network\"><option value=\"\">Choose a network</option>{{networks}}</select><p class=\"hint\">Password-protected 2.4 GHz networks found when setup opened. For a hidden network, enter its name above.</p></div>\n"
"<div class=\"field\"><label for=\"pass\">Password</label><div class=\"password\"><input id=\"pass\" name=\"pass\" type=\"password\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\"><button type=\"button\" id=\"toggle\" aria-label=\"Show password\" aria-controls=\"pass\" aria-pressed=\"false\">Show</button></div><p class=\"hint\">Password required for a new network. Leave blank to keep the saved password for the same network.</p></div>\n"
"</section>\n"
"<section class=\"card\"><h2 id=\"refresh-title\">Refresh speed</h2>\n"
"<fieldset aria-labelledby=\"refresh-title\">\n"
"<label class=\"radio\" for=\"wf-5s\"><input type=\"radio\" name=\"waveform\" value=\"5s\" id=\"wf-5s\" {{wave_5s}}><span><strong>5 s</strong><span class=\"badge\">Experimental</span><span class=\"hint\">Fastest updates. Colors may look softer than Native.</span></span></label>\n"
"<label class=\"radio\" for=\"wf-10s\"><input type=\"radio\" name=\"waveform\" value=\"10s\" id=\"wf-10s\" {{wave_10s}}><span><strong>10 s</strong><span class=\"badge\">Experimental</span><span class=\"hint\">A slower fast mode. Compare color quality on your display.</span></span></label>\n"
"<label class=\"radio\" for=\"wf-native\"><input type=\"radio\" name=\"waveform\" value=\"native\" id=\"wf-native\" {{wave_native}}><span><strong>Native</strong><span class=\"badge\">Default</span><span class=\"hint\">Uses the panel's built-in waveform. Refresh takes longer.</span></span></label>\n"
"</fieldset>\n"
"<div class=\"refresh-help\"><p class=\"hint\">Your refresh choice is saved with the other settings using Save &amp; restart below. The selected speed applies to images and all built-in screens. Native is the default until you choose another speed. Hold the front button for 20 seconds and release at the three-flash cue to return to setup.</p></div>\n"
"</section>\n"
"<section class=\"card\"><h2>Device</h2>\n"
"<div class=\"field\"><label for=\"id\">Device ID <span class=\"muted\">(optional)</span></label><input id=\"id\" name=\"id\" value=\"{{id}}\" maxlength=\"32\" pattern=\"[a-z][a-z0-9_-]{1,31}\" placeholder=\"note4c-kitchen\" autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\"><p class=\"hint\">Leave blank to keep the current ID. New devices get a name based on their MAC address, such as note4c-a1b2c3.</p></div>\n"
"</section>\n"
"<section class=\"card\"><h2>Connection</h2>\n"
"<label class=\"radio\"><input type=\"radio\" name=\"transport\" value=\"rest\" id=\"transport-rest\" {{transport_rest}}><span><strong>REST</strong><span class=\"hint\">Connect directly to your Tesserae server.</span></span></label>\n"
"<label class=\"radio\"><input type=\"radio\" name=\"transport\" value=\"relay\" id=\"transport-relay\" {{transport_relay}}><span><strong>Cloud relay</strong><span class=\"hint\">Receive encrypted images through a relay.</span></span></label>\n"
"</section>\n"
"<section class=\"card\" id=\"rest-fields\"><h2>Tesserae server</h2>\n"
"<div class=\"field\"><label for=\"server\">Server URL</label><input id=\"server\" name=\"server\" value=\"{{server}}\" maxlength=\"159\" required placeholder=\"http://tesserae.local:8765\" autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\"><p class=\"hint\">Enter your Tesserae server address, starting with http:// or https://. Your display must be able to connect to it. For HTTPS, use a certificate from a trusted certificate provider.</p></div>\n"
"<div class=\"field\"><label for=\"pair\">Pairing code <span class=\"muted\">(optional)</span></label><input id=\"pair\" name=\"pair\" inputmode=\"numeric\" maxlength=\"6\" pattern=\"[0-9]{6}\" placeholder=\"6-digit code\" autocomplete=\"off\"><p class=\"hint\">Leave blank and register in Settings → Devices, or enter a code from Pair new device. An existing pairing is kept when the server and device ID stay the same.</p></div>\n"
"</section>\n"
"<section class=\"card\" id=\"relay-fields\"><h2>Cloud relay</h2>\n"
"<div class=\"field\"><label for=\"relay-url\">Relay URL</label><input id=\"relay-url\" name=\"relay_url\" value=\"{{relay_url}}\" placeholder=\"https://relay.tesserae.ink\" maxlength=\"159\" required autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\"><p class=\"hint\">Use the relay address configured in your Tesserae server. HTTPS requires a trusted certificate.</p></div>\n"
"<div class=\"field\"><label for=\"relay-code\">Relay pairing code</label><input id=\"relay-code\" name=\"relay_code\" maxlength=\"32\" pattern=\"[A-Za-z0-9-]+\" autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\"><p class=\"hint\">Create a cloud relay pairing code in Tesserae and enter it here. Leave blank to keep an existing relay pairing at the same address.</p></div>\n"
"<p class=\"hint\">If this device is removed from the relay, its screen will ask you to hold the front button for 20 seconds to open setup.</p>\n"
"</section>\n"
"<button class=\"submit\" id=\"submit\" type=\"submit\">Save &amp; restart</button>\n"
"<p class=\"hint\" id=\"progress\" role=\"status\">Saves all settings above. After restarting, the display checks its connection. If setup fails, reconnect to its setup Wi-Fi for details.</p>\n"
"</form>\n"
"<footer>Dashboard companion · <span>dmellok/tesserae</span><br>ZecTrix NOTE4C Devkit firmware · <span>varanu5</span></footer>\n"
"<script>\n"
"function updateTransport(){\n"
"const relay=document.getElementById('transport-relay').checked;\n"
"for(const [id,active] of [['rest-fields',!relay],['relay-fields',relay]]){\n"
"const section=document.getElementById(id);section.hidden=!active;\n"
"section.querySelectorAll('input').forEach(input=>{input.disabled=!active;});\n"
"}}\n"
"document.getElementById('transport-rest').addEventListener('change',updateTransport);\n"
"document.getElementById('transport-relay').addEventListener('change',updateTransport);\n"
"updateTransport();\n"
"const ssid=document.getElementById('ssid'),pw=document.getElementById('pass');\n"
"document.getElementById('network').addEventListener('change',e=>{const o=e.target.selectedOptions[0];if(!o.value)return;ssid.value=o.value;pw.value='';pw.focus();});\n"
"document.getElementById('toggle').addEventListener('click',e=>{const show=pw.type==='password';pw.type=show?'text':'password';e.target.textContent=show?'Hide':'Show';e.target.setAttribute('aria-label',show?'Hide password':'Show password');e.target.setAttribute('aria-pressed',String(show));});\n"
"document.getElementById('setup-form').addEventListener('submit',()=>{const button=document.getElementById('submit');button.disabled=true;button.textContent='Saving…';document.getElementById('progress').textContent='Saving all settings. Keep this page open until the display restarts.';});\n"
"</script>\n"
"</main></body>\n"
"</html>\n"
;
static char s_nonce[33];
static atomic_bool s_saved, s_dns_stop, s_dns_running;
static const char *s_note;
static const char *TAG="setup";
#define SCAN_MAX 20
static struct { char ssid[33]; int rssi; } s_scan[SCAN_MAX];
static unsigned s_scan_count;

static void scan_networks(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_scan_config_t scan={0};
    esp_err_t err=ESP_ERR_WIFI_STATE;
    for (unsigned i=0;i<15 && err==ESP_ERR_WIFI_STATE;i++) {
        err=esp_wifi_scan_start(&scan,true);
        if (err==ESP_ERR_WIFI_STATE) vTaskDelay(pdMS_TO_TICKS(100));
    }
    wifi_ap_record_t *records=calloc(SCAN_MAX,sizeof(*records));
    uint16_t n=SCAN_MAX;
    if (err==ESP_OK && records) err=esp_wifi_scan_get_ap_records(&n,records);
    else { esp_wifi_clear_ap_list(); n=0; }
    if (err==ESP_OK) for (unsigned i=0;i<n;i++) {
        records[i].ssid[32]=0;
        if (!records[i].ssid[0]) continue;
        wifi_auth_mode_t auth = records[i].authmode;
        if (auth != WIFI_AUTH_WPA2_PSK && auth != WIFI_AUTH_WPA_WPA2_PSK &&
            auth != WIFI_AUTH_WPA3_PSK && auth != WIFI_AUTH_WPA2_WPA3_PSK) continue;
        bool duplicate=false;
        for (unsigned j=0;j<s_scan_count;j++)
            if (!strcmp(s_scan[j].ssid,(char *)records[i].ssid)) duplicate=true;
        if (duplicate) continue;
        strlcpy(s_scan[s_scan_count].ssid,(char *)records[i].ssid,33);
        s_scan[s_scan_count].rssi=records[i].rssi;
        s_scan_count++;
    }
    if (err!=ESP_OK) { esp_wifi_clear_ap_list(); ESP_LOGW(TAG,"network scan unavailable"); }
    free(records);
    ESP_ERROR_CHECK(esp_wifi_stop());
}

typedef struct {
    httpd_req_t *req;
    char ssid[200], server[960], relay_url[960], id[200], error[768], networks[8192];
    bool relay;
    display_mode_t display_mode;
} view_t;
static const char *view_value(const char *key,void *context) {
    view_t *v=context;
    if (!strcmp(key,"ssid")) return v->ssid;
    if (!strcmp(key,"server")) return v->server;
    if (!strcmp(key,"relay_url")) return v->relay_url;
    if (!strcmp(key,"transport_rest")) return v->relay ? "" : "checked";
    if (!strcmp(key,"transport_relay")) return v->relay ? "checked" : "";
    if (!strcmp(key,"id")) return v->id;
    if (!strcmp(key,"error")) return v->error;
    if (!strcmp(key,"networks")) return v->networks;
    if (!strcmp(key,"nonce")) return s_nonce;
    if (!strcmp(key,"version")) return FW_VERSION;
    if (!strcmp(key,"wave_native")) return v->display_mode == DISPLAY_NATIVE ? "checked" : "";
    if (!strcmp(key,"wave_5s")) return v->display_mode == DISPLAY_FAST_5S ? "checked" : "";
    if (!strcmp(key,"wave_10s")) return v->display_mode == DISPLAY_FAST_10S ? "checked" : "";
    return NULL;
}
static bool view_send(const char *data,size_t n,void *context) {
    if (!n) return true;
    return httpd_resp_send_chunk(((view_t *)context)->req,data,n)==ESP_OK;
}
static esp_err_t form(httpd_req_t *req,const char *error,const setup_request_t *submitted) {
    view_t *v=calloc(1,sizeof(*v));
    if (!v) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Not enough memory. Retry setup.");
    v->req=req;
    v->display_mode=config_get_display_mode();
    setup_request_t fields={0};
    if (submitted) fields=*submitted;
    else {
        config_get_wifi(fields.ssid,sizeof fields.ssid,fields.pass,sizeof fields.pass);
        config_get_server_url(fields.server,sizeof fields.server);
        fields.relay = config_relay_configured();
        config_get_relay_url(fields.relay_url,sizeof fields.relay_url);
        config_get_device_id(fields.id,sizeof fields.id);
    }
    v->relay = fields.relay;
    provform_html_escape(fields.relay_url,v->relay_url,sizeof v->relay_url);
    if (fields.has_display_mode) v->display_mode=fields.display_mode;
    provform_html_escape(fields.ssid,v->ssid,sizeof v->ssid);
    provform_html_escape(fields.server,v->server,sizeof v->server);
    provform_html_escape(fields.id,v->id,sizeof v->id);
    provform_html_escape(error ? error : "",v->error,sizeof v->error);
    memset(&fields,0,sizeof fields);
    size_t used=0;
    for (unsigned i=0;i<s_scan_count;i++) {
        char escaped[200]; provform_html_escape(s_scan[i].ssid,escaped,sizeof escaped);
        int n=snprintf(v->networks+used,sizeof v->networks-used,
            "<option value=\"%s\">%s (%d dBm)</option>",
            escaped,escaped,s_scan[i].rssi);
        if (n<0 || (size_t)n>=sizeof v->networks-used) { v->networks[used]=0; break; }
        used+=(size_t)n;
    }
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    bool ok=portal_render(k_portal_html,view_value,view_send,v);
    free(v);
    return ok ? httpd_resp_send_chunk(req,NULL,0) : ESP_FAIL;
}
static esp_err_t page(httpd_req_t *req) { return form(req,s_note,NULL); }
static esp_err_t redirect(httpd_req_t *req) {
    httpd_resp_set_status(req,"302 Found");
    httpd_resp_set_hdr(req,"Location","http://192.168.4.1/");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,NULL,0);
}
static esp_err_t saved(httpd_req_t *req) {
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(req,
        "<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Settings saved</title><style>body{font:17px/1.5 system-ui;background:#f1f0ec;color:#18181b;padding:40px 20px}"
        "main{max-width:420px;margin:auto;background:white;padding:28px;border-radius:12px}h1{color:#0d8c7e;font-size:26px}</style>"
        "<main><h1>Settings saved</h1><p>Your ZecTrix NOTE4C Devkit is restarting and checking its connection.</p>"
        "<p>Return to your home Wi-Fi and open Tesserae. For REST, register the device in Settings &rarr; Devices if needed. Cloud relay pairing completes automatically when your server is online.</p>"
        "<p>If the display returns to Wi-Fi setup, reconnect to its setup network and open 192.168.4.1 for details.</p></main></html>");
    atomic_store(&s_saved,true);
    return result;
}
static esp_err_t read_form(httpd_req_t *req,char *body,size_t capacity) {
    if (atomic_load(&s_saved)) {
        httpd_resp_set_status(req,"409 Conflict");
        httpd_resp_sendstr(req,"Settings already saved. Reconnecting shortly.");
        return ESP_FAIL;
    }
    if (!req->content_len || req->content_len>=capacity) {
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Form too large. Go back and shorten the fields.");
        return ESP_FAIL; // Close the connection without parsing unread bytes.
    }
    size_t used=0;
    while (used<req->content_len) {
        int n=httpd_req_recv(req,body+used,req->content_len-used);
        if (n<=0) { memset(body,0,capacity); return ESP_FAIL; }
        used+=(size_t)n;
    }
    body[used]=0;
    return ESP_OK;
}
static esp_err_t save(httpd_req_t *req) {
    static char body[2048];
    if (read_form(req,body,sizeof body)!=ESP_OK) return ESP_FAIL;
    char old_ssid[33],old_pass[65],old_id[33];
    config_get_wifi(old_ssid,sizeof old_ssid,old_pass,sizeof old_pass);
    config_get_device_id(old_id,sizeof old_id);
    setup_request_t fields;
    const char *error=setup_request_parse(body,s_nonce,old_ssid,old_pass,old_id,&fields);
    memset(old_pass,0,sizeof old_pass); memset(body,0,sizeof body);
    if (!error && fields.relay && !fields.relay_code[0]) {
        char current[160]; config_get_relay_url(current,sizeof current);
        if (!config_relay_configured() || config_relay_revoked() || strcmp(current,fields.relay_url))
            error="Enter a new cloud relay pairing code from your server.";
    }
    if (!error && config_save_setup_transport(fields.ssid,fields.pass,fields.server,fields.id,fields.pair,
            fields.has_display_mode ? fields.display_mode : config_get_display_mode(),
            fields.relay, fields.relay_url, fields.relay_code)!=ESP_OK)
        error="Settings could not be fully saved. Please try again.";
    if (error) {
        httpd_resp_set_status(req,"400 Bad Request");
        esp_err_t result=form(req,error,&fields);
        memset(&fields,0,sizeof fields);
        return result;
    }
    memset(&fields,0,sizeof fields);
    return saved(req);
}
static void dns(void *unused) {
    (void)unused;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = INADDR_ANY};
    if (fd < 0) { atomic_store(&s_dns_running,false); vTaskDelete(NULL); return; }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) { close(fd); atomic_store(&s_dns_running,false); vTaskDelete(NULL); return; }
    struct timeval timeout={.tv_usec=250000};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
    uint8_t packet[512];
    while (!atomic_load(&s_dns_stop)) {
        struct sockaddr_in peer; socklen_t plen = sizeof(peer);
        int n = recvfrom(fd, packet, sizeof(packet) - 16, 0, (struct sockaddr *)&peer, &plen);
        if (n < 17 || packet[2] & 0x80 || packet[4] || packet[5] != 1) continue;
        int q = 12;
        while (q < n && packet[q] && packet[q] <= 63) q += packet[q] + 1;
        if (q + 5 > n || packet[q] != 0) continue;
        q++;
        bool ipv4 = packet[q] == 0 && packet[q+1] == 1 && packet[q+2] == 0 && packet[q+3] == 1;
        int end = q + 4;
        packet[2] = 0x81; packet[3] = 0x80;
        packet[6] = 0; packet[7] = ipv4 ? 1 : 0;
        memset(packet + 8, 0, 4);
        if (ipv4) {
            const uint8_t answer[] = {0xc0,12,0,1,0,1,0,0,0,30,0,4,192,168,4,1};
            memcpy(packet + end, answer, sizeof(answer)); end += sizeof(answer);
        }
        sendto(fd, packet, end, 0, (struct sockaddr *)&peer, plen);
    }
    close(fd);
    atomic_store(&s_dns_running,false);
    vTaskDelete(NULL);
}
void provisioning_run(const char *note) {
    s_note=note;
    wifi_release();
    char ssid[SETUP_SSID_CAPACITY];
    ESP_ERROR_CHECK(setup_ap_ssid(ssid));
    splash_setup(ssid);
    esp_err_t err=esp_netif_init();
    if (err!=ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    err=esp_event_loop_create_default();
    if (err!=ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) esp_netif_create_default_wifi_sta();
    if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    scan_networks();
    wifi_config_t config={.ap={.channel=1,.authmode=WIFI_AUTH_WPA2_PSK,.max_connection=2}};
    strlcpy((char *)config.ap.ssid,ssid,sizeof config.ap.ssid);
    config.ap.ssid_len=strlen(ssid);
    strlcpy((char *)config.ap.password,PROVISION_AP_PASS,sizeof config.ap.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&config));
    ESP_ERROR_CHECK(esp_wifi_start());
    snprintf(s_nonce,sizeof s_nonce,"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),
             (unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();
    hc.stack_size=8192; hc.uri_match_fn=httpd_uri_match_wildcard;
    hc.recv_wait_timeout=5; hc.max_open_sockets=4; hc.lru_purge_enable=true;
    httpd_handle_t http;
    ESP_ERROR_CHECK(httpd_start(&http,&hc));
    const httpd_uri_t handlers[]={
        {.uri="/",.method=HTTP_GET,.handler=page},
        {.uri="/save",.method=HTTP_POST,.handler=save},
        {.uri="/*",.method=HTTP_GET,.handler=redirect},
    };
    for (unsigned i=0;i<sizeof handlers/sizeof handlers[0];i++)
        ESP_ERROR_CHECK(httpd_register_uri_handler(http,&handlers[i]));
    atomic_store(&s_dns_running,true);
    if (xTaskCreate(dns,"setup_dns",3072,NULL,3,NULL)!=pdPASS) {
        atomic_store(&s_dns_running,false);
        ESP_LOGW(TAG,"captive DNS unavailable; open http://192.168.4.1");
    }
    ESP_LOGI(TAG,"setup network %s; open http://192.168.4.1",ssid);
    int64_t deadline=esp_timer_get_time()+(int64_t)PROVISION_PORTAL_TIMEOUT_S*1000000;
    while (!atomic_load(&s_saved)) {
        wifi_sta_list_t clients;
        if (esp_wifi_ap_get_sta_list(&clients)==ESP_OK && clients.num)
            deadline=esp_timer_get_time()+(int64_t)PROVISION_PORTAL_TIMEOUT_S*1000000;
        if (esp_timer_get_time()>=deadline) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (atomic_load(&s_saved)) { vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart(); }
    atomic_store(&s_dns_stop,true);
    for (unsigned i=0;i<50 && atomic_load(&s_dns_running);i++) vTaskDelay(pdMS_TO_TICKS(20));
    httpd_stop(http);
    esp_wifi_stop();
    power_sleep(SLEEP_INTERVAL_DEFAULT_S,false);
}
