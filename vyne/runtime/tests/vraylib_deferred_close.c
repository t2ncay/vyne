// SPDX-License-Identifier: MIT
// Test-only native close hook: no public event injection API.
#include <assert.h>
#include "../../../vendor/raylib/src/raylib.h"
static int native_close, consume_on_sample, close_queries;
static bool test_window_should_close(void) {
    ++close_queries;
    int requested=native_close;
    if(consume_on_sample) native_close=0;
    return requested!=0;
}
#define WindowShouldClose test_window_should_close
#include "../vraylib/bridge.c"

static VR call(int id,VR *args,size_t count) {
    VyneRayResult result=vyne_ray_call(id,args,count); assert(!result.error); return result.value;
}
static VR text(const char *value) { VR out={0}; out.kind=VR_TEXT; out.text=value; return out; }
static VR config(int64_t session,const char *key,VR value) {
    VR args[]={vr_int(session),text(key),value}; return call(-27,args,3);
}
static int frame(int64_t session) { VR owner=vr_int(session); return call(-30,&owner,1).integer!=0; }
static int requested(int64_t session) {
    VR args[]={vr_int(session),text("close_requested")};
    int queries=close_queries, pending=call(-28,args,2).integer!=0;
    assert(close_queries==queries); // Getter must not consume the native flag.
    return pending;
}
static int64_t open_session(void) {
    native_close=0;
    VR root=text(""); int64_t session=call(-15,&root,1).integer;
    VR window[]={vr_int(session),vr_int(160),vr_int(80),text("Deferred close regression"),vr_int(0),vr_int(FLAG_WINDOW_HIDDEN)};
    call(-25,window,6); return session;
}
static void close_session(int64_t session) {
    VR owner=vr_int(session); call(-18,&owner,1);
    assert(call(VR_LIVE,NULL,0).integer==0);
}
static void poll_events(void) {
    // GLFW retains the sampled close level until EndDrawing polls events.
    BeginDrawing(); EndDrawing(); native_close=0;
}
int main(void) {
    int64_t session=open_session();
    assert(!requested(session));
    native_close=1;
    assert(!frame(session)); // Default still terminates on native close.
    assert(!requested(session));
    close_session(session);

    session=open_session(); config(session,"defer_close",vr_bool(1));
    assert(frame(session) && !requested(session));
    native_close=1;
    int queries=close_queries;
    assert(frame(session) && close_queries==queries+1);
    assert(requested(session) && !requested(session));
    poll_events();
    assert(frame(session) && !requested(session)); // Cancel does not recur.
    native_close=1; assert(frame(session));
    poll_events();
    assert(frame(session)); // Unread event survives frame rollover.
    config(session,"defer_close",vr_bool(1));
    assert(requested(session) && !requested(session));
    native_close=0; assert(frame(session));
    // Also support a backend whose WindowShouldClose getter consumes the flag.
    consume_on_sample=1; native_close=1;
    assert(frame(session) && native_close==0 && requested(session));
    assert(frame(session) && !requested(session));
    native_close=1; assert(frame(session) && requested(session));
    config(session,"stop",vr_bool(1));
    queries=close_queries;
    assert(!frame(session) && close_queries==queries); // Stop is definitive.
    assert(!frame(session));
    close_session(session);

    session=open_session(); config(session,"defer_close",vr_bool(1));
    config(session,"limit",vr_int(2));
    native_close=1; assert(frame(session) && requested(session));
    native_close=1; assert(frame(session) && requested(session));
    native_close=1; queries=close_queries;
    assert(!frame(session) && close_queries==queries);
    assert(vr_session(session)->frames==2); // Deferred requests do not evade limit.
    close_session(session);

    consume_on_sample=0;
    session=open_session(); config(session,"defer_close",vr_bool(1));
    native_close=1; assert(frame(session));
    config(session,"defer_close",vr_bool(0));
    assert(!requested(session) && !frame(session));
    close_session(session);

    session=open_session(); config(session,"defer_close",vr_bool(1));
    CloseWindow();
    assert(!frame(session)); // An actually lost window cannot be deferred.
    close_session(session);

    VR root=text(""); session=call(-15,&root,1).integer;
    config(session,"defer_close",vr_bool(1)); config(session,"limit",vr_int(1));
    queries=close_queries;
    assert(frame(session) && !requested(session));
    assert(!frame(session) && close_queries==queries);
    close_session(session);
    call(VR_SHUTDOWN,NULL,0);
    assert(call(VR_LIVE,NULL,0).integer==0);
    puts("VRAYLIB_DEFERRED_CLOSE_OK");
}
