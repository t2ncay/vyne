// Run the real driver logic against injected hardware outcomes and task races.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <initializer_list>
#include <thread>
#include "../../../../vyne/runtime/esp32/detail/mcu_wifi_sniff_driver.h"

static std::atomic<unsigned> allocations{0};
void* operator new(std::size_t bytes) {
    ++allocations;
    if (void* result = std::malloc(bytes)) return result;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

static void reset_mock() {
    sniff_mock::calls = sniff_mock::fail_call = 0;
    sniff_mock::fail_start = sniff_mock::fail_stop = false;
}

int main() {
    using namespace vyne_wifi_sniff;
    for (int64_t invalid : {int64_t(0), int64_t(-1), int64_t(15), INT64_MAX}) {
        assert(!start(invalid) && last_error == ESP_ERR_INVALID_ARG);
        assert(sniff_mock::calls == 0);
    }
    sniff_mock::mode = WIFI_STA;
    assert(!start(6) && last_error == ESP_ERR_INVALID_STATE);
    assert(sniff_mock::mode == WIFI_STA); // Existing radio ownership preserved.
    sniff_mock::mode = WIFI_OFF;
    assert(!start(12) && last_error == ESP_ERR_INVALID_ARG); // Country 1..11.
    assert(!owns_wifi && sniff_mock::mode == WIFI_OFF);
    reset_mock(); sniff_mock::fail_start = true;
    assert(!start(6) && last_error == ESP_FAIL && !owns_wifi);
    for (int failure = 1; failure <= 7; ++failure) {
        reset_mock(); sniff_mock::fail_call = failure;
        assert(!start(6) && last_error == 42);
        assert(!active && !owns_wifi && sniff_mock::mode == WIFI_OFF);
        assert(sniff_mock::callback == nullptr);
    }
    reset_mock();
    assert(start(6) && active && owns_wifi && sniff_mock::enabled);
    assert(sniff_mock::filter == 7 && sniff_mock::control_filter == WIFI_PROMIS_CTRL_FILTER_MASK_ALL);
    assert(!start(1) && channel == 6); // Re-entry does not retune/reset capture.
    wifi_promiscuous_pkt_t packet = {{0, 24}};
    const unsigned before = allocations.load();
    receive(&packet, WIFI_PKT_MGMT); receive(&packet, WIFI_PKT_DATA); receive(&packet, WIFI_PKT_CTRL);
    receive(nullptr, WIFI_PKT_MGMT); receive(nullptr, WIFI_PKT_MISC);
    receive(&packet, WIFI_PKT_MISC);
    packet.rx_ctrl.sig_len = 1; receive(&packet, WIFI_PKT_DATA);
    packet.rx_ctrl.sig_len = 24; packet.rx_ctrl.rx_state = 1; receive(&packet, WIFI_PKT_DATA);
    packet.rx_ctrl.rx_state = 0;
    assert(!take()); // Zero duration does not reset the three pending frames.
    sniff_mock::clock_us += 500000;
    assert(take() && snapshot.elapsed_us == 500000);
    for (auto count : snapshot.counts.frames) assert(count == 1);
    assert(allocations.load() == before); // No allocations in receive/snapshot.
    sniff_mock::clock_us += 750000;
    assert(take() && snapshot.elapsed_us == 750000);
    for (auto count : snapshot.counts.frames) assert(count == 0);

    // Contended callback/snapshot intervals conserve every observed frame.
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (unsigned i = 0; i < 100000; ++i) receive(&packet, WIFI_PKT_DATA);
        done = true;
    });
    uint64_t observed = 0;
    do {
        ++sniff_mock::clock_us;
        assert(take()); observed += snapshot.counts.frames[1];
    } while (!done);
    producer.join();
    ++sniff_mock::clock_us; assert(take()); observed += snapshot.counts.frames[1];
    assert(observed == 100000);

    assert(stop() && !active && !owns_wifi && sniff_mock::mode == WIFI_OFF);
    receive(&packet, WIFI_PKT_DATA);
    assert(!take() && last_error == ESP_ERR_INVALID_STATE);
    reset_mock(); assert(start(6));
    sniff_mock::fail_stop = true;
    assert(!stop() && !active && owns_wifi);
    assert(!start(6));
    sniff_mock::fail_stop = false;
    assert(stop() && !owns_wifi && stop());

    VyneWiFiSniffCounts counts;
    VyneWiFiSniffSnapshot snap;
    uint64_t prior = UINT64_MAX - 10;
    counts.frames[0] = UINT32_MAX;
    counts.record(0); counts.record(3);
    assert(vyne_wifi_sniff_take(counts, prior, 9, snap));
    assert(snap.elapsed_us == 20 && snap.counts.frames[0] == UINT32_MAX && snap.counts.saturated);
    assert(counts.frames[0] == 0 && !counts.saturated);
    std::printf("SNIFF_HOST_PASS: classification, failures, ownership, 100000 contended frames, saturation/wrap, no receive allocations\n");
}
