// SPDX-License-Identifier: MIT
#include "../modules/vraylib.h"
#include <assert.h>
static uint32_t vraylib_field_id(const char *s) { return _vyne_hash_str(s); }

static void rejected(VyneValue session,VyneValue enabled) {
    unsigned int depth=vraylib_transient_error_depth;
    int mark=vyne_try_push();
    if(vyne_setjmp(g_exc_stack[mark].buf)==0) {
        vraylib_managed_config(session,vyne_string("transient_errors"),enabled);
        assert(!"configuration should fail");
    }
    assert(vraylib_transient_error_depth==depth);
}
int main(void) {
    VyneValue session=vraylib_managed_session(vyne_string(""));
    rejected(session,vyne_bool(1)); // No active frame: failure must not enable.
    assert(vraylib_managed_frame(session).as.i64);
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(1));
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(1));
    assert(vraylib_transient_error_depth==2);
    rejected(vyne_int(0),vyne_bool(0));
    rejected(session,vyne_string("yes"));
    rejected(session,vyne_float(0.5));
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(0));
    assert(vraylib_transient_error_depth==1);
    vraylib_transient_error_depth=UINT_MAX;
    rejected(session,vyne_bool(1));
    vraylib_transient_error_depth=1;
    size_t committed=g_commit_arena.total_allocated;
    VyneValue recent[64];
    for(int i=0;i<4096;++i) {
        ArenaCheckpoint cp=arena_checkpoint();
        char text[80]; snprintf(text,sizeof(text),"recoverable %d",i);
        recent[i%64]=vraylib_promote_exception(vyne_string(text));
        arena_rewind(cp);
    }
    assert(g_commit_arena.total_allocated==committed);
    for(int i=4032;i<4096;++i) {
        char text[80]; snprintf(text,sizeof(text),"recoverable %d",i);
        assert(!strcmp(recent[i%64].as.str,text));
    }
    // Rethrow into the oldest ring slot, including the exact same source slot.
    VyneValue saved=vraylib_promote_exception(recent[0]);
    assert(!strcmp(saved.as.str,"recoverable 4032"));
    char short_text[1024]; memset(short_text,'x',1023); short_text[1023]=0;
    assert(strlen(vraylib_promote_exception(vyne_string(short_text)).as.str)==1023);
    assert(g_commit_arena.total_allocated==committed);
    char long_text[1025]; memset(long_text,'y',1024); long_text[1024]=0;
    VyneValue long_error=vraylib_promote_exception(vyne_string(long_text));
    assert(g_commit_arena.total_allocated>committed);
    VyneValue structured=vyne_array_create(1);
    structured.as.arr->elements[0]=vyne_string("structured payload");
    structured=vraylib_promote_exception(structured);
    vraylib_managed_finish(session);
    assert(vraylib_transient_error_depth==0 && !vraylib_frame_active);
    assert(!strcmp(saved.as.str,"recoverable 4032"));
    assert(strlen(long_error.as.str)==1024);
    assert(!strcmp(structured.as.arr->elements[0].as.str,"structured payload"));
    assert(vraylib_managed_frame(session).as.i64);
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(1));
    assert(vraylib_managed_frame(session).as.i64);
    assert(vraylib_transient_error_depth==0); // Starting next frame resets nesting.
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(0));
    assert(vraylib_transient_error_depth==0); // No unsigned underflow.
    vraylib_managed_config(session,vyne_string("transient_errors"),vyne_bool(1));
    vraylib_managed_close(session);
    assert(vraylib_transient_error_depth==0 && !g_vyne_exception_promoter);
    VyneValue fresh=vraylib_managed_session(vyne_string(""));
    assert(vraylib_managed_frame(fresh).as.i64);
    vraylib_managed_config(fresh,vyne_string("transient_errors"),vyne_bool(1));
    vraylib_managed_close(session);
    assert(vraylib_transient_error_depth==1); // Stale close is harmless.
    vraylib_shutdown();
    assert(vraylib_transient_error_depth==0 && vraylib_live_handles().as.i64==0);
    arena_free_all();
    puts("VRAYLIB_RECOVERY_NATIVE_OK");
}
