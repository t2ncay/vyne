#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <thread>

// Platformaya uyğun başlıqlar
#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
    #include <psapi.h>
    #define getpid _getpid
#else
    #include <unistd.h>
    #include <sys/resource.h>
#endif

class SymbolContainer; 
class StringPool;

static inline const char* vcore_runtime_now() {
    static char buf[64];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

static inline void vcore_runtime_sleep(long long ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms * 1000);
#endif
}

static inline double vcore_get_mem() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (double)pmc.WorkingSetSize;
#else
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        long rss;
        if (fscanf(fp, "%*s%ld", &rss) == 1) {
            fclose(fp);
            return (double)rss * sysconf(_SC_PAGESIZE);
        }
        fclose(fp);
    }
#endif
    return 0.0;
}

static inline int vcore_get_pid() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

struct NativeMapping {
    std::string vyneName;
    std::string cName;
    bool isProperty;
};

// VCore modulunun bütün mapping-lərini saxlayan siyahı
const std::vector<NativeMapping> VCORE_MAP = {
    {"now",          "vcore_runtime_now",   false},
    {"sleep",        "vcore_runtime_sleep", false},
    {"memory_usage", "vcore_get_mem",       true},
    {"pid",          "vcore_get_pid",       true}
};

void setupVCore(SymbolContainer& env, StringPool& pool);

double getPhysicalMemoryUsage();