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
