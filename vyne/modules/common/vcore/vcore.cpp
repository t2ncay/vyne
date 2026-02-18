#include "vcore.h"

#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
    #include <psapi.h>
    #define getpid _getpid
#elif __linux__
    #include <unistd.h>
    #include <fstream>
#elif __APPLE__
    #include <mach/mach.h>
#endif

double getPhysicalMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize);
    }
#elif __linux__
    long rss = 0L;
    std::ifstream fp("/proc/self/statm");
    if (fp >> rss) {
        return static_cast<double>(rss) * sysconf(_SC_PAGESIZE);
    }
#elif __APPLE__
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        return static_cast<double>(info.resident_size);
    }
#endif
    return 0.0;
}

/**
 * VCore Native Method Implementations
 */
namespace VCoreNative {

    Value now(std::vector<Value>& args) {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        #ifdef _WIN32
                localtime_s(&tm, &t);
        #else
                localtime_r(&t, &tm);
        #endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return Value(oss.str());
    }

    Value sleep(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("vcore.sleep() expects 1 argument (ms)");
        long long ms = static_cast<long long>(args[0].asNumber());
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value(true);
    }

    Value platform(std::vector<Value>& args) {
        #if defined(_WIN64)
            #if defined(_M_ARM64)
                return Value("Windows ARM64");
            #else
                return Value("Windows x64");
            #endif
        #elif defined(_WIN32)
            return Value("Windows x86");
        #elif __APPLE__ || __MACH__
            return Value("Mac OSX");
        #elif __linux__
            return Value("Linux");
        #elif __FreeBSD__
            return Value("FreeBSD");
        #elif __unix || __unix__
            return Value("Unix");
        #else
            return Value("Other/Unknown");
        #endif
    }

    /**
     * @brief Reads a line of text from the standard input (stdin).
     * * This native function pauses script execution and waits for the user to press Enter.
     * It can optionally take a single string argument to display as a prompt before 
     * reading input.
     * * @param args A vector containing:
     * - args[0] (Optional): A string prompt to display to the user.
     * * @return Value A string containing the user's input. Returns a Null value if
     * input stream fails or reaches end-of-file (EOF).
     * * @note If a non-string argument is provided as the first parameter, it is 
     * ignored and no prompt is displayed.
     */

    Value input(std::vector<Value>& args){
        if(!args.empty() && args[0].getType() == Value::STRING){
            std::cout << args[0].asString();
        }

        std::string input;
        if(std::getline(std::cin, input)){
            return Value(input);
        }

        return Value();
    }
}

void setupVCore(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "global.vcore";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vcore = env[path];

    // VCore methods
    vcore[pool.intern("now")]             = Value(VCoreNative::now);
    vcore[pool.intern("sleep")]           = Value(VCoreNative::sleep);
    vcore[pool.intern("platform")]        = Value(VCoreNative::platform);
    vcore[pool.intern("input")]           = Value(VCoreNative::input);

    // VCore properties
    vcore[pool.intern("version")]         = Value("v0.0.1-alpha").setReadOnly();
    vcore[pool.intern("engine")]          = Value("Vyne Native").setReadOnly();
    vcore[pool.intern("build")]           = Value(std::string(__DATE__) + " " + std::string(__TIME__)).setReadOnly();
    vcore[pool.intern("cwd")]             = Value(std::filesystem::current_path().string()).setReadOnly();
    vcore[pool.intern("processor_count")] = Value(std::thread::hardware_concurrency());
    vcore[pool.intern("pid")]             = Value(static_cast<double>(getpid()));
    vcore[pool.intern("memory_usage")]    = Value(getPhysicalMemoryUsage()).setReadOnly();
}