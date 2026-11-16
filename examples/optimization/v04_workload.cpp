// Controlled benchmark target: independent monotonic timing and touched memory.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char** argv) {
    using clock = std::chrono::steady_clock;
    if (argc > 1) {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }
    const auto start = clock::now();
    if (std::getenv("V04_DESCENDANT")) {
#ifdef _WIN32
        STARTUPINFOA startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION child = {};
        char command[32768];
        std::snprintf(command, sizeof(command), "\"%s\" --descendant", argv[0]);
        if (!CreateProcessA(nullptr, command, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &child)) return 3;
        std::printf("descendant_pid: %lu\n", child.dwProcessId);
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
#else
        (void)argv;
        const pid_t child = fork();
        if (child < 0) return 3;
        if (child == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(20));
            _exit(0);
        }
        std::printf("descendant_pid: %ld\n", static_cast<long>(child));
#endif
        std::fflush(stdout);
    }
    const char* duration = std::getenv("V04_DURATION_MS");
    const char* memory = std::getenv("V04_MEMORY_BYTES");
    const char* burst = std::getenv("V04_BURST");
    const int ms = duration ? std::atoi(duration) : 20;
    const size_t bytes = memory ? std::strtoull(memory, nullptr, 10) : 0;
    volatile unsigned char* data = bytes
        ? static_cast<unsigned char*>(std::malloc(bytes)) : nullptr;
    if (bytes && !data) return 2;
    unsigned long long checksum = 0;
    for (size_t i = 0; i < bytes; i += 4096) {
        data[i] = 7;
        checksum += data[i];
    }
    if (burst) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::free(const_cast<unsigned char*>(data));
        data = nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    std::free(const_cast<unsigned char*>(data));
    const double elapsed = std::chrono::duration<double>(clock::now() - start).count();
    std::printf("work_seconds: %.9f\nchecksum: %llu\n", elapsed, checksum);
    if (std::getenv("V04_MALFORMED_ARENA")) std::fputs("vmem dump\n  main: live=oops\n", stderr);
    if (std::getenv("V04_OUTPUT_VOLUME")) {
        for (int i = 0; i < 4096; ++i) {
            std::puts("stdout drainage test: abcdefghijklmnopqrstuvwxyz");
            std::fputs("stderr drainage test: abcdefghijklmnopqrstuvwxyz\n", stderr);
        }
    }
    return 0;
}
