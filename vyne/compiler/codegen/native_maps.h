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
    bool        isProperty;
    bool        usesArgv  = false;
    const char* nativeF64 = nullptr;
};

// --- vcore -----------------------------------------------------------------
static const NativeMapEntry VCORE_MAP[] = {
    {"now",             "vcore_runtime_now",              false},
    {"sleep",           "vcore_runtime_sleep",            false},
    {"platform",        "vcore_runtime_platform",         false},
    {"input",           "vcore_runtime_input",            false, true},
    {"parse_array",     "vcore_parse_array",              false},
    {"hex_to_int64",    "vcore_hex_to_int64",             false},

    {"version",         "vcore_get_version()",            true},
    {"engine",          "vcore_get_engine()",             true},
    {"build",           "vcore_get_build()",              true},
    {"processor_count", "vcore_get_processor_count()",    true},
    {"pid",             "vcore_get_pid()",                true},
    {"memory_usage",    "vcore_get_mem()",                true},
};

// --- vmath -----------------------------------------------------------------
// nativeF64 = unboxed double→double variant, used when every argument is a
// provable Float64. See MethodCallNode::getCExpr for the dispatcher.
static const NativeMapEntry VMATH_MAP[] = {
    // --- 1-arg functions ---
    {"sqrt",    "vmath_sqrt",    false, false, "vmath_sqrt_f64"},
    {"abs",     "vmath_abs",     false, false, "vmath_abs_f64"},
    {"sin",     "vmath_sin",     false},
    {"cos",     "vmath_cos",     false},
    {"tan",     "vmath_tan",     false},
    {"asin",    "vmath_asin",    false},
    {"acos",    "vmath_acos",    false},
    {"atan",    "vmath_atan",    false},
    {"sinh",    "vmath_sinh",    false},
    {"cosh",    "vmath_cosh",    false},
    {"tanh",    "vmath_tanh",    false},
    {"log",     "vmath_log",     false, false, "vmath_log_f64"},
    {"log10",   "vmath_log10",   false},
    {"exp",     "vmath_exp",     false, false, "vmath_exp_f64"},
    {"floor",   "vmath_floor",   false, false, "vmath_floor_f64"},
    {"ceil",    "vmath_ceil",    false, false, "vmath_ceil_f64"},
    {"round",   "vmath_round",   false, false, "vmath_round_f64"},
    {"erf",     "vmath_erf",     false},
    {"erfc",    "vmath_erfc",    false},
    {"tgamma",  "vmath_tgamma",  false},
    {"lgamma",  "vmath_lgamma",  false},
    {"sigmoid", "vmath_sigmoid", false, false, "vmath_sigmoid_f64"},
    {"relu",    "vmath_relu",    false, false, "vmath_relu_f64"},
    {"degrees", "vmath_degrees", false},
    {"radians", "vmath_radians", false},

    // --- 2-arg functions ---
    {"pow",     "vmath_pow",     false, false, "vmath_pow_f64"},
    {"hypot",   "vmath_hypot",   false, false, "vmath_hypot_f64"},
    {"atan2",   "vmath_atan2",   false},
    {"fmod",    "vmath_fmod",    false, false, "vmath_fmod_f64"},
    {"min",     "vmath_min",     false, false, "vmath_min_f64"},
    {"max",     "vmath_max",     false, false, "vmath_max_f64"},

    // --- 3-arg functions ---
    {"clamp",   "vmath_clamp",   false, false, "vmath_clamp_f64"},

    // --- variadic / special (no unboxed variant) ---
    {"random",       "vmath_random",       false},
    {"random_float", "vmath_random_float", false},

    // --- constants (zero-arg getters) ---
    {"pi",      "vmath_pi()",    true},
    {"e",       "vmath_e()",     true},
    {"tau",     "vmath_tau()",   true},
    {"phi",     "vmath_phi()",   true},
    {"sqrt2",   "vmath_sqrt2()", true},
    {"inf",     "vmath_inf()",   true},
    {"nan",     "vmath_nan()",   true},
};