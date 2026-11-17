// Common reporting and correctness policy; diagnostics are outside timed batches.
#pragma once
#include <Arduino.h>
#include <cmath>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "weights.h"

static volatile float bench_sink = 0.0f;
static int64_t bench_elapsed[20] = {};
static int64_t bench_setup_us = -1; // -1 means not measured, never zero overhead.
static size_t bench_arena_reserved = 0, bench_arena_used = 0, bench_scratch_bytes = 0;
static float bench_max_error = 0.0f;
static unsigned bench_checked = 0;

[[noreturn]] static void bench_fail(const char* reason) {
    Serial.printf("BENCH_ERROR implementation=%s reason=%s\n", BENCH_IMPLEMENTATION, reason);
    Serial.flush();
    for (;;) delay(1000);
}

static void bench_check(int index, float actual) {
    if (index < 0 || index >= 80) bench_fail("reference_index");
    float error = fabsf(actual - bench_expected[index]);
    if (!std::isfinite(actual) || error > 0.00002f + 0.0002f * fabsf(bench_expected[index]))
        bench_fail("numerical_mismatch");
    if (error > bench_max_error) bench_max_error = error;
    ++bench_checked;
}

static void bench_report() {
    if (bench_checked != 80) bench_fail("incomplete_validation");
    // ESP-IDF reports high-water marks in bytes (not upstream FreeRTOS words).
    size_t stack_free_min = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("BENCH_META implementation=%s model=%s cpu_mhz=%u psram_bytes=%u arena_capacity=%u arena_used=%u scratch_bytes=%u setup_us=%lld sketch_bytes=%u heap_free=%u heap_min=%u heap_largest=%u stack_free_min_bytes=%u stack_reserved_bytes=%u checked=%u max_error=%.9g\n",
        BENCH_IMPLEMENTATION, BENCH_MODEL_SHA, getCpuFrequencyMhz(),
        unsigned(ESP.getPsramSize()),
        unsigned(bench_arena_reserved), unsigned(bench_arena_used), unsigned(bench_scratch_bytes),
        (long long)bench_setup_us, unsigned(ESP.getSketchSize()),
        unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        unsigned(stack_free_min), unsigned(getArduinoLoopTaskStackSize()), bench_checked, double(bench_max_error));
    for (int batch = 0; batch < 20; ++batch) {
        if (bench_elapsed[batch] <= 0) bench_fail("timer");
        Serial.printf("BENCH_SAMPLE implementation=%s batch=%d iterations=100 elapsed_us=%lld\n",
                     BENCH_IMPLEMENTATION, batch, (long long)bench_elapsed[batch]);
    }
    Serial.printf("BENCH_DONE implementation=%s\n", BENCH_IMPLEMENTATION);
    Serial.flush();
}
