// SPDX-License-Identifier: MIT
// Only the fixed kernel runs on raylib's audio thread. All ownership and
// attach/detach operations run on the application thread under raylib's mutex.
#include "saturation.h"
static VyneRaySaturation vr_saturation;
static int64_t vr_saturation_owner;

static void vr_saturation_callback(void *data, unsigned int frames) {
    vyne_ray_saturation_process(&vr_saturation, (float *)data, frames);
}
static void vr_saturation_detach(int64_t session) {
    if (!vr_saturation_owner || (session && session != vr_saturation_owner)) return;
    if (IsAudioDeviceReady()) DetachAudioMixedProcessor(vr_saturation_callback);
    atomic_store_explicit(&vr_saturation.settings, 0, memory_order_relaxed);
    vr_saturation_owner = 0;
}
static int vr_saturation_enable(int64_t session, float drive, int mode) {
    if (!isfinite(drive) || drive < 0.0f || drive > 1.0f || (mode != 0 && mode != 1))
        vr_fail("saturation drive must be 0..1 and mode must be 0 (soft) or 1 (hard)");
    if (drive == 0.0f) { vr_saturation_detach(session); return 1; }
    if (!IsAudioDeviceReady()) return 0;
    if (vr_saturation_owner && vr_saturation_owner != session) vr_fail("saturation belongs to another application");
    vyne_ray_saturation_set(&vr_saturation, drive, mode);
    if (!vr_saturation_owner) {
        atomic_store_explicit(&vr_saturation.blocks, 0, memory_order_relaxed);
        AttachAudioMixedProcessor(vr_saturation_callback);
        vr_saturation_owner = session;
    }
    return 1;
}
