// Optional ESP32 sniffer. Only the application task calls start/take/stop/getters.
// The Wi-Fi task touches pending counts/active only under the same short lock.
#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "mcu_wifi_sniff_counts.h"

namespace vyne_wifi_sniff {
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static VyneWiFiSniffCounts pending;
static VyneWiFiSniffSnapshot snapshot;
static uint64_t previous_us = 0;
static bool active = false;
static bool owns_wifi = false;
static int channel = 0;
static esp_err_t last_error = ESP_OK;

static void receive(void* buffer, wifi_promiscuous_pkt_type_t type) {
    unsigned category;
    switch (type) {
    case WIFI_PKT_MGMT: category = 0; break;
    case WIFI_PKT_DATA: category = 1; break;
    case WIFI_PKT_CTRL: category = 2; break;
    default: return; // MISC can have a different buffer shape; do not cast it.
    }
    if (!buffer) return;
    const auto* packet = static_cast<const wifi_promiscuous_pkt_t*>(buffer);
    if (packet->rx_ctrl.rx_state != 0 || packet->rx_ctrl.sig_len < 2) return;
    // No payload parsing, saved driver pointers, allocation, or display calls.
    portENTER_CRITICAL(&mux);
    if (active) pending.record(category);
    portEXIT_CRITICAL(&mux);
}

static inline bool stop() {
    if (!owns_wifi) { last_error = ESP_OK; return true; }
    portENTER_CRITICAL(&mux);
    active = false;
    portEXIT_CRITICAL(&mux);
    esp_err_t error = esp_wifi_set_promiscuous(false);
    const esp_err_t detach = esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (error == ESP_OK) error = detach;
    // Wi-Fi was OFF before start. Never erase credentials or touch display/NVS.
    if (!WiFi.mode(WIFI_OFF)) {
        last_error = error == ESP_OK ? ESP_FAIL : error;
        return false; // Retain ownership so explicit stop can retry cleanup.
    }
    owns_wifi = false;
    channel = 0;
    last_error = error;
    return error == ESP_OK;
}

static inline bool failed_start(esp_err_t error) {
    stop();
    last_error = error; // Keep the operation's original diagnostic.
    return false;
}

static inline bool start(int64_t requested_channel) {
    if (requested_channel < 1 || requested_channel > 14) {
        last_error = ESP_ERR_INVALID_ARG; return false;
    }
    if (owns_wifi || WiFi.getMode() != WIFI_OFF) {
        last_error = ESP_ERR_INVALID_STATE; return false;
    }
    owns_wifi = true;
    if (!WiFi.mode(WIFI_STA)) return failed_start(ESP_FAIL);
    wifi_country_t country = {};
    esp_err_t error = esp_wifi_get_country(&country);
    if (error != ESP_OK) return failed_start(error);
    // Respect the driver's configured country; never widen its channel range.
    if (!country.nchan || requested_channel < country.schan ||
        requested_channel >= (int64_t)country.schan + country.nchan)
        return failed_start(ESP_ERR_INVALID_ARG);
    error = esp_wifi_set_ps(WIFI_PS_NONE);
    if (error != ESP_OK) return failed_start(error);
    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
    error = esp_wifi_set_promiscuous_filter(&filter);
    if (error != ESP_OK) return failed_start(error);
    filter.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
    error = esp_wifi_set_promiscuous_ctrl_filter(&filter);
    if (error != ESP_OK) return failed_start(error);
    error = esp_wifi_set_channel((uint8_t)requested_channel, WIFI_SECOND_CHAN_NONE);
    if (error != ESP_OK) return failed_start(error);
    error = esp_wifi_set_promiscuous_rx_cb(receive);
    if (error != ESP_OK) return failed_start(error);
    portENTER_CRITICAL(&mux);
    pending = VyneWiFiSniffCounts{};
    snapshot = VyneWiFiSniffSnapshot{};
    previous_us = (uint64_t)esp_timer_get_time();
    active = true;
    portEXIT_CRITICAL(&mux);
    error = esp_wifi_set_promiscuous(true);
    if (error != ESP_OK) return failed_start(error);
    channel = (int)requested_channel;
    last_error = ESP_OK;
    return true;
}

static inline bool take() {
    portENTER_CRITICAL(&mux);
    const bool running = active;
    const bool sampled = running && vyne_wifi_sniff_take(pending, previous_us,
        (uint64_t)esp_timer_get_time(), snapshot);
    portEXIT_CRITICAL(&mux);
    last_error = running ? ESP_OK : ESP_ERR_INVALID_STATE;
    return sampled;
}
} // namespace vyne_wifi_sniff
