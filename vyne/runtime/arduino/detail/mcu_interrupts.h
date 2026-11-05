// runtime/arduino/detail/mcu_interrupts.h
#pragma once
#include "mcu_types.h"
#include <avr/wdt.h>

// ---------------------------------------------------------------------
// Interrupts — counter-based, not callback-based.
//
// A Vyne fn compiles to `VyneValue fn_X(int argc, VyneValue* args)`.
// AVR ISRs are `void(void)`. Installing a Vyne fn directly would require
// the compiler to emit a per-function ISR trampoline, which is a
// separate feature. Until then, attach_interrupt installs a built-in
// trampoline that counts edges; sketches poll interrupt_count /
// interrupt_fired / interrupt_reset from the loop body.
//
// The Uno R3 has two external-interrupt pins: D2 (IRQ 0) and D3 (IRQ 1).
// digitalPinToInterrupt() returns -1 for every other pin.
// ---------------------------------------------------------------------

static volatile int64_t g_vyne_isr_count[2] = {0, 0};

static void _vyne_isr_0(void) {
    g_vyne_isr_count[0]++;
}
static void _vyne_isr_1(void) {
    g_vyne_isr_count[1]++;
}

static inline VyneValue
arduino_attach_interrupt(VyneValue pin, VyneValue mode) {
    // mode: 0=LOW, 1=CHANGE, 2=RISING, 3=FALLING.
    int p = (int)pin.as.i64;
    int irq = digitalPinToInterrupt(p);
    if (irq < 0 || irq > 1) {
        ::Serial.println(F("VNE-091: pin has no external interrupt"));
        return vyne_int(-1);
    }

    int avr_mode;
    switch ((int)mode.as.i64) {
    case 0:
        avr_mode = LOW;
        break;
    case 1:
        avr_mode = CHANGE;
        break;
    case 2:
        avr_mode = RISING;
        break;
    case 3:
        avr_mode = FALLING;
        break;
    default:
        avr_mode = CHANGE;
    }

    noInterrupts();
    g_vyne_isr_count[irq] = 0;
    interrupts();

    if (irq == 0)
        attachInterrupt(0, _vyne_isr_0, avr_mode);
    else
        attachInterrupt(1, _vyne_isr_1, avr_mode);

    return vyne_int((int64_t)irq);
}

static inline VyneValue arduino_detach_interrupt(VyneValue pin) {
    int irq = digitalPinToInterrupt((int)pin.as.i64);
    if (irq >= 0 && irq <= 1)
        detachInterrupt(irq);
    return vyne_null();
}

static inline VyneValue arduino_interrupt_count(VyneValue pin) {
    int irq = digitalPinToInterrupt((int)pin.as.i64);
    if (irq < 0 || irq > 1)
        return vyne_int(0);
    // 64-bit read is not atomic on 8-bit AVR; mask interrupts.
    noInterrupts();
    int64_t c = g_vyne_isr_count[irq];
    interrupts();
    return vyne_int(c);
}

static inline VyneValue arduino_interrupt_reset(VyneValue pin) {
    int irq = digitalPinToInterrupt((int)pin.as.i64);
    if (irq < 0 || irq > 1)
        return vyne_null();
    noInterrupts();
    g_vyne_isr_count[irq] = 0;
    interrupts();
    return vyne_null();
}

static inline VyneValue arduino_interrupt_fired(VyneValue pin) {
    int irq = digitalPinToInterrupt((int)pin.as.i64);
    if (irq < 0 || irq > 1)
        return vyne_bool(false);
    noInterrupts();
    bool fired = (g_vyne_isr_count[irq] > 0);
    interrupts();
    return vyne_bool(fired);
}

// ---------------------------------------------------------------------
// Watchdog. Enabled timeouts snap up to the nearest supported AVR
// interval. If a reset happens while WDTO is in effect, verify the
// cause via MCUSR on the next boot (not exposed here — sketch-level
// concern).
// ---------------------------------------------------------------------

static inline VyneValue arduino_watchdog_enable(VyneValue timeout_ms) {
    int64_t ms = timeout_ms.as.i64;
    if (ms <= 15)
        wdt_enable(WDTO_15MS);
    else if (ms <= 30)
        wdt_enable(WDTO_30MS);
    else if (ms <= 60)
        wdt_enable(WDTO_60MS);
    else if (ms <= 120)
        wdt_enable(WDTO_120MS);
    else if (ms <= 250)
        wdt_enable(WDTO_250MS);
    else if (ms <= 500)
        wdt_enable(WDTO_500MS);
    else if (ms <= 1000)
        wdt_enable(WDTO_1S);
    else if (ms <= 2000)
        wdt_enable(WDTO_2S);
    else if (ms <= 4000)
        wdt_enable(WDTO_4S);
    else
        wdt_enable(WDTO_8S);
    return vyne_null();
}

static inline VyneValue arduino_watchdog_reset(void) {
    wdt_reset();
    return vyne_null();
}

static inline VyneValue arduino_watchdog_disable(void) {
    wdt_disable();
    return vyne_null();
}
