#pragma once
#include "mcu_types.h"
#include "mcu_wifi_sniff_driver.h"

static inline VyneValue esp32_wifi_sniff_start(VyneValue channel) {
    if (channel.type != V_INT64) {
        vyne_wifi_sniff::last_error = ESP_ERR_INVALID_ARG;
        return vyne_bool(false);
    }
    return vyne_bool(vyne_wifi_sniff::start(channel.as.i64));
}
static inline VyneValue esp32_wifi_sniff_stop(void) { return vyne_bool(vyne_wifi_sniff::stop()); }
static inline VyneValue esp32_wifi_sniff_sample(void) { return vyne_bool(vyne_wifi_sniff::take()); }
static inline VyneValue esp32_wifi_sniff_management(void) { return vyne_int(vyne_wifi_sniff::snapshot.counts.frames[0]); }
static inline VyneValue esp32_wifi_sniff_data(void) { return vyne_int(vyne_wifi_sniff::snapshot.counts.frames[1]); }
static inline VyneValue esp32_wifi_sniff_control(void) { return vyne_int(vyne_wifi_sniff::snapshot.counts.frames[2]); }
static inline VyneValue esp32_wifi_sniff_elapsed_us(void) { return vyne_int((int64_t)vyne_wifi_sniff::snapshot.elapsed_us); }
static inline VyneValue esp32_wifi_sniff_saturated(void) { return vyne_bool(vyne_wifi_sniff::snapshot.counts.saturated); }
static inline VyneValue esp32_wifi_sniff_error(void) { return vyne_int(vyne_wifi_sniff::last_error); }
