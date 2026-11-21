// SPDX-License-Identifier: MIT
// Compile the real bridge with deterministic allocation failure injection.
// raylib itself remains in its normal, separately compiled library.
#include <assert.h>
#include <stdlib.h>
#include "../../../vendor/raylib/src/raylib.h"
static void *allocations[2048];
static int live_allocations, remaining=-1, default_unloads, custom_unloads;
static int reject_allocation(void) {
    if(remaining<0) return 0;
    if(remaining==0) return 1;
    --remaining; return 0;
}
static int allocation_slot(void *p) {
    for(int i=0;i<2048;++i) if(allocations[i]==p) return i;
    return -1;
}
static void *test_calloc(size_t n,size_t size) {
    if(reject_allocation()) return NULL;
    void *p=calloc(n,size);
    if(p) { int i=allocation_slot(NULL); assert(i>=0); allocations[i]=p; ++live_allocations; }
    return p;
}
static void *test_realloc(void *p,size_t size) {
    if(reject_allocation()) return NULL;
    int fresh=p==NULL;
    int i=p ? allocation_slot(p) : allocation_slot(NULL); assert(i>=0);
    void *next=realloc(p,size);
    if(next) { if(fresh) ++live_allocations; allocations[i]=next; }
    return next;
}
static void test_free(void *p) {
    if(p) {
        int i=allocation_slot(p); assert(i>=0);
        allocations[i]=NULL; --live_allocations;
    }
    free(p);
}
static void test_unload_font(Font font) {
    if(font.texture.id==GetFontDefault().texture.id) ++default_unloads;
    else if(font.texture.id) ++custom_unloads;
    UnloadFont(font);
}
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define UnloadFont test_unload_font
#include "../vraylib/bridge.c"

static VR call(int id,VR *args,size_t count) {
    VyneRayResult result=vyne_ray_call(id,args,count); assert(!result.error); return result.value;
}
static VyneRayResult load_font(int64_t session,const char *path) {
    VR items[]={ {.kind=VR_TEXT,.text=path}, {.kind=VR_INT,.integer=24} };
    VR args[]={ {.kind=VR_INT,.integer=session}, {.kind=VR_TEXT,.text="font.load_size"},
                {.kind=VR_LIST,.items=items,.count=2} };
    return vyne_ray_call(-27,args,3);
}
int main(int argc,char **argv) {
    assert(argc==3);
    VR root=vr_null(); root.kind=VR_TEXT; root.text="";
    int64_t session=call(-15,&root,1).integer;
    VR window[]={vr_int(session),vr_int(160),vr_int(80),root,vr_int(0),vr_int(FLAG_WINDOW_HIDDEN)};
    call(-25,window,6);
    Font fallback=GetFontDefault();
    // Fill the registry so font ownership registration must also grow it.
    while(vr_capacity>0) {
        size_t used=0; for(size_t i=0;i<vr_capacity;++i) used+=vr_slots[i].data!=NULL;
        if(used==vr_capacity) break;
        VR args[]={root,vr_int(1)}; args[0].text="int";
        call(VR_BUFFER,args,2);
    }
    int baseline=live_allocations;
    for(int fail=0;fail<9;++fail) {
        remaining=fail;
        VyneRayResult result=load_font(session,argv[1]);
        remaining=-1;
        assert(result.error && live_allocations==baseline && !vr_temps);
        assert(default_unloads==0 && custom_unloads==0);
    }
    // A real, existing unsupported file triggers raylib's default fallback.
    VyneRayResult bad=load_font(session,argv[2]);
    assert(bad.error && live_allocations==baseline && !vr_temps);
    assert(default_unloads==0 && custom_unloads==0);
    Font after=GetFontDefault();
    assert(after.texture.id==fallback.texture.id && after.glyphs==fallback.glyphs);
    assert(MeasureTextEx(after,"default font still alive",16,1).x>0);
    VyneRayResult good=load_font(session,argv[1]); assert(!good.error);
    Font *font=vr_checked(good.value,"Font",1); assert(IsFontValid(*font));
    assert(load_font(session,argv[1]).value.integer==good.value.integer);
    VR owner=vr_int(session); call(-18,&owner,1);
    assert(custom_unloads==1 && default_unloads==0);
    call(VR_SHUTDOWN,NULL,0);
    assert(!vr_temps && live_allocations==1); // Reusable handle registry only.
    puts("VRAYLIB_FONT_FAILURE_OK");
}
