/* vyne/runtime/modules/vcore.h */
#ifndef VYNE_VCORE_RT_H
#define VYNE_VCORE_RT_H

#include "../vyne_runtime.h"
#include <time.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
    #define NOUSER
    #include <windows.h>
    #include <psapi.h>
#else
    #include <unistd.h>
#endif

static inline VyneValue vcore_runtime_now() {
    static char buf[64];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return vyne_string(buf);
}

static inline VyneValue vcore_get_mem() {
    size_t rss = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) rss = pmc.WorkingSetSize;
#else
    rss = 0; // Linux implementation simplified for now
#endif
    return vyne_float((double)rss);
}

#endif