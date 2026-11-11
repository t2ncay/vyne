// runtime/esp32/detail/mcu_interrupts.h
//
// ESP32 adaptation of the AVR interrupt surface. The Vyne-level API is
// identical, but the implementation differs in three ways:
//
//   1. Every GPIO can be an interrupt source, so there is no "IRQ 0/1"
//      restriction. Up to eight pins can be attached at once; each gets
//      a dedicated trampoline that increments its own counter.
//   2. The counter is 64-bit and atomic on Xtensa/RISC-V, so no
//      noInterrupts()/interrupts() bracket is required around the read.
//   3. The watchdog is the ESP-IDF Task Watchdog (TWDT), whose init
//      signature changed in IDF 5.x. Both signatures are handled.
//
// As on AVR, the surface is counter-based rather than callback-based:
// Vyne functions compile to the boxed signature and cannot be installed
// directly as `void(void)` ISRs. Poll interrupt_count / interrupt_fired.
#pragma once
#include "mcu_types.h"
#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#define VYNE_ISR_MAX_PINS 8

static volatile int64_t g_vyne_isr_count[VYNE_ISR_MAX_PINS];
static int g_vyne_isr_pin[VYNE_ISR_MAX_PINS];
static int g_vyne_isr_used = 0;

static void _vyne_isr_0(void) {
    g_vyne_isr_count[0]++;
}
static void _vyne_isr_1(void) {
    g_vyne_isr_count[1]++;
}
static void _vyne_isr_2(void) {
    g_vyne_isr_count[2]++;
}
static void _vyne_isr_3(void) {
    g_vyne_isr_count[3]++;
}
static void _vyne_isr_4(void) {
    g_vyne_isr_count[4]++;
}
static void _vyne_isr_5(void) {
    g_vyne_isr_count[5]++;
}
static void _vyne_isr_6(void) {
    g_vyne_isr_count[6]++;
}
static void _vyne_isr_7(void) {
    g_vyne_isr_count[7]++;
}

typedef void (*VyneIsr)(void);
static const VyneIsr g_vyne_isr_table[VYNE_ISR_MAX_PINS] = {
    _vyne_isr_0,
    _vyne_isr_1,
    _vyne_isr_2,
    _vyne_isr_3,
    _vyne_isr_4,
    _vyne_isr_5,
    _vyne_isr_6,
    _vyne_isr_7,
};

static inline int _vyne_isr_slot(int pin, bool create) {
    for (int i = 0; i < g_vyne_isr_used; ++i)
        if (g_vyne_isr_pin[i] == pin)
            return i;
    if (!create || g_vyne_isr_used >= VYNE_ISR_MAX_PINS)
        return -1;
    int slot = g_vyne_isr_used++;
    g_vyne_isr_pin[slot] = pin;
    g_vyne_isr_count[slot] = 0;
    return slot;
}

static inline VyneValue
arduino_attach_interrupt(VyneValue pin, VyneValue mode) {
    // mode: 0=LOW, 1=CHANGE, 2=RISING, 3=FALLING.
    int p = (int)pin.as.i64;
    int slot = _vyne_isr_slot(p, true);
    if (slot < 0) {
        ::Serial.println(F("VNE-091: too many interrupt pins attached"));
        return vyne_int(-1);
    }

    int esp_mode;
    switch ((int)mode.as.i64) {
    case 0:
        esp_mode = ONLOW;
        break;
    case 1:
        esp_mode = CHANGE;
        break;
    case 2:
        esp_mode = RISING;
        break;
    case 3:
        esp_mode = FALLING;
        break;
    default:
        esp_mode = CHANGE;
    }

    g_vyne_isr_count[slot] = 0;
    attachInterrupt(digitalPinToInterrupt(p), g_vyne_isr_table[slot], esp_mode);
    return vyne_int((int64_t)slot);
}

static inline VyneValue arduino_detach_interrupt(VyneValue pin) {
    int slot = _vyne_isr_slot((int)pin.as.i64, false);
    if (slot >= 0)
        detachInterrupt(digitalPinToInterrupt(g_vyne_isr_pin[slot]));
    return vyne_null();
}

static inline VyneValue arduino_interrupt_count(VyneValue pin) {
    int slot = _vyne_isr_slot((int)pin.as.i64, false);
    if (slot < 0)
        return vyne_int(0);
    // A naturally aligned 64-bit load is atomic on both Xtensa and
    // RISC-V, so no critical section is needed here.
    return vyne_int(g_vyne_isr_count[slot]);
}

static inline VyneValue arduino_interrupt_reset(VyneValue pin) {
    int slot = _vyne_isr_slot((int)pin.as.i64, false);
    if (slot >= 0)
        g_vyne_isr_count[slot] = 0;
    return vyne_null();
}

static inline VyneValue arduino_interrupt_fired(VyneValue pin) {
    int slot = _vyne_isr_slot((int)pin.as.i64, false);
    if (slot < 0)
        return vyne_bool(false);
    return vyne_bool(g_vyne_isr_count[slot] > 0);
}

// ---------------------------------------------------------------------
// Task watchdog. The timeout is passed straight to the TWDT, whose
// resolution is milliseconds.
// ---------------------------------------------------------------------
static inline VyneValue arduino_watchdog_enable(VyneValue timeout_ms) {
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t cfg = {};
    cfg.timeout_ms = (uint32_t)timeout_ms.as.i64;
    cfg.idle_core_mask = 0;
    cfg.trigger_panic = true;
    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        // Arduino may have initialized the TWDT already; reconfigure it
        // instead of failing.
        esp_task_wdt_reconfigure(&cfg);
    }
    esp_task_wdt_add(NULL);
#else
    esp_task_wdt_init((uint32_t)timeout_ms.as.i64, true);
    esp_task_wdt_add(NULL);
#endif
    return vyne_null();
}

static inline VyneValue arduino_watchdog_reset(void) {
    esp_task_wdt_reset();
    return vyne_null();
}

static inline VyneValue arduino_watchdog_disable(void) {
    esp_task_wdt_delete(NULL);
    return vyne_null();
}
