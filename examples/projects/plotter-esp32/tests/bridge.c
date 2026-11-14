#define VPLOT_TESTING 1
#include "../../../../vyne/runtime/modules/vplot.h"
#include <assert.h>

#define I(n) vyne_int(n)
#define F(n) vyne_float(n)
#define S(n) vyne_string_static(n)
static int truth(VyneValue value){return vyne_is_truthy(value);}
int main(void){
    assert(vplot_new_plot(I(1),S("x"),S("y")).as.i64==0);
    assert(vplot_image_surface(I(-1),I(240)).as.i64==0);
    assert(vplot_rgb(I(-1),I(0),I(0)).as.i64==-1);
    assert(vplot_rgb(I(256),I(0),I(0)).as.i64==-1);
    assert(vplot_rgb(I(255),I(0),I(0)).as.i64==0xFF0000);
    char too_long[VPLOT_TEXT_SIZE+1];memset(too_long,'x',sizeof(too_long)-1);too_long[sizeof(too_long)-1]=0;
    assert(vplot_new_plot(S(too_long),S(""),S("")).as.i64==0);
    VyneValue plot=vplot_new_plot(S("Boundary"),S("X"),S("Y"));assert(plot.as.i64);
    for(int failure=0;failure<2;++failure){size_t bytes=vp_stats.live;vp_fail_allocation_after=failure;
        assert(vplot_live_series(plot,I(8),S("fail"),I(0)).as.i64==0);assert(vp_stats.live==bytes);}
    vp_fail_allocation_after=-1;
    VyneArray_f64 data=vyne_array_f64_create(5);for(int i=0;i<5;++i)data.data[i]=i;
    VyneValue array=vyne_array_f64_to_value(&data);
    VyneValue hist=vplot_histogram(plot,array,I(4),S("Histogram"),I(0xFF0000));assert(hist.as.i64);
    assert(vplot_sample_y(hist,I(0)).as.f64==1);assert(vplot_sample_y(hist,I(3)).as.f64==2);
    size_t allocs=vp_stats.allocations;
    for(int i=0;i<1000;++i)assert(truth(vplot_histogram_update(hist,array)));
    assert(vp_stats.allocations==allocs);
    VyneValue live=vplot_live_series(plot,I(4),S("live"),I(0x00FF00));assert(live.as.i64);
    VyneArray_f64 malformed=data;malformed.cap=1;VyneValue malformed_value=vyne_array_f64_to_value(&malformed);
    assert(!truth(vplot_set_data(live,malformed_value,malformed_value)));
    assert(!truth(vplot_set_data(live,array,array)));assert(vp_last_error==VP_CAPACITY);
    VyneArray_i64 integers=vyne_array_i64_create(3);for(int i=0;i<3;++i)integers.data[i]=i;
    VyneValue ints=vyne_array_i64_to_value(&integers);assert(truth(vplot_set_data(live,ints,ints)));
    assert(vplot_sample_y(live,I(2)).as.f64==2);
    assert(!truth(vplot_append(live,S("bad"),F(1))));
    VyneArray_f32 floats=vyne_array_f32_create(3);for(int i=0;i<3;++i)floats.data[i]=(float)(i*.25);
    VyneValue f32=vyne_array_f32_to_value(&floats);assert(truth(vplot_set_data(live,f32,f32)));
    assert(vplot_sample_y(live,I(2)).as.f64==.5);
    VyneValue image=vplot_image_surface(I(320),I(240));assert(image.as.i64);
    assert(!truth(vplot_render(image,plot)));assert(truth(vplot_render(plot,image)));
    assert(!truth(vplot_theme(plot,I(2))));assert(!truth(vplot_marker_size(plot,I(9))));
    assert(!truth(vplot_autoscale(plot,F(.05),F(1),F(0))));assert(!truth(vplot_limits(plot,F(0),F(INFINITY),F(0),F(1))));
    assert(!truth(vplot_render_at(plot,image,I(INT64_MAX),I(0),I(200),I(200))));
    VyneValue pixels=vplot_image_pixels(image);assert(vp_b_length(pixels)==320*240*3);
    VyneValue bound=vplot_image_bind(pixels,I(320),I(240),I(3));assert(bound.as.i64);
    assert(truth(vplot_transparent(plot,vyne_bool(true))));assert(truth(vplot_render(plot,bound)));
    assert(truth(vplot_destroy(bound)));assert(vp_b_at(pixels,0)>=0);
    assert(vplot_image_bind(pixels,I(320),I(240),I(4)).as.i64==0);
    assert(vplot_image_pixels(plot).type==V_NULL);
    assert(truth(vplot_destroy(plot)));assert(!truth(vplot_destroy(plot)));assert(!truth(vplot_append(live,F(0),F(0))));
    assert(truth(vplot_destroy(image)));assert(vp_stats.live==0);
    // Borrowed grayscale and RGBA surfaces preserve caller-owned buffers.
    for(int channels=1;channels<=4;channels+=channels==1?2:1){
        VyneArray_f64 storage=vyne_array_f64_create(128*128*channels);VyneValue values=vyne_array_f64_to_value(&storage);
        plot=vplot_new_plot(S("Image"),S(""),S(""));image=vplot_image_bind(values,I(128),I(128),I(channels));assert(image.as.i64);
        assert(truth(vplot_render(plot,image)));assert(truth(vplot_destroy(image)));assert(truth(vplot_destroy(plot)));assert(storage.data[0]>0);
    }
    int64_t handles[VPLOT_MAX_HANDLES];for(int i=0;i<VPLOT_MAX_HANDLES;++i){handles[i]=vp_create_plot("pool","","" );assert(handles[i]);}
    size_t before=vp_stats.live;assert(vp_create_plot("overflow","","" )==0);assert(vp_last_error==VP_CAPACITY);assert(vp_stats.live==before);
    for(int i=0;i<VPLOT_MAX_HANDLES;++i)assert(vp_destroy_plot(handles[i]));
    assert(vp_stats.live==0);arena_free_all();puts("VPLOT_BRIDGE_PASS types, failures, histogram, updates, images, stale handles and pool limits");return 0;
}
