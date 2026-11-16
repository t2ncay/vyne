#pragma once
#include <stdint.h>
enum wifi_mode_t { WIFI_OFF, WIFI_STA };
namespace sniff_mock {
inline wifi_mode_t mode = WIFI_OFF;
inline bool fail_start = false, fail_stop = false;
}
struct MockWiFi {
    wifi_mode_t getMode() const { return sniff_mock::mode; }
    bool mode(wifi_mode_t requested) {
        if ((requested == WIFI_STA && sniff_mock::fail_start) ||
            (requested == WIFI_OFF && sniff_mock::fail_stop)) return false;
        sniff_mock::mode = requested;
        return true;
    }
};
inline MockWiFi WiFi;
