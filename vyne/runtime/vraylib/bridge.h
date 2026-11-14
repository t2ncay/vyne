// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include <stddef.h>
#include <stdint.h>

// This boundary deliberately includes neither windows.h nor any raylib header.
// All native graphics types stay in bridge.c, in a separate translation unit.
enum { VR_NULL, VR_INT, VR_REAL, VR_BOOL, VR_TEXT, VR_LIST, VR_RECORD, VR_HANDLE };
typedef struct VyneRayValue VyneRayValue;
struct VyneRayValue {
    int kind;
    int64_t integer;
    double real;
    const char *text;
    const char *name;
    VyneRayValue *items;
    size_t count;
};
typedef struct VyneRayResult {
    VyneRayValue value;
    const char *error;
} VyneRayResult;

VyneRayResult vyne_ray_call(int id, const VyneRayValue *args, size_t count);
void vyne_ray_dispose(VyneRayValue value);

// Ids shared with the generated public binding table.
enum {
    VR_BUFFER = -1, VR_READ = -2, VR_WRITE = -3, VR_RELEASE = -4,
    VR_FIELD = -5, VR_SET_FIELD = -6, VR_LIVE = -7, VR_SHUTDOWN = -8,
    VR_MODEL_TEXTURE = -9, VR_MODEL_SHADER = -10, VR_RENDER_TEXTURE = -11,
    VR_BUFFER_SIZE = -12, VR_VALID = -13, VR_ENVIRONMENT = -14
};
