// SPDX-License-Identifier: MIT
// A single user translation unit includes Windows-dependent Vyne + both image
// libraries, while raylib stays isolated in its separately compiled C bridge.
#include "../../../../vyne/runtime/modules/vraylib.h"
#include "../../../../vyne/runtime/modules/vimage.h"
#include <assert.h>
static uint32_t vraylib_field_id(const char *s) { return _vyne_hash_str(s); }

int main(void) {
    VyneValue image=vraylib_gen_image_color(vyne_int(3),vyne_int(2),vraylib_RED());
    assert(vraylib_field(image,vyne_string("width"),vyne_int(0)).as.i64==3);
    VyneValue copy=vraylib_image_copy(image);
    vraylib_unload_image(image);
    assert(vraylib_is_image_valid(copy).as.i64);
    vraylib_unload_image(copy);
    VyneValue buffer=vraylib_buffer(vyne_string("float"),vyne_int(1));
    vraylib_buffer_write(buffer,vyne_int(0),vyne_float(42.5));
    for(int i=0;i<1000;++i) {
        ArenaCheckpoint cp=arena_checkpoint();
        assert(vraylib_buffer_read(buffer,vyne_int(0)).as.f64==42.5);
        arena_rewind(cp);
    }
    vraylib_release(buffer);
    // Buffer metadata must survive a caller-owned, dynamically computed type
    // label being rewound and overwritten. This also exercises C type aliases.
    ArenaCheckpoint cp=arena_checkpoint();
    char *label=arena_alloc(6); memcpy(label,"float",6);
    buffer=vraylib_buffer(vyne_string_own(label),vyne_int(1));
    arena_rewind(cp);
    char *overwrite=arena_alloc(6); memcpy(overwrite,"xxxxx",6);
    vraylib_buffer_write(buffer,vyne_int(0),vyne_float(7.0));
    assert(vraylib_buffer_read(buffer,vyne_int(0)).as.f64==7.0);
    vraylib_release(buffer);
    buffer=vraylib_buffer(vyne_string("Camera"),vyne_int(1));
    vraylib_release(buffer);
    vraylib_shutdown();
    assert(vraylib_live_handles().as.i64==0);
    arena_free_all();
    puts("VRAYLIB_NATIVE_OK");
    return 0;
}
