#include <stdint.h>
static uint32_t test_clock=1000;
#define VPLOT_CLOCK_MS() test_clock
#define VPLOT_TESTING 1
#include "../../../../vyne/runtime/modules/vplot_export.h"
#include <assert.h>

static int tries;
static size_t allowed;
static int64_t fallback(size_t budget){++tries;if(budget>allowed)return vp_fail(VP_MEMORY);return vp_raster_surface(32,32,NULL,3);}
static int64_t wrong_configuration(size_t budget){(void)budget;++tries;return vp_fail(VP_CONFIGURATION);}
int main(void){
    for(int i=0;i<4;++i){allowed=(size_t[]){16384,8192,4096,0}[i];tries=0;int64_t s=vp_auto_surface(fallback);
        assert(s&&tries==i+1);assert(vp_destroy_surface(s));assert(vp_stats.live==0);}
    tries=0;assert(!vp_auto_surface(wrong_configuration)&&tries==1);
    VyneArray_f64 x=vyne_array_f64_create(3),y=vyne_array_f64_create(3);
    for(int i=0;i<3;++i){x.data[i]=i;y.data[i]=i%2;}
    VyneValue xs=vyne_array_f64_to_value(&x),ys=vyne_array_f64_to_value(&y);
    VyneValue plot=vplot_chart(vyne_string_static("Managed"));assert(plot.as.i64);
    VPPlot* p=(VPPlot*)vp_object(plot.as.i64,VP_OBJECT_PLOT);assert(p->thickness==2&&!p->legend);
    assert(vplot_add_line(plot,xs,ys,vyne_string_static("First")).as.i64);assert(!p->legend);
    assert(vplot_add_line(plot,xs,ys,vyne_string_static("Second")).as.i64);assert(p->legend);
    assert(p->series[0]->color!=p->series[1]->color);
    assert(vyne_is_truthy(vplot_legend(plot,vyne_bool(false))));
    assert(vplot_add_line(plot,xs,ys,vyne_string_static("Third")).as.i64);assert(!p->legend);
    assert(vyne_is_truthy(vplot_show(plot)));int64_t old=p->managed_surface;
    size_t allocations=vp_stats.allocations,bytes=vp_stats.live;
    assert(vyne_is_truthy(vplot_show(plot)));assert(vp_stats.allocations==allocations&&vp_stats.live==bytes);
    assert(vp_destroy_surface(old));assert(vyne_is_truthy(vplot_show(plot)));assert(p->managed_surface!=old);
    size_t arena=g_arena.total_allocated;
    assert(vyne_is_truthy(vplot_save(plot,vyne_string_static("build/managed/chart.png"))));
    assert(g_arena.total_allocated==arena);assert(vyne_is_truthy(vplot_destroy(plot)));assert(vp_stats.live==0);
    assert(vyne_is_truthy(vplot_save_line(xs,ys,vyne_string_static("Quick"),vyne_string_static("build/managed/line.png"))));assert(vp_stats.live==0);
    assert(!vyne_is_truthy(vplot_save_line(xs,vyne_null(),vyne_string_static("Bad"),vyne_string_static("build/managed/bad.png"))));assert(vp_stats.live==0&&vp_last_error==VP_ARGUMENT);
    for(int failure=0;failure<6;++failure){vp_fail_allocation_after=failure;
        assert(vplot_live_chart(vyne_string_static("Failure")).as.i64==0);assert(vp_stats.live==0);}
    vp_fail_allocation_after=-1;
    plot=vplot_live_chart(vyne_string_static("Live"));assert(plot.as.i64);p=(VPPlot*)vp_object(plot.as.i64,VP_OBJECT_PLOT);
    assert(((VPSurface*)vp_object(p->managed_surface,VP_OBJECT_SURFACE))->frames==1);
    allocations=vp_stats.allocations;bytes=vp_stats.live;arena=g_arena.total_allocated;
    test_clock=1049;assert(vyne_is_truthy(vplot_push(plot,vyne_float(1))));
    assert(fabs(vplot_sample_x(vyne_int(p->managed_series),vyne_int(0)).as.f64-.049)<1e-9);
    assert(vplot_frame_count(vyne_int(p->managed_surface)).as.i64==1);
    test_clock=1100;assert(vyne_is_truthy(vplot_push(plot,vyne_float(2))));assert(vplot_frame_count(vyne_int(p->managed_surface)).as.i64==2);
    for(int i=0;i<1000;++i){test_clock+=20;assert(vyne_is_truthy(vplot_push(plot,vyne_float(i%10))));}
    assert(vplot_live_count(plot).as.i64==512);assert(vp_stats.allocations==allocations&&vp_stats.live==bytes&&g_arena.total_allocated==arena);
    VyneValue second=vplot_add_live_series(plot,vyne_string_static("Second"));assert(second.as.i64);
    test_clock+=20;assert(vyne_is_truthy(vplot_push(second,vyne_float(3))));assert(vplot_series_count(second).as.i64==1);
    assert(vyne_is_truthy(vplot_destroy(plot)));assert(!vyne_is_truthy(vplot_push(plot,vyne_float(0))));assert(vp_stats.live==0);
    test_clock=UINT32_MAX-50;plot=vplot_live_chart(vyne_string_static("Wrap"));assert(plot.as.i64);
    test_clock=25;assert(vyne_is_truthy(vplot_push(plot,vyne_float(1))));p=(VPPlot*)vp_object(plot.as.i64,VP_OBJECT_PLOT);
    assert(fabs(vplot_sample_x(vyne_int(p->managed_series),vyne_int(0)).as.f64-.076)<1e-9);
    assert(vplot_frame_count(vyne_int(p->managed_surface)).as.i64==1);
    test_clock=50;assert(vyne_is_truthy(vplot_push(plot,vyne_float(2))));assert(vplot_frame_count(vyne_int(p->managed_surface)).as.i64==2);
    assert(vyne_is_truthy(vplot_destroy(plot)));assert(vp_stats.live==0);
    arena_free_all();puts("VPLOT_MANAGED_PASS defaults, ownership, fallback, save, clocks, throttling, wrapping and no update allocations");return 0;
}
