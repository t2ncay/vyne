// runtime/esp32/detail/mcu_wifi.h
//
// WiFi + HTTP, backed by the ESP32 core's `<WiFi.h>` and
// `<HTTPClient.h>`. This is the largest optional feature in the runtime:
// the first call that initializes the WiFi driver (connect, scan, mode
// STA) permanently reserves roughly 50 KB of static heap for the
// lwIP/WiFi stack. Budget for it explicitly; a sketch that never touches
// `esp32.wifi_*` never links this header (VYNE_USE_WIFI stays undefined).
//
// `wifi_scan()` note: the MCU type union in mcu_types.h has no `map`
// member — maps are a desktop-only container on this runtime — so the
// scan returns a V_ARRAY of V_STRING, one line per network:
//
//     "<ssid>\t<rssi>\t<channel>\t<secure>"
//
// Parse with string splitting in Vyne, or read the fields visually.
// This is the one deliberate surface deviation from the ESP32 spec; a
// real Map return needs the MCU map runtime first.
#pragma once
#include "mcu_types.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <stdio.h>

static int g_vyne_http_status = 0;

static inline VyneValue esp32_wifi_mode(VyneValue mode) {
    // 0=OFF, 1=STA, 2=AP, 3=AP_STA.
    switch ((int)mode.as.i64) {
    case 0:
        WiFi.mode(WIFI_OFF);
        break;
    case 1:
        WiFi.mode(WIFI_STA);
        break;
    case 2:
        WiFi.mode(WIFI_AP);
        break;
    case 3:
        WiFi.mode(WIFI_AP_STA);
        break;
    default:
        WiFi.mode(WIFI_STA);
        break;
    }
    return vyne_null();
}

static inline VyneValue
esp32_wifi_connect(VyneValue ssid, VyneValue password) {
    const char* s = (ssid.type == V_STRING && ssid.as.str) ? ssid.as.str : "";
    const char* p =
        (password.type == V_STRING && password.as.str) ? password.as.str : "";
    WiFi.begin(s, p);
    return vyne_null();
}

static inline VyneValue esp32_wifi_disconnect(void) {
    WiFi.disconnect();
    return vyne_null();
}

static inline VyneValue esp32_wifi_status(void) {
    // Same numeric values as wl_status_t: 0=IDLE, 1=NO_SSID_AVAILABLE,
    // 3=CONNECTED, 4=CONNECT_FAILED, 6=DISCONNECTED.
    return vyne_int((int64_t)WiFi.status());
}

static inline VyneValue esp32_wifi_local_ip(void) {
    if (WiFi.status() != WL_CONNECTED)
        return vyne_string("");
    String ip = WiFi.localIP().toString();
    return vyne_string(ip.c_str());
}

static inline VyneValue esp32_wifi_rssi(void) {
    if (WiFi.status() != WL_CONNECTED)
        return vyne_int(0);
    return vyne_int((int64_t)WiFi.RSSI());
}

static inline VyneValue esp32_wifi_ssid(void) {
    if (WiFi.status() != WL_CONNECTED)
        return vyne_string("");
    String s = WiFi.SSID();
    return vyne_string(s.c_str());
}

static inline VyneValue esp32_wifi_mac(void) {
    String mac = WiFi.macAddress();
    return vyne_string(mac.c_str());
}

static inline VyneValue esp32_wifi_hostname(VyneValue name) {
    const char* n = (name.type == V_STRING && name.as.str) ? name.as.str : "";
    WiFi.setHostname(n);
    return vyne_null();
}

static inline VyneValue esp32_wifi_scan(void) {
    int n = WiFi.scanNetworks();
    VyneValue arr = vyne_array_create(n > 0 ? n : 0);
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        int channel = WiFi.channel(i);
        bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        char line[192];
        snprintf(
            line,
            sizeof(line),
            "%s\t%d\t%d\t%s",
            ssid.c_str(),
            rssi,
            channel,
            secure ? "secure" : "open"
        );
        vyne_array_push(arr, vyne_string(line));
    }
    WiFi.scanDelete();
    return arr;
}

// ---------------------------------------------------------------------
// HTTP helpers. On failure `http_get`/`http_post` return "", and
// `http_status` holds the negative HTTPClient error code.
// ---------------------------------------------------------------------
static inline VyneValue esp32_http_get(VyneValue url) {
    const char* u = (url.type == V_STRING && url.as.str) ? url.as.str : "";
    HTTPClient http;
    String body = "";
    if (http.begin(u)) {
        g_vyne_http_status = http.GET();
        if (g_vyne_http_status > 0)
            body = http.getString();
        http.end();
    } else {
        g_vyne_http_status = -1;
    }
    return vyne_string(body.c_str());
}

static inline VyneValue esp32_http_post(VyneValue url, VyneValue payload) {
    const char* u = (url.type == V_STRING && url.as.str) ? url.as.str : "";
    const char* p =
        (payload.type == V_STRING && payload.as.str) ? payload.as.str : "";
    HTTPClient http;
    String body = "";
    if (http.begin(u)) {
        http.addHeader("Content-Type", "text/plain");
        g_vyne_http_status = http.POST((uint8_t*)p, strlen(p));
        if (g_vyne_http_status > 0)
            body = http.getString();
        http.end();
    } else {
        g_vyne_http_status = -1;
    }
    return vyne_string(body.c_str());
}

static inline VyneValue esp32_http_post_json(VyneValue url, VyneValue json) {
    const char* u = (url.type == V_STRING && url.as.str) ? url.as.str : "";
    const char* p = (json.type == V_STRING && json.as.str) ? json.as.str : "";
    HTTPClient http;
    String body = "";
    if (http.begin(u)) {
        http.addHeader("Content-Type", "application/json");
        g_vyne_http_status = http.POST((uint8_t*)p, strlen(p));
        if (g_vyne_http_status > 0)
            body = http.getString();
        http.end();
    } else {
        g_vyne_http_status = -1;
    }
    return vyne_string(body.c_str());
}

static inline VyneValue esp32_http_status(void) {
    return vyne_int((int64_t)g_vyne_http_status);
}
