#ifndef VYNE_VGLIB_RT_H
#define VYNE_VGLIB_RT_H

#include "../vyne_runtime.h"
#include "raylib.h"          /* adjust path if raylib headers live elsewhere */

/* The only case where an inline pattern can't express it: camera() needs
   an arena-allocated Camera3D that survives the frame. */
static inline VyneValue vyne_vglib_camera(int argc, VyneValue* a) {
    Camera3D* c = (Camera3D*)arena_alloc(sizeof(Camera3D));
    c->position = (Vector3){10,10,10};
    c->target   = (Vector3){0,0,0};
    c->up       = (Vector3){0,1,0};
    c->fovy     = (argc > 0) ? (float)a[0].as.f64 : 45.0f;
    c->projection = CAMERA_PERSPECTIVE;
    return vyne_int((int64_t)c);
}

#endif