// runtime/esp32/detail/mcu_power.h
//
// Sleep and reset control. Timers are in microseconds to match the rest
// of the Vyne timing surface. Wake-cause values match
// esp_sleep_wakeup_cause_t:
//
//   0 UNDEFINED, 2 EXT0, 3 EXT1, 4 TIMER, 5 TOUCHPAD, 6 ULP, 7 GPIO
//
// For a sleep to be useful the sketch must arm a wake source first:
// power_wake_timer() or power_wake_pin(), then deep/light sleep.
#pragma once
#include "mcu_types.h"
#include <esp_sleep.h>

static inline VyneValue esp32_power_deep_sleep_us(VyneValue us) {
    esp_deep_sleep((uint64_t)us.as.i64);
    return vyne_null();
}

static inline VyneValue esp32_power_light_sleep_us(VyneValue us) {
    esp_sleep_enable_timer_wakeup((uint64_t)us.as.i64);
    esp_light_sleep_start();
    return vyne_null();
}

static inline VyneValue esp32_power_wake_timer(VyneValue us) {
    esp_sleep_enable_timer_wakeup((uint64_t)us.as.i64);
    return vyne_null();
}

static inline VyneValue
esp32_power_wake_pin(VyneValue pin, VyneValue level) {
    // level: 0 = wake on LOW, 1 = wake on HIGH. EXT0 wakes from one pin.
    esp_sleep_enable_ext0_wakeup(
        (gpio_num_t)pin.as.i64,
        (int)(level.as.i64 != 0)
    );
    return vyne_null();
}

static inline VyneValue esp32_power_wake_cause(void) {
    return vyne_int((int64_t)esp_sleep_get_wakeup_cause());
}

static inline VyneValue esp32_power_restart(void) {
    ESP.restart();
    return vyne_null();
}
