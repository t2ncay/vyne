// Allocation-free interval accounting; caller supplies synchronization and clock.
#pragma once
#include <stdint.h>
#include <limits.h>

struct VyneWiFiSniffCounts {
    uint32_t frames[3] = {}; // management, data, control
    bool saturated = false;

    void record(unsigned category) {
        if (category >= 3) return;
        if (frames[category] == UINT32_MAX) saturated = true;
        else ++frames[category];
    }
};

struct VyneWiFiSniffSnapshot {
    VyneWiFiSniffCounts counts;
    uint64_t elapsed_us = 0;
};

static inline bool vyne_wifi_sniff_take(VyneWiFiSniffCounts& pending,
                                        uint64_t& previous_us, uint64_t now_us,
                                        VyneWiFiSniffSnapshot& snapshot) {
    const uint64_t elapsed = now_us - previous_us;
    if (!elapsed) return false; // Preserve pending counts and the last snapshot.
    snapshot.counts = pending;
    snapshot.elapsed_us = elapsed;
    pending = VyneWiFiSniffCounts{};
    previous_us = now_us;
    return true;
}
