// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Vyne is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the
// Free Software Foundation, version 3.
//
// Vyne is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Vyne. If not, see <https://www.gnu.org/licenses/>.

#pragma once
// native_maps.h — Vyne name → C runtime symbol tables.
//
// Shared by C_Emitter and any other consumer that resolves native module
// members (e.g. vcore.now, vmath.sqrt) to C symbols. Intentionally
// dependency-free so emitter.h can include it without a cycle.
//
// Fields:
//   vyneName   Vyne-side name ("sqrt").
//   cName      C symbol, or full expression for properties ("vmath_pi()").
//   isProperty True for zero-arg constants/getters.
//   usesArgv   True for variadic natives taking (int argc, VyneValue* argv).
//   nativeF64  Optional unboxed `double(double, ...)` variant. M5.

struct NativeMapEntry {
    const char* vyneName;
    const char* cName;
    bool isProperty;
    bool usesArgv = false;
    const char* nativeF64 = nullptr;
    const char* nativeZero = nullptr;
};

// --- vcore -----------------------------------------------------------------
static const NativeMapEntry VCORE_MAP[] = {
    {"now", "vcore_runtime_now", false},
    {"now_ns", "vcore_runtime_now_ns", false},
    {"sleep", "vcore_runtime_sleep", false},
    {"platform", "vcore_runtime_platform", false},
    {"input", "vcore_runtime_input", false, true},
    {"parse_array", "vcore_parse_array", false},
    {"hex_to_int64", "vcore_hex_to_int64", false},
    {"chr", "vcore_chr", false},

    // String builder — general-purpose O(1)-append string accumulation.
    {"sb_create", "vcore_sb_create", false},
    {"sb_append", "vcore_sb_append", false},
    {"sb_append_char", "vcore_sb_append_char", false},
    {"sb_build", "vcore_sb_build", false},
    {"sb_reset", "vcore_sb_reset", false},

    {"version", "vcore_get_version()", true},
    {"engine", "vcore_get_engine()", true},
    {"build", "vcore_get_build()", true},
    {"processor_count", "vcore_get_processor_count()", true},
    {"pid", "vcore_get_pid()", true},
    {"memory_usage", "vcore_get_mem()", true},
};

// --- vmath -----------------------------------------------------------------
// nativeF64 = unboxed double→double variant, used when every argument is a
// provable Float64. See MethodCallNode::getCExpr for the dispatcher.
static const NativeMapEntry VMATH_MAP[] = {
    // --- 1-arg functions ---
    {"sqrt", "vmath_sqrt", false, false, "vmath_sqrt_f64"},
    {"abs", "vmath_abs", false, false, "vmath_abs_f64"},
    {"sin", "vmath_sin", false, false, "vmath_sin_f64"},
    {"cos", "vmath_cos", false, false, "vmath_cos_f64"},
    {"tan", "vmath_tan", false, false, "vmath_tan_f64"},
    {"asin", "vmath_asin", false, false, "vmath_asin_f64"},
    {"acos", "vmath_acos", false, false, "vmath_acos_f64"},
    {"atan", "vmath_atan", false, false, "vmath_atan_f64"},
    {"sinh", "vmath_sinh", false, false, "vmath_sinh_f64"},
    {"cosh", "vmath_cosh", false, false, "vmath_cosh_f64"},
    {"tanh", "vmath_tanh", false, false, "vmath_tanh_f64"},
    {"log", "vmath_log", false, false, "vmath_log_f64"},
    {"log10", "vmath_log10", false, false, "vmath_log10_f64"},
    {"exp", "vmath_exp", false, false, "vmath_exp_f64"},
    {"floor", "vmath_floor", false, false, "vmath_floor_f64"},
    {"ceil", "vmath_ceil", false, false, "vmath_ceil_f64"},
    {"round", "vmath_round", false, false, "vmath_round_f64"},
    {"erf", "vmath_erf", false, false, "vmath_erf_f64"},
    {"erfc", "vmath_erfc", false, false, "vmath_erfc_f64"},
    {"tgamma", "vmath_tgamma", false},
    {"lgamma", "vmath_lgamma", false},
    {"sigmoid", "vmath_sigmoid", false, false, "vmath_sigmoid_f64"},
    {"relu", "vmath_relu", false, false, "vmath_relu_f64"},
    {"degrees", "vmath_degrees", false},
    {"radians", "vmath_radians", false},

    // --- 2-arg functions ---
    {"pow", "vmath_pow", false, false, "vmath_pow_f64"},
    {"hypot", "vmath_hypot", false, false, "vmath_hypot_f64"},
    {"atan2", "vmath_atan2", false, false, "vmath_atan2_f64"},
    {"fmod", "vmath_fmod", false, false, "vmath_fmod_f64"},
    {"min", "vmath_min", false, false, "vmath_min_f64"},
    {"max", "vmath_max", false, false, "vmath_max_f64"},

    // --- 3-arg functions ---
    {"clamp", "vmath_clamp", false, false, "vmath_clamp_f64"},

    // --- variadic / special (no unboxed variant) ---
    {"random", "vmath_random", false},
    {"random_float",
     "vmath_random_float",
     false,
     false,
     nullptr,
     "vmath_random_float_unit"},
    {"seed", "vmath_seed", false},

    // --- constants (zero-arg getters) ---
    {"pi", "vmath_pi()", true},
    {"e", "vmath_e()", true},
    {"tau", "vmath_tau()", true},
    {"phi", "vmath_phi()", true},
    {"sqrt2", "vmath_sqrt2()", true},
    {"inf", "vmath_inf()", true},
    {"nan", "vmath_nan()", true},
};

// --- vmem ------------------------------------------------------------------
static const NativeMapEntry VMEM_MAP[] = {
    // Core (unchanged)
    {"checkpoint", "vmem_runtime_checkpoint", false},
    {"rewind", "vmem_runtime_rewind", false},
    {"total_allocated", "vmem_runtime_total_allocated", false},
    {"reset", "vmem_runtime_reset", false},

    // Peak tracking
    {"peak_allocated", "vmem_runtime_peak_allocated", false},
    {"commit_peak_allocated", "vmem_runtime_commit_peak_allocated", false},
    {"peak_reset", "vmem_runtime_peak_reset", false},

    // Live state inspection
    {"checkpoint_depth", "vmem_runtime_checkpoint_depth", false},
    {"checkpoint_capacity", "vmem_runtime_checkpoint_capacity", false},
    {"checkpoint_remaining", "vmem_runtime_checkpoint_remaining", false},
    {"block_count", "vmem_runtime_block_count", false},
    {"current_block_used", "vmem_runtime_current_block_used", false},
    {"current_block_capacity", "vmem_runtime_current_block_capacity", false},
    {"commit_total_allocated", "vmem_runtime_commit_total_allocated", false},

    // @streamed pinned arena
    {"stream_total_allocated", "vmem_runtime_stream_total_allocated", false},
    {"stream_peak_allocated", "vmem_runtime_stream_peak_allocated", false},
    {"stream_is_pinned", "vmem_runtime_stream_is_pinned", false},
    {"stream_block_count", "vmem_runtime_stream_block_count", false},

    // Checkpoint management
    {"pop", "vmem_runtime_pop", false},
    {"pop_n", "vmem_runtime_pop_n", false},
    {"rewind_freed", "vmem_runtime_rewind_freed", false},

    // Diagnostics
    {"is_balanced", "vmem_runtime_is_balanced", false},
    {"assert_balanced", "vmem_runtime_assert_balanced", false},
    {"verify_checkpoint", "vmem_runtime_verify_checkpoint", false},
    {"dump", "vmem_runtime_dump", false},
    {"stats", "vmem_runtime_stats", false},
};

static const NativeMapEntry VFS_MAP[] = {
    // Read
    {"read", "vfs_read", false},
    {"read_lines", "vfs_read_lines", false},
    {"read_bytes", "vfs_read_bytes", false},
    // Write
    {"write", "vfs_write", false},
    {"append", "vfs_append", false},
    {"write_lines", "vfs_write_lines", false},
    {"write_bytes", "vfs_write_bytes", false},
    // Metadata
    {"exists", "vfs_exists", false},
    {"is_file", "vfs_is_file", false},
    {"is_dir", "vfs_is_dir", false},
    {"size", "vfs_size", false},
    {"mtime", "vfs_mtime", false},
    {"atime", "vfs_atime", false},
    // Directory
    {"list_dir", "vfs_list_dir", false},
    {"walk", "vfs_walk", false},
    {"glob", "vfs_glob", false},
    {"mkdir", "vfs_mkdir", false},
    {"mkdir_p", "vfs_mkdir_p", false},
    {"remove", "vfs_remove", false},
    {"remove_dir", "vfs_remove_dir", false},
    // Transfer
    {"copy", "vfs_copy", false},
    {"move", "vfs_move", false},
    {"rename", "vfs_move", false}, // alias
    // Path
    {"basename", "vfs_basename", false},
    {"dirname", "vfs_dirname", false},
    {"stem", "vfs_stem", false},
    {"extension", "vfs_extension", false},
    {"join", "vfs_join", false},
};

// --- arduino ---------------------------------------------------------------
// No nativeF64 or nativeZero — none of these has a scalar return.
// `millis` is a zero-arg getter, `delay` is a one-arg void.
static const NativeMapEntry ARDUINO_MAP[] = {
    // --- Base pin / digital / analog (mcu_hardware.h) ---
    {"pin_mode", "arduino_pin_mode", false},
    {"digital_write", "arduino_digital_write", false},
    {"digital_read", "arduino_digital_read", false},
    {"analog_read", "arduino_analog_read", false},
    {"analog_write", "arduino_analog_write", false},
    {"delay", "arduino_delay", false},
    {"delay_microseconds", "arduino_delay_microseconds", false},

    // --- Serial (mcu_serial.h) ---
    {"serial_begin", "arduino_serial_begin", false},
    {"serial_print", "arduino_serial_print", false},
    {"serial_available", "arduino_serial_available", false},
    {"serial_read", "arduino_serial_read", false},
    {"serial_read_line", "arduino_serial_read_line", false},
    {"serial_parse_int", "arduino_serial_parse_int", false},
    {"serial_parse_float", "arduino_serial_parse_float", false},
    {"serial_println", "arduino_serial_println", false},
    {"serial_flush", "arduino_serial_flush", false},

    // --- Timing (mcu_hardware.h + mcu_timing.h) ---
    {"millis", "arduino_millis()", true},
    {"micros", "arduino_micros()", true},
    {"pulse_width", "arduino_pulse_width", false},
    {"timer_read", "arduino_timer_read", false},

    // --- Math (mcu_math.h). The _f64 entries fire when every arg is
    //     statically Float64; the boxed C symbol is the fallback. ---
    {"sin", "arduino_sin", false, false, "arduino_sin_f64"},
    {"cos", "arduino_cos", false, false, "arduino_cos_f64"},
    {"sqrt", "arduino_sqrt", false, false, "arduino_sqrt_f64"},
    {"tan", "arduino_tan", false, false, "arduino_tan_f64"},
    {"asin", "arduino_asin", false, false, "arduino_asin_f64"},
    {"acos", "arduino_acos", false, false, "arduino_acos_f64"},
    {"atan", "arduino_atan", false, false, "arduino_atan_f64"},
    {"atan2", "arduino_atan2", false, false, "arduino_atan2_f64"},
    {"log", "arduino_log", false, false, "arduino_log_f64"},
    {"log10", "arduino_log10", false, false, "arduino_log10_f64"},
    {"exp", "arduino_exp", false, false, "arduino_exp_f64"},
    {"pow", "arduino_pow", false, false, "arduino_pow_f64"},
    {"fmod", "arduino_fmod", false, false, "arduino_fmod_f64"},
    {"constrain", "arduino_constrain", false, false, "arduino_constrain_f64"},
    {"fmap", "arduino_fmap", false, false, "arduino_fmap_f64"},
    {"lerp", "arduino_lerp", false, false, "arduino_lerp_f64"},
    {"smoothstep",
     "arduino_smoothstep",
     false,
     false,
     "arduino_smoothstep_f64"},
    {"radians", "arduino_radians", false, false, "arduino_radians_f64"},
    {"degrees", "arduino_degrees", false, false, "arduino_degrees_f64"},
    {"random", "arduino_random_variadic", false, true}, // (max) or (lo, hi)
    {"random_seed", "arduino_random_seed", false},

    // --- Digital / analog extensions (mcu_pins_ext.h) ---
    {"pin_toggle", "arduino_pin_toggle", false},
    {"pulse_in", "arduino_pulse_in", false},
    {"shift_out", "arduino_shift_out", false},
    {"pulse_count", "arduino_pulse_count", false},
    {"analog_read_voltage", "arduino_analog_read_voltage", false},
    {"analog_reference", "arduino_analog_reference", false},
    {"analog_read_resolution", "arduino_analog_read_resolution", false},
    {"analog_write_resolution", "arduino_analog_write_resolution", false},
    {"analog_read_oversampled", "arduino_analog_read_oversampled", false},

    // --- Memory introspection (mcu_memory.h) ---
    {"arena_used", "arduino_arena_used()", true},
    {"arena_peak", "arduino_arena_peak()", true},
    {"arena_free", "arduino_arena_free()", true},
    {"free_ram", "arduino_free_ram()", true},
    {"stack_free", "arduino_stack_free()", true},

    // --- Bit manipulation (mcu_bits.h) ---
    {"bit_set", "arduino_bit_set", false},
    {"bit_clear", "arduino_bit_clear", false},
    {"bit_toggle", "arduino_bit_toggle", false},
    {"bit_read", "arduino_bit_read", false},
    {"low_byte", "arduino_low_byte", false},
    {"high_byte", "arduino_high_byte", false},
    {"make_word", "arduino_make_word", false},
    {"popcount", "arduino_popcount", false},
    {"crc8", "arduino_crc8", false},

    // --- EEPROM (mcu_eeprom.h) ---
    {"eeprom_read_int", "arduino_eeprom_read_int", false},
    {"eeprom_write_int", "arduino_eeprom_write_int", false},
    {"eeprom_read_float", "arduino_eeprom_read_float", false},
    {"eeprom_write_float", "arduino_eeprom_write_float", false},

    // --- Interrupts + watchdog (mcu_interrupts.h) ---
    {"attach_interrupt", "arduino_attach_interrupt", false},
    {"detach_interrupt", "arduino_detach_interrupt", false},
    {"interrupt_count", "arduino_interrupt_count", false},
    {"interrupt_reset", "arduino_interrupt_reset", false},
    {"interrupt_fired", "arduino_interrupt_fired", false},
    {"watchdog_enable", "arduino_watchdog_enable", false},
    {"watchdog_reset", "arduino_watchdog_reset()", true},
    {"watchdog_disable", "arduino_watchdog_disable()", true},

    // --- NOT IMPLEMENTED (documented, intentionally absent from map) ---
    // dot, norm, argmax, argmin require a typed-array container
    // (VyneArray_f64 / VyneArray_i64) in mcu_types.h plus boxAny
    // lowering for MCU targets. Calling these names from a sketch will
    // fail at compile time with "not defined" — deliberately clearer
    // than a runtime halt.
};
