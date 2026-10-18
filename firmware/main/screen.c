// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 varanu5 <https://github.com/varanu5>
#include "screen.h"
#include "board.h"
#include "defaults.h"
#include "setup_font.h"
#include "vendor/qrcodegen.h"
#include <stddef.h>
#include <string.h>

#define ASSET(name) extern const uint8_t _binary_splash_##name##_bin_start[], _binary_splash_##name##_bin_end[]
ASSET(setup); ASSET(lowbatt); ASSET(waiting); ASSET(register); ASSET(revoked);
static bool load(uint8_t *fb,const uint8_t *start,const uint8_t *end) {
    if (!fb || (size_t)(end-start)!=EPD_FB_BYTES) return false;
    memcpy(fb,start,EPD_FB_BYTES); return true;
}
static void pixel(uint8_t *fb,int x,int y,bool black) {
    if (x<0 || x>=EPD_W || y<0 || y>=EPD_H) return;
    size_t at=(size_t)y*(EPD_W/4)+x/4; unsigned shift=6-2*(x%4);
    fb[at]=(uint8_t)((fb[at]&~(3u<<shift)) | ((black?0u:1u)<<shift));
}
static void text(uint8_t *fb,int x,int y,const char *value) {
    for (;*value;value++) {
        unsigned c=(unsigned char)*value;
        if (c<32 || c>126) c='?';
        for (int row=0;row<24;row++) for (int col=0;col<24;col++)
            if (setup_font[c-32].rows[row]&(1u<<col)) pixel(fb,x+col,y+row,true);
        x+=setup_font[c-32].advance;
    }
}
static bool field(char *payload,size_t cap,size_t *used,const char *value) {
    for (;*value;value++) {
        if (strchr("\\;,:\"",*value)) {
            if (*used+1>=cap) return false;
            payload[(*used)++]='\\';
        }
        if (*used+1>=cap) return false;
        payload[(*used)++]=*value;
    }
    payload[*used]=0; return true;
}
bool screen_setup(uint8_t *fb,const char *ssid) {
    if (!ssid || !load(fb,_binary_splash_setup_bin_start,_binary_splash_setup_bin_end)) return false;
    text(fb,156,144,ssid); text(fb,156,191,PROVISION_AP_PASS);
    char payload[224]="WIFI:T:WPA;S:"; size_t used=strlen(payload);
    if (!field(payload,sizeof payload,&used,ssid) || used+3>=sizeof payload) return true;
    memcpy(payload+used,";P:",4); used+=3;
    if (!field(payload,sizeof payload,&used,PROVISION_AP_PASS) || used+2>=sizeof payload) return true;
    memcpy(payload+used,";;",3);
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(4)],tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(4)];
    if (!qrcodegen_encodeText(payload,tmp,qr,qrcodegen_Ecc_MEDIUM,1,4,qrcodegen_Mask_AUTO,true)) return true;
    int size=qrcodegen_getSize(qr), width=(size+8)*3;
    int left=12+(129-width)/2, top=150-width/2;
    for (int y=0;y<width;y++) for (int x=0;x<width;x++) {
        int mx=x/3-4, my=y/3-4;
        bool black=mx>=0 && my>=0 && mx<size && my<size && qrcodegen_getModule(qr,mx,my);
        pixel(fb,left+x,top+y,black);
    }
    return true;
}
bool screen_low_battery(uint8_t *fb) {
    return load(fb,_binary_splash_lowbatt_bin_start,_binary_splash_lowbatt_bin_end);
}
bool screen_waiting(uint8_t *fb,bool registered) {
    return registered ? load(fb,_binary_splash_waiting_bin_start,_binary_splash_waiting_bin_end)
                      : load(fb,_binary_splash_register_bin_start,_binary_splash_register_bin_end);
}

bool screen_relay_removed(uint8_t *fb) {
    return load(fb,_binary_splash_revoked_bin_start,_binary_splash_revoked_bin_end);
}
