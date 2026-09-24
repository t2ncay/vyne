#pragma once
// ============================================================================
// Native module mapping tables.
//
// Shared between C_Emitter (transpiler codegen) and anything else that needs
// to resolve a Vyne native module member (e.g. vcore.now, vmath.sqrt) to the
// corresponding C runtime symbol.
//
// This header intentionally does NOT include ast.h, value.h, or any other
// heavyweight header. It must be safe to include from emitter.h without
// creating an include cycle.
//
// Fields:
//   vyneName   - name as used in Vyne source (e.g. "sqrt")
//   cName      - C symbol, or full expression for properties (e.g. "vmath_pi()")
//   isProperty - true for zero-arg constants/getters, false for functions
// ============================================================================
struct NativeMapEntry {
    const char* vyneName;
    const char* cName;
    bool        isProperty;
    bool        usesArgv = false;
};

// --- vcore -----------------------------------------------------------------
// --- vcore -----------------------------------------------------------------
static const NativeMapEntry VCORE_MAP[] = {
    {"now",             "vcore_runtime_now",              false},
    {"sleep",           "vcore_runtime_sleep",            false},
    {"platform",        "vcore_runtime_platform",         false},
    {"input",           "vcore_runtime_input",            false, true},   // ← variadic
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
static const NativeMapEntry VMATH_MAP[] = {
    // --- 1-arg functions ---
    {"sqrt",    "vmath_sqrt",    false},
    {"abs",     "vmath_abs",     false},
    {"sin",     "vmath_sin",     false},
    {"cos",     "vmath_cos",     false},
    {"tan",     "vmath_tan",     false},
    {"asin",    "vmath_asin",    false},
    {"acos",    "vmath_acos",    false},
    {"atan",    "vmath_atan",    false},
    {"sinh",    "vmath_sinh",    false},
    {"cosh",    "vmath_cosh",    false},
    {"tanh",    "vmath_tanh",    false},
    {"log",     "vmath_log",     false},
    {"log10",   "vmath_log10",   false},
    {"exp",     "vmath_exp",     false},
    {"floor",   "vmath_floor",   false},
    {"ceil",    "vmath_ceil",    false},
    {"round",   "vmath_round",   false},
    {"erf",     "vmath_erf",     false},
    {"erfc",    "vmath_erfc",    false},
    {"tgamma",  "vmath_tgamma",  false},
    {"lgamma",  "vmath_lgamma",  false},
    {"sigmoid", "vmath_sigmoid", false},
    {"relu",    "vmath_relu",    false},
    {"degrees", "vmath_degrees", false},
    {"radians", "vmath_radians", false},

    // --- 2-arg functions ---
    {"pow",     "vmath_pow",     false},
    {"hypot",   "vmath_hypot",   false},
    {"atan2",   "vmath_atan2",   false},
    {"fmod",    "vmath_fmod",    false},
    {"min",     "vmath_min",     false},
    {"max",     "vmath_max",     false},

    // --- 3-arg functions ---
    {"clamp",   "vmath_clamp",   false},

    // --- variadic / special ---
    {"random",  "vmath_random",  false},

    // --- constants (zero-arg getters; isProperty = true) ---
    {"pi",      "vmath_pi()",    true},
    {"e",       "vmath_e()",     true},
    {"tau",     "vmath_tau()",   true},
    {"phi",     "vmath_phi()",   true},
    {"sqrt2",   "vmath_sqrt2()", true},
    {"inf",     "vmath_inf()",   true},
    {"nan",     "vmath_nan()",   true},
};