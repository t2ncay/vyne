/* vyne/runtime/modules/vcore.h
 * -------------------------------------------------------------------
 * Vyne runtime vcore module — transpiler target.
 *
 * Mirror of the interpreter's VCoreNative namespace, ported to
 * static-inline C functions operating on VyneValue.
 *
 * Naming convention: vcore_<name>
 *   - Methods    -> vcore_runtime_<name>(VyneValue)
 *   - Properties -> vcore_get_<name>() or inline expressions
 *   - `input` is variadic; see vcore_runtime_input(int, VyneValue*).
 * ------------------------------------------------------------------- */

#ifndef VYNE_VCORE_RT_H
#define VYNE_VCORE_RT_H

#include "../vyne_runtime.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- platform detection -------------------------------------- */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
    #define NOUSER
    #include <windows.h>
    #include <psapi.h>
    #include <process.h>
    #define vyne_getpid _getpid
#else
    #include <unistd.h>
    #include <sys/types.h>
#endif

/* ---------- now() — same format as interpreter ---------------------- */
static inline VyneValue vcore_runtime_now(void) {
    static char buf[64];
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return vyne_string(buf);
}

/* ---------- sleep(ms) ---------------------------------------------- */
static inline VyneValue vcore_runtime_sleep(VyneValue ms_val) {
    int64_t ms = (ms_val.type == V_INT64) ? ms_val.as.i64
                                          : (int64_t)ms_val.as.f64;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
    return vyne_int(1);   /* interpreter returns Value(true) -> Int64 1 */
}

/* ---------- platform() — compile-time string ------------------------ */
static inline VyneValue vcore_runtime_platform(void) {
#if defined(_WIN64)
    #if defined(_M_ARM64)
        return vyne_string("Windows ARM64");
    #else
        return vyne_string("Windows x64");
    #endif
#elif defined(_WIN32)
    return vyne_string("Windows x86");
#elif defined(__APPLE__)
    return vyne_string("Mac OSX");
#elif defined(__linux__)
    return vyne_string("Linux");
#elif defined(__FreeBSD__)
    return vyne_string("FreeBSD");
#elif defined(__unix__) || defined(__unix)
    return vyne_string("Unix");
#else
    return vyne_string("Other/Unknown");
#endif
}

/* ---------- input([prompt]) — variadic, matches interpreter --------- */
static inline VyneValue vcore_runtime_input(int argc, VyneValue* argv) {
    if (argc > 0 && argv[0].type == V_STRING && argv[0].as.str) {
        printf("%s", argv[0].as.str);
        fflush(stdout);
    }
    char buf[4096];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        return vyne_string(buf);
    }
    return vyne_null();
}

/* ---------- parse_array("[1, 2, [3, 4]]") --------------------------- */
static inline const char* _vcore_pa_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static VyneValue _vcore_pa_elem(const char** p) {
    *p = _vcore_pa_ws(*p);

    if (**p == '[') {
        (*p)++;
        VyneValue arr = vyne_array_create(0);
        *p = _vcore_pa_ws(*p);
        while (**p && **p != ']') {
            VyneValue elem = _vcore_pa_elem(p);
            vyne_array_push(arr, elem);
            *p = _vcore_pa_ws(*p);
            if (**p == ',') { (*p)++; *p = _vcore_pa_ws(*p); }
        }
        if (**p == ']') (*p)++;
        return arr;
    }

    const char* start = *p;
    while (**p && (isdigit((unsigned char)**p) || **p == '.' ||
                   **p == '-' || **p == '+' || **p == 'e' || **p == 'E')) {
        (*p)++;
    }
    size_t len = (size_t)(*p - start);
    if (len == 0) return vyne_float(0.0);

    char tmp[64];
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, start, len);
    tmp[len] = '\0';

    if (strchr(tmp, '.') || strchr(tmp, 'e') || strchr(tmp, 'E'))
        return vyne_float(atof(tmp));
    return vyne_int((int64_t)strtoll(tmp, NULL, 10));
}

static inline VyneValue vcore_parse_array(VyneValue s) {
    if (s.type != V_STRING || !s.as.str) {
        fprintf(stderr, "Runtime error: vcore.parse_array() expects 1 String argument.\n");
        exit(1);
    }
    const char* p = s.as.str;
    return _vcore_pa_elem(&p);
}

/* ---------- hex_to_int64("0xFF2D809A") ------------------------------ */
static inline VyneValue vcore_hex_to_int64(VyneValue s) {
    if (s.type != V_STRING || !s.as.str) {
        fprintf(stderr, "Runtime error: vcore.hex_to_int64() expects 1 String argument.\n");
        exit(1);
    }
    const char* p = s.as.str;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    return vyne_int((int64_t)strtoull(p, NULL, 16));
}

/* ---------- properties ---------------------------------------------- */
static inline VyneValue vcore_get_version(void) { return vyne_string("v0.0.1-alpha"); }
static inline VyneValue vcore_get_engine(void)  { return vyne_string("Vyne Native"); }
static inline VyneValue vcore_get_build(void)   { return vyne_string(__DATE__ " " __TIME__); }

static inline VyneValue vcore_get_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return vyne_int((int64_t)si.dwNumberOfProcessors);
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return vyne_int(n > 0 ? (int64_t)n : 1);
#endif
}

/* Matches interpreter: pid stored as Float64 */
static inline VyneValue vcore_get_pid(void) {
    return vyne_float((double)vyne_getpid());
}

/* Matches interpreter: bytes as Float64 */
static inline VyneValue vcore_get_mem(void) {
    double rss = 0.0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        rss = (double)pmc.WorkingSetSize;
#else
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        long pages = 0;
        if (fscanf(fp, "%*s%ld", &pages) == 1)
            rss = (double)pages * sysconf(_SC_PAGESIZE);
        fclose(fp);
    }
#endif
    return vyne_float(rss);
}

#endif /* VYNE_VCORE_RT_H */