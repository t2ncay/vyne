// Persistent TFLM lifecycle. Build stages this file and generated model headers.
#include <Arduino.h>
#include <esp_timer.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model.h"
#include "weights.h"
#define BENCH_IMPLEMENTATION "tflm"
#include "report.h"

#ifndef BENCH_TFLM_ARENA_BYTES
#define BENCH_TFLM_ARENA_BYTES 16384
#endif
const int kTensorArenaSize = BENCH_TFLM_ARENA_BYTES;
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];
static tflite::MicroMutableOpResolver<2> resolver;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

static void invoke(int vector) {
    memcpy(input_tensor->data.f, bench_inputs + vector * 64, 64 * sizeof(float));
    if (interpreter->Invoke() != kTfLiteOk) bench_fail("invoke");
    for (int j = 0; j < 10; ++j) bench_sink = output_tensor->data.f[j];
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    if (getCpuFrequencyMhz() != 240) bench_fail("cpu_frequency");
    int64_t start = esp_timer_get_time();
    const tflite::Model* model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) bench_fail("schema");
    if (resolver.AddFullyConnected() != kTfLiteOk || resolver.AddRelu() != kTfLiteOk)
        bench_fail("resolver");
    // Model bytes, resolver and arena are static; instance lives forever.
    static tflite::MicroInterpreter instance(model, resolver, tensor_arena,
                                             kTensorArenaSize);
    interpreter = &instance;
    if (interpreter->AllocateTensors() != kTfLiteOk) bench_fail("allocate_tensors");
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
    if (!input_tensor || !output_tensor || input_tensor->type != kTfLiteFloat32 ||
        output_tensor->type != kTfLiteFloat32 || input_tensor->bytes != 64 * sizeof(float) ||
        output_tensor->bytes != 10 * sizeof(float) || interpreter->inputs_size() != 1 ||
        interpreter->outputs_size() != 1 || input_tensor->dims->size != 2 ||
        input_tensor->dims->data[0] != 1 || input_tensor->dims->data[1] != 64 ||
        output_tensor->dims->size != 2 || output_tensor->dims->data[0] != 1 ||
        output_tensor->dims->data[1] != 10) bench_fail("tensor_contract");
    bench_setup_us = esp_timer_get_time() - start;
    bench_arena_reserved = sizeof(tensor_arena);
    bench_arena_used = interpreter->arena_used_bytes();
    bench_scratch_bytes = 0; // Activation storage is included in the tensor arena.
    for (int i = 0; i < 10; ++i) invoke(i % 8);
    for (int batch = 0; batch < 20; ++batch) {
        start = esp_timer_get_time();
        for (int i = 0; i < 100; ++i) invoke(i % 8);
        bench_elapsed[batch] = esp_timer_get_time() - start;
    }
    for (int vector = 0; vector < 8; ++vector) {
        invoke(vector);
        for (int j = 0; j < 10; ++j) bench_check(vector * 10 + j, output_tensor->data.f[j]);
    }
    bench_report();
}

void loop() { delay(1000); }
