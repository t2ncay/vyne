#pragma once
#include <stdint.h>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 258, ESP_ERR_INVALID_STATE = 259;
enum wifi_promiscuous_pkt_type_t { WIFI_PKT_MGMT, WIFI_PKT_CTRL, WIFI_PKT_DATA, WIFI_PKT_MISC };
struct wifi_promiscuous_pkt_t { struct { unsigned rx_state, sig_len; } rx_ctrl; };
struct wifi_country_t { uint8_t schan, nchan; };
struct wifi_promiscuous_filter_t { uint32_t filter_mask; };
constexpr uint32_t WIFI_PROMIS_FILTER_MASK_MGMT = 1, WIFI_PROMIS_FILTER_MASK_CTRL = 2,
    WIFI_PROMIS_FILTER_MASK_DATA = 4, WIFI_PROMIS_CTRL_FILTER_MASK_ALL = 0xFF800000;
constexpr int WIFI_PS_NONE = 0, WIFI_SECOND_CHAN_NONE = 0;
using wifi_promiscuous_cb_t = void (*)(void*, wifi_promiscuous_pkt_type_t);
namespace sniff_mock {
inline int calls = 0, fail_call = 0, selected_channel = 0;
inline bool enabled = false;
inline wifi_promiscuous_cb_t callback = nullptr;
inline uint32_t filter = 0, control_filter = 0;
inline esp_err_t step() { return ++calls == fail_call ? 42 : ESP_OK; }
}
inline esp_err_t esp_wifi_get_country(wifi_country_t* country) {
    const int error = sniff_mock::step(); *country = {1, 11}; return error;
}
inline esp_err_t esp_wifi_set_ps(int) { return sniff_mock::step(); }
inline esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t* filter) {
    sniff_mock::filter = filter->filter_mask; return sniff_mock::step();
}
inline esp_err_t esp_wifi_set_promiscuous_ctrl_filter(const wifi_promiscuous_filter_t* filter) {
    sniff_mock::control_filter = filter->filter_mask; return sniff_mock::step();
}
inline esp_err_t esp_wifi_set_channel(uint8_t channel, int) {
    sniff_mock::selected_channel = channel; return sniff_mock::step();
}
inline esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t callback) {
    const int error = sniff_mock::step();
    if (!error) sniff_mock::callback = callback;
    return error;
}
inline esp_err_t esp_wifi_set_promiscuous(bool enabled) {
    const int error = sniff_mock::step();
    if (!error) sniff_mock::enabled = enabled;
    return error;
}
