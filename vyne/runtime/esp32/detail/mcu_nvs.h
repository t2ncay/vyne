// runtime/esp32/detail/mcu_nvs.h
//
// Non-volatile storage, backed by the ESP32 Preferences library
// (`<Preferences.h>`, a thin wrapper over NVS). This replaces the AVR
// `eeprom_*` surface. The names are deliberately different: EEPROM slot
// addresses and NVS key/value pairs are not interchangeable, so
// `arduino.eeprom_read_int` is absent from ESP32_ARDUINO_MAP and fails
// at compile time rather than silently reading the wrong storage.
//
// Only one namespace is open at a time. `nvs_open()` closes the previous
// namespace first. Writes are committed by `nvs_commit()` (end + reopen)
// and on `nvs_close()`.
#pragma once
#include "mcu_types.h"
#include <Preferences.h>

static Preferences g_vyne_nvs;
static String g_vyne_nvs_ns;

static inline const char* _vyne_nvs_key(VyneValue v) {
    return (v.type == V_STRING && v.as.str) ? v.as.str : "";
}

static inline VyneValue esp32_nvs_open(VyneValue namespace_str) {
    const char* ns = _vyne_nvs_key(namespace_str);
    if (g_vyne_nvs_ns.length() > 0)
        g_vyne_nvs.end();
    g_vyne_nvs_ns = ns;
    bool ok = g_vyne_nvs.begin(ns, false);
    return vyne_bool(ok);
}

static inline VyneValue esp32_nvs_close(void) {
    g_vyne_nvs.end();
    g_vyne_nvs_ns = "";
    return vyne_null();
}

static inline VyneValue esp32_nvs_set_int(VyneValue key, VyneValue value) {
    g_vyne_nvs.putInt(_vyne_nvs_key(key), (int32_t)value.as.i64);
    return vyne_null();
}

static inline VyneValue esp32_nvs_get_int(VyneValue key, VyneValue fallback) {
    return vyne_int((int64_t)g_vyne_nvs.getInt(
        _vyne_nvs_key(key),
        (int32_t)fallback.as.i64
    ));
}

static inline VyneValue esp32_nvs_set_float(VyneValue key, VyneValue value) {
    g_vyne_nvs.putFloat(_vyne_nvs_key(key), (float)value.as.f64);
    return vyne_null();
}

static inline VyneValue esp32_nvs_get_float(VyneValue key, VyneValue fallback) {
    return vyne_float((double)g_vyne_nvs.getFloat(
        _vyne_nvs_key(key),
        (float)fallback.as.f64
    ));
}

static inline VyneValue esp32_nvs_set_string(VyneValue key, VyneValue value) {
    const char* s = (value.type == V_STRING && value.as.str) ? value.as.str : "";
    g_vyne_nvs.putString(_vyne_nvs_key(key), s);
    return vyne_null();
}

static inline VyneValue esp32_nvs_get_string(VyneValue key, VyneValue fallback) {
    const char* k = _vyne_nvs_key(key);
    const char* d =
        (fallback.type == V_STRING && fallback.as.str) ? fallback.as.str : "";
    if (!g_vyne_nvs.isKey(k))
        return vyne_string(d);
    String s = g_vyne_nvs.getString(k, d);
    return vyne_string(s.c_str());
}

static inline VyneValue esp32_nvs_commit(void) {
    // Preferences buffers writes until end(). Re-open immediately so the
    // caller can keep using the same namespace.
    String ns = g_vyne_nvs_ns;
    g_vyne_nvs.end();
    if (ns.length() > 0)
        g_vyne_nvs.begin(ns.c_str(), false);
    return vyne_null();
}

static inline VyneValue esp32_nvs_erase_all(void) {
    g_vyne_nvs.clear();
    return vyne_null();
}

static inline VyneValue esp32_nvs_erase_key(VyneValue key) {
    g_vyne_nvs.remove(_vyne_nvs_key(key));
    return vyne_null();
}

static inline VyneValue esp32_nvs_exists(VyneValue key) {
    return vyne_bool(g_vyne_nvs.isKey(_vyne_nvs_key(key)));
}
