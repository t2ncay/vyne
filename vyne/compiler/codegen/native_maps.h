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
    int arity = -1; // optional fixed native-call argument count
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

    // @realtime telemetry — see runtime/detail/realtime.h.
    {"realtime_worst_ms", "vcore_realtime_worst_ms", false},
    {"realtime_overruns", "vcore_realtime_overruns", false},
    {"realtime_reset", "vcore_realtime_reset", false},
    {"realtime_worst_ms_named", "vcore_realtime_worst_ms_named", false},
    {"realtime_overruns_named", "vcore_realtime_overruns_named", false},

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

// --- vimage ----------------------------------------------------------------
// Image decode/encode on top of vendored stb_image / stb_image_write.
// All functions take boxed VyneValue arguments; typed arrays cross the
// boundary O(1) through vyne_array_f64_to_value.
static const NativeMapEntry VIMAGE_MAP[] = {
    {"load_raw", "vimage_load_raw", false},
    {"pixels", "vimage_pixels", false},
    {"save_raw", "vimage_save_raw", false},
    {"resize_bilinear", "vimage_resize_bilinear", false},
    {"crop_raw", "vimage_crop", false},
    {"grayscale_raw", "vimage_grayscale", false},
};

// --- vplot -----------------------------------------------------------------
// vplot owns native handles; append/render use fixed-arity stack arguments.
#define VPLOT_CALL(name, count) {#name,"vplot_" #name,false,false,nullptr,nullptr,count}
static const NativeMapEntry VPLOT_MAP[] = {
    VPLOT_CALL(chart,1), VPLOT_CALL(labels,3), VPLOT_CALL(add_line,4),
    VPLOT_CALL(show,1), VPLOT_CALL(output_surface,1), VPLOT_CALL(live_chart,1), VPLOT_CALL(add_live_series,2),
    VPLOT_CALL(push,2), VPLOT_CALL(push_xy,3), VPLOT_CALL(live_count,1),
    {"close","vplot_destroy",false,false,nullptr,nullptr,1},
    VPLOT_CALL(show_line,3), VPLOT_CALL(show_scatter,3), VPLOT_CALL(show_bars,3),
    VPLOT_CALL(show_histogram,2), VPLOT_CALL(show_heatmap,4),
    VPLOT_CALL(save,2), VPLOT_CALL(save_line,4), VPLOT_CALL(save_scatter,4),
    VPLOT_CALL(save_bars,4), VPLOT_CALL(save_histogram,3), VPLOT_CALL(save_heatmap,5),
    VPLOT_CALL(new_plot,3), VPLOT_CALL(destroy,1), VPLOT_CALL(live_series,4),
    VPLOT_CALL(append,3), VPLOT_CALL(clear,1), VPLOT_CALL(set_data,3),
    VPLOT_CALL(line,5), VPLOT_CALL(line_view,5), VPLOT_CALL(scatter,5), VPLOT_CALL(bars,5),
    VPLOT_CALL(bar_width,2), VPLOT_CALL(histogram,5), VPLOT_CALL(histogram_update,2),
    VPLOT_CALL(heatmap,5), VPLOT_CALL(heatmap_update,2), VPLOT_CALL(limits,5),
    VPLOT_CALL(autoscale,4), VPLOT_CALL(theme,2), VPLOT_CALL(grid,2), VPLOT_CALL(minor_grid,2), VPLOT_CALL(legend,2),
    VPLOT_CALL(transparent,2), VPLOT_CALL(decimation,2), VPLOT_CALL(thickness,2),
    VPLOT_CALL(marker_size,2), VPLOT_CALL(invalidate,1), VPLOT_CALL(rgb,3),
    VPLOT_CALL(render,2), VPLOT_CALL(render_at,6), VPLOT_CALL(surface_width,1),
    VPLOT_CALL(surface_height,1), VPLOT_CALL(frame_count,1), VPLOT_CALL(series_count,1),
    VPLOT_CALL(sample_x,2), VPLOT_CALL(sample_y,2), VPLOT_CALL(memory_bytes,0),
    VPLOT_CALL(peak_bytes,0), VPLOT_CALL(allocation_count,0), VPLOT_CALL(last_error,0),
    VPLOT_CALL(image_surface,2), VPLOT_CALL(image_bind,4), VPLOT_CALL(image_pixels,1),
    VPLOT_CALL(pixel_rgb,3), VPLOT_CALL(display_surface,1),
    {"DARK","vyne_int(0)",true,false,nullptr,nullptr,0}, {"LIGHT","vyne_int(1)",true,false,nullptr,nullptr,0}
};
#undef VPLOT_CALL

// --- vconv -----------------------------------------------------------------
// CPU forward convolution primitives for the SR inference path.
static const NativeMapEntry VCONV_MAP[] = {
    {"conv2d_fwd", "vconv_conv2d_fwd", false},
    {"conv2d_backward_input", "vconv_conv2d_backward_input", false},
    {"conv2d_backward_weight", "vconv_conv2d_backward_weight", false},
    {"conv2d_backward_bias", "vconv_conv2d_backward_bias", false},
    {"relu_backward", "vconv_relu_backward", false},
    {"relu_fwd", "vconv_relu_fwd", false},
    {"leaky_relu_fwd", "vconv_leaky_relu_fwd", false},
    {"tanh_fwd", "vconv_tanh_fwd", false},
    {"sigmoid_fwd", "vconv_sigmoid_fwd", false},
    {"pixel_shuffle_fwd", "vconv_pixel_shuffle_fwd", false},
    {"load_model", "vconv_load_model", false},
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

    // --- I2C / TWI (mcu_i2c.h + bundled Wire library) ---
    // Zero-arg entry points are ordinary functions (not properties):
    // the emitter emits `arduino_i2c_begin()` and discards the result.
    {"i2c_begin", "arduino_i2c_begin", false},
    {"i2c_end", "arduino_i2c_end", false},
    {"i2c_set_clock", "arduino_i2c_set_clock", false},
    {"i2c_write", "arduino_i2c_write", false},
    {"i2c_read", "arduino_i2c_read", false},
    {"i2c_write_reg", "arduino_i2c_write_reg", false},
    {"i2c_read_reg", "arduino_i2c_read_reg", false},
    {"i2c_available", "arduino_i2c_available", false},

    // --- SPI (mcu_spi.h + bundled SPI library) ---
    {"spi_begin", "arduino_spi_begin", false},
    {"spi_end", "arduino_spi_end", false},
    {"spi_set", "arduino_spi_set", false},
    {"spi_end_transaction", "arduino_spi_end_transaction", false},
    {"spi_transfer", "arduino_spi_transfer", false},
    {"spi_transfer16", "arduino_spi_transfer16", false},

    // --- @realtime telemetry (mcu_realtime.h) ---
    {"realtime_worst_ms", "arduino_realtime_worst_ms", false},
    {"realtime_overruns", "arduino_realtime_overruns", false},
    {"realtime_reset", "arduino_realtime_reset", false},
    {"realtime_worst_ms_named", "arduino_realtime_worst_ms_named", false},
    {"realtime_overruns_named", "arduino_realtime_overruns_named", false},

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

// --- esp32 shared surface ----------------------------------------------
//
// Every `arduino.*` name that has a faithful ESP32 implementation. The
// C symbols are the same `arduino_*` names as ARDUINO_MAP because the
// ESP32 runtime under vyne/runtime/esp32/ keeps the shared surface
// source-compatible. The eeprom_* entries are deliberately absent: NVS
// is a different storage model, so a sketch that calls them fails at
// compile time instead of reading the wrong store.
static const NativeMapEntry ESP32_ARDUINO_MAP[] = {
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

    // --- Math (mcu_math.h) ---
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
    {"random", "arduino_random_variadic", false, true},
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

    // --- I2C (mcu_i2c.h) ---
    {"i2c_begin", "arduino_i2c_begin", false},
    {"i2c_end", "arduino_i2c_end", false},
    {"i2c_set_clock", "arduino_i2c_set_clock", false},
    {"i2c_write", "arduino_i2c_write", false},
    {"i2c_read", "arduino_i2c_read", false},
    {"i2c_write_reg", "arduino_i2c_write_reg", false},
    {"i2c_read_reg", "arduino_i2c_read_reg", false},
    {"i2c_available", "arduino_i2c_available", false},

    // --- SPI (mcu_spi.h) ---
    {"spi_begin", "arduino_spi_begin", false},
    {"spi_end", "arduino_spi_end", false},
    {"spi_set", "arduino_spi_set", false},
    {"spi_end_transaction", "arduino_spi_end_transaction", false},
    {"spi_transfer", "arduino_spi_transfer", false},
    {"spi_transfer16", "arduino_spi_transfer16", false},

    // --- @realtime telemetry (mcu_realtime.h) ---
    {"realtime_worst_ms", "arduino_realtime_worst_ms", false},
    {"realtime_overruns", "arduino_realtime_overruns", false},
    {"realtime_reset", "arduino_realtime_reset", false},
    {"realtime_worst_ms_named", "arduino_realtime_worst_ms_named", false},
    {"realtime_overruns_named", "arduino_realtime_overruns_named", false},

    // --- Interrupts + watchdog (mcu_interrupts.h) ---
    {"attach_interrupt", "arduino_attach_interrupt", false},
    {"detach_interrupt", "arduino_detach_interrupt", false},
    {"interrupt_count", "arduino_interrupt_count", false},
    {"interrupt_reset", "arduino_interrupt_reset", false},
    {"interrupt_fired", "arduino_interrupt_fired", false},
    {"watchdog_enable", "arduino_watchdog_enable", false},
    {"watchdog_reset", "arduino_watchdog_reset()", true},
    {"watchdog_disable", "arduino_watchdog_disable()", true},

    // --- Deliberately absent: eeprom_read_int / eeprom_write_int /
    //     eeprom_read_float / eeprom_write_float. Use esp32.nvs_*.
};

// --- esp32-specific surface --------------------------------------------
//
// ESP32-only names reachable as `esp32.*` (and also as `arduino.*` when
// ESP32_ARDUINO_MAP has no matching entry — the dispatcher searches the
// shared table first, then this one). All C symbols use the `esp32_`
// prefix so they are distinguishable from the shared `arduino_*` surface
// in symbol dumps and link errors.
static const NativeMapEntry ESP32_SPECIFIC_MAP[] = {
    // --- DAC + built-in capacitive touch pins (mcu_pins_ext.h) ---
    {"dac_write", "esp32_dac_write", false},
    {"dac_write_mv", "esp32_dac_write_mv", false},
    {"touch_pin_read", "esp32_touch_pin_read", false},
    {"touch_pin_threshold", "esp32_touch_pin_threshold", false},
    {"touch_pin_attach", "esp32_touch_pin_attach", false},
    {"touch_pin_detach", "esp32_touch_pin_detach", false},

    // --- Memory (mcu_memory.h) ---
    {"heap_largest_free", "esp32_heap_largest_free", false},

    // --- NVS (mcu_nvs.h) ---
    {"nvs_open", "esp32_nvs_open", false},
    {"nvs_close", "esp32_nvs_close", false},
    {"nvs_set_int", "esp32_nvs_set_int", false},
    {"nvs_get_int", "esp32_nvs_get_int", false},
    {"nvs_set_float", "esp32_nvs_set_float", false},
    {"nvs_get_float", "esp32_nvs_get_float", false},
    {"nvs_set_string", "esp32_nvs_set_string", false},
    {"nvs_get_string", "esp32_nvs_get_string", false},
    {"nvs_commit", "esp32_nvs_commit", false},
    {"nvs_erase_all", "esp32_nvs_erase_all", false},
    {"nvs_erase_key", "esp32_nvs_erase_key", false},
    {"nvs_exists", "esp32_nvs_exists", false},

    // --- WiFi (mcu_wifi.h) ---
    {"wifi_mode", "esp32_wifi_mode", false},
    {"wifi_connect", "esp32_wifi_connect", false},
    {"wifi_disconnect", "esp32_wifi_disconnect", false},
    {"wifi_status", "esp32_wifi_status", false},
    {"wifi_local_ip", "esp32_wifi_local_ip", false},
    {"wifi_rssi", "esp32_wifi_rssi", false},
    {"wifi_ssid", "esp32_wifi_ssid", false},
    {"wifi_mac", "esp32_wifi_mac", false},
    {"wifi_hostname", "esp32_wifi_hostname", false},
    {"wifi_scan", "esp32_wifi_scan", false},

    // --- HTTP (mcu_wifi.h) ---
    {"http_get", "esp32_http_get", false},
    {"http_post", "esp32_http_post", false},
    {"http_post_json", "esp32_http_post_json", false},
    {"http_status", "esp32_http_status", false},

    // --- UART1/UART2 (mcu_serial.h). UART2 is absent on C3/C6; the C
    //     layer defines the symbol only when SOC_UART_NUM > 2, so a call
    //     on those chips fails at compile time rather than linking a
    //     missing function. ---
    {"serial1_begin", "esp32_serial1_begin", false},
    {"serial1_print", "esp32_serial1_print", false},
    {"serial1_available", "esp32_serial1_available", false},
    {"serial1_read", "esp32_serial1_read", false},
    {"serial2_begin", "esp32_serial2_begin", false},

    // --- Display (mcu_display.h). Colors are packed RGB565 Int64. ---
    {"display_init", "esp32_display_init", false, false, nullptr, "esp32_display_init_default"},
    {"display_rotation", "esp32_display_rotation", false},
    {"display_width", "esp32_display_width", false},
    {"display_height", "esp32_display_height", false},
    {"display_set_rotation", "esp32_display_set_rotation", false},
    {"display_clear", "esp32_display_clear", false},
    {"display_pixel", "esp32_display_pixel", false},
    {"display_line", "esp32_display_line", false},
    {"display_rect", "esp32_display_rect", false},
    {"display_rect_filled", "esp32_display_rect_filled", false},
    {"display_circle", "esp32_display_circle", false},
    {"display_circle_filled", "esp32_display_circle_filled", false},
    {"display_text", "esp32_display_text", false},
    {"display_text_bg", "esp32_display_text_bg", false},
    {"display_set_text_wrap", "esp32_display_set_text_wrap", false},
    {"display_backlight", "esp32_display_backlight", false},
    {"display_push", "esp32_display_push", false},
    {"display_rgb", "esp32_display_rgb", false},

    // --- Sprite / double-buffer surface (mcu_display.h). Create a
    //     TFT_eSprite, select it, draw with the normal display_* calls,
    //     push it, then select(-1) to return to the panel. ---
    {"display_sprite_create", "esp32_display_sprite_create", false},
    {"display_sprite_delete", "esp32_display_sprite_delete", false},
    {"display_sprite_select", "esp32_display_sprite_select", false},
    {"display_sprite_push", "esp32_display_sprite_push", false},
    {"display_sprite_width", "esp32_display_sprite_width", false},
    {"display_sprite_height", "esp32_display_sprite_height", false},
    {"display_sprite_fill", "esp32_display_sprite_fill", false},

    // --- Touchscreen / pen (mcu_touch.h) ---
    {"touch_screen_init", "esp32_touch_screen_init", false},
    {"touch_screen_configured", "esp32_touch_screen_configured", false},
    {"touch_screen_calibrated", "esp32_touch_screen_calibrated", false},
    {"touch_screen_pressed", "esp32_touch_screen_pressed", false},
    {"touch_screen_x", "esp32_touch_screen_x", false},
    {"touch_screen_y", "esp32_touch_screen_y", false},
    {"touch_screen_pressure", "esp32_touch_screen_pressure", false},
    {"touch_screen_read", "esp32_touch_screen_read", false},
    {"touch_screen_wait", "esp32_touch_screen_wait", false},
    {"touch_screen_wait_release", "esp32_touch_screen_wait_release", false},
    {"touch_screen_calibrate", "esp32_touch_screen_calibrate", false},

    // --- IMU / pen (mcu_imu.h) ---
    {"imu_init", "esp32_imu_init", false},
    {"imu_ok", "esp32_imu_ok", false},
    {"imu_accel_x", "esp32_imu_accel_x", false},
    {"imu_accel_y", "esp32_imu_accel_y", false},
    {"imu_accel_z", "esp32_imu_accel_z", false},
    {"imu_gyro_x", "esp32_imu_gyro_x", false},
    {"imu_gyro_y", "esp32_imu_gyro_y", false},
    {"imu_gyro_z", "esp32_imu_gyro_z", false},
    {"imu_temp", "esp32_imu_temp", false},
    {"imu_read", "esp32_imu_read", false},

    // --- Power / sleep (mcu_power.h) ---
    {"power_deep_sleep_us", "esp32_power_deep_sleep_us", false},
    {"power_light_sleep_us", "esp32_power_light_sleep_us", false},
    {"power_wake_timer", "esp32_power_wake_timer", false},
    {"power_wake_pin", "esp32_power_wake_pin", false},
    {"power_wake_cause", "esp32_power_wake_cause", false},
    {"power_restart", "esp32_power_restart", false},
};
