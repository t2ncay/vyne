#define VPLOT_TESTING 1
#include "../../../../vyne/runtime/vplot/vplot_core.h"
#include <assert.h>

static unsigned checks;
#define CHECK(condition) do { ++checks; if(!(condition)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition);abort();} } while(0)
typedef struct {unsigned writes;int width,height;uint32_t pixels[320*240];} Recording;
static void record_pixel(void* ctx,int x,int y,uint32_t color){Recording* r=(Recording*)ctx;CHECK(x>=0&&y>=0&&x<r->width&&y<r->height);r->pixels[y*r->width+x]=color;++r->writes;}
static int count_color(int64_t surface,uint32_t color){VPSurface* s=(VPSurface*)vp_object(surface,VP_OBJECT_SURFACE);VPRaster* r=(VPRaster*)s->canvas.context;int count=0;
    for(int i=0;i<r->width*r->height;++i)if((((uint32_t)r->pixels[i*3]<<16)|((uint32_t)r->pixels[i*3+1]<<8)|r->pixels[i*3+2])==color)++count;return count;}

int main(void){
    CHECK(vp_create_plot(NULL,"x","y")==0);CHECK(vp_raster_surface(0,240,NULL,3)==0);
    CHECK(vp_raster_surface(INT_MAX,240,NULL,3)==0);CHECK(vp_stats.live==0);
    for(int stage=0;stage<3;++stage){vp_fail_allocation_after=stage;int64_t image=vp_raster_surface(100,100,NULL,3);CHECK(image==0);CHECK(vp_stats.live==0);}
    vp_fail_allocation_after=-1;
    int64_t p=vp_create_plot("Chart","X","Y"),image=vp_raster_surface(320,240,NULL,3);
    CHECK(p&&image);CHECK(vp_render(p,image));CHECK(count_color(image,0x0F172A)>0);
    VPSurface* surface=(VPSurface*)vp_object(image,VP_OBJECT_SURFACE);CHECK(surface->frames==1);CHECK(vp_render(p,image));CHECK(surface->frames==1);
    CHECK(vp_limits(p,0,10,-1,1));CHECK(!vp_limits(p,NAN,10,-1,1));CHECK(!vp_limits(p,0,0,-1,1));CHECK(!vp_limits(p,-DBL_MAX,DBL_MAX,-1,1));
    int64_t series=vp_add_series(p,8,"Line",0x38BDF8,VP_LINE);CHECK(series);
    CHECK(vp_append(series,-10,0));CHECK(vp_append(series,20,0));CHECK(vp_render(p,image));CHECK(count_color(image,0x38BDF8)>150);
    VPSeries* s=(VPSeries*)vp_object(series,VP_OBJECT_SERIES);CHECK(s->count==2);
    CHECK(vp_clear(series));CHECK(vp_append(series,5,0));CHECK(vp_render(p,image));CHECK(count_color(image,0x38BDF8)>0);
    CHECK(vp_clear(series));CHECK(vp_append(series,0,0));CHECK(vp_append(series,NAN,NAN));CHECK(vp_append(series,10,0));CHECK(vp_render(p,image));CHECK(count_color(image,0x38BDF8)<50);
    for(int i=0;i<100;++i)CHECK(vp_append(series,i,i));CHECK(s->count==8);double x,y;vp_point(s,0,&x,&y);CHECK(x==92&&y==92);vp_point(s,7,&x,&y);CHECK(x==99);
    double bx[]={0,1,2},by[]={1,2,3};CHECK(vp_borrow(series,bx,by,3));by[1]=42;vp_point(s,1,&x,&y);CHECK(y==42);CHECK(!vp_append(series,3,4));CHECK(vp_clear(series));CHECK(!s->borrowed);
    CHECK(!vp_autoscale(p,-1,0,0));CHECK(vp_autoscale(p,0,.25,5));CHECK(vp_append(series,20,2));CHECK(vp_prepare((VPPlot*)vp_object(p,VP_OBJECT_PLOT)));
    VPPlot* plot=(VPPlot*)vp_object(p,VP_OBJECT_PLOT);CHECK(plot->xmin==15&&plot->xmax==20);CHECK(plot->ymax>plot->ymin);
    CHECK(vp_clear(series));CHECK(vp_append(series,0,1000));CHECK(vp_append(series,100,1));CHECK(vp_prepare(plot));CHECK(plot->xmin==95&&plot->ymax<5);
    size_t allocation_count=vp_stats.allocations,bytes=vp_stats.live;
    for(int i=0;i<500;++i){CHECK(vp_append(series,20+i,sin(i*.1)));CHECK(vp_render(p,image));}
    CHECK(vp_stats.allocations==allocation_count);CHECK(vp_stats.live==bytes);
    CHECK(!vp_render_at(p,image,(VPRect){-1,0,200,150}));CHECK(!vp_render_at(p,image,(VPRect){0,0,31,31}));
    CHECK(vp_destroy_plot(p));CHECK(!vp_object(series,VP_OBJECT_SERIES));CHECK(!vp_destroy_plot(p));CHECK(vp_destroy_surface(image));CHECK(vp_stats.live==0);
    int64_t reused=vp_create_plot("reuse","","" );CHECK(reused!=p);CHECK(!vp_object(p,VP_OBJECT_PLOT));CHECK(vp_destroy_plot(reused));

    // All chart kinds, signed bars, heatmap NaNs, palette endpoints and overlays.
    for(int kind=0;kind<4;++kind){p=vp_create_plot("Kinds","x","y");image=vp_raster_surface(320,240,NULL,3);series=vp_add_series(p,12,"Data",0xFF0000,kind);CHECK(series);
        s=(VPSeries*)vp_object(series,VP_OBJECT_SERIES);plot=s->owner;
        if(kind==VP_HEATMAP){s->cols=4;s->rows=3;s->count=12;for(int i=0;i<12;++i)s->ys[i]=i-5;s->ys[3]=NAN;}
        else for(int i=0;i<12;++i)CHECK(vp_append(series,i,(i%3)-1));
        CHECK(vp_render(p,image));if(kind!=VP_HEATMAP)CHECK(count_color(image,0xFF0000)>0);
        plot->light=1;++plot->revision;CHECK(vp_render(p,image));CHECK(count_color(image,0xF8FAFC)>0);
        CHECK(vp_destroy_plot(p));CHECK(vp_destroy_surface(image));CHECK(vp_stats.live==0);
    }
    CHECK(vp_heat_color(0,0)==0x440154);CHECK(vp_heat_color(1,0)==0xFDE725);CHECK(vp_heat_color(.5,1)==0xF8FAFC);
    Recording record={0};record.width=320;record.height=240;
    p=vp_create_plot("Adapter","","" );image=vp_custom_surface(320,240,&record,record_pixel,NULL,NULL,1);CHECK(image);CHECK(vp_render(p,image));CHECK(record.writes>0);
    unsigned before=record.writes;CHECK(vp_render(p,image));CHECK(record.writes==before);
    plot=(VPPlot*)vp_object(p,VP_OBJECT_PLOT);plot->transparent=1;++plot->revision;CHECK(vp_render_at(p,image,(VPRect){50,40,180,150}));
    CHECK(vp_destroy_plot(p));CHECK(vp_destroy_surface(image));CHECK(vp_stats.live==0);

    // Min/max reduction must retain an isolated spike among thousands of samples.
    p=vp_create_plot("Spike","","" );image=vp_raster_surface(320,240,NULL,3);series=vp_add_series(p,4096,"Spike",0xFFFF00,VP_LINE);
    for(int i=0;i<4096;++i)CHECK(vp_append(series,i,i==2001?1:0));CHECK(vp_limits(p,0,4095,0,1));CHECK(vp_render(p,image));
    plot=(VPPlot*)vp_object(p,VP_OBJECT_PLOT);VPRect r=vp_data_rect(plot,(VPRect){0,0,320,240});VPRaster* raster=(VPRaster*)((VPSurface*)vp_object(image,VP_OBJECT_SURFACE))->canvas.context;
    int found=0;for(int yy=r.y;yy<r.y+5;++yy)for(int xx=r.x;xx<r.x+r.w;++xx){size_t n=((size_t)yy*320+xx)*3;if(raster->pixels[n]==255&&raster->pixels[n+1]==255&&raster->pixels[n+2]==0)found=1;}CHECK(found);
    CHECK(vp_destroy_plot(p));CHECK(vp_destroy_surface(image));CHECK(vp_stats.live==0);
    // Empty/non-finite/constant data, responsive bounds, many legends, options,
    // clipping at viewport edges and sustained object lifecycle reuse.
    const int widths[]={32,96,128,200,320,1024};
    for(int size=0;size<6;++size){int width=widths[size],height=size==0?32:size==2?64:size==5?768:240;
        p=vp_create_plot("Responsive chart with a long title","Horizontal unit","Vertical unit");image=vp_raster_surface(width,height,NULL,3);
        CHECK(p&&image);
        for(int i=0;i<VPLOT_MAX_SERIES;++i){series=vp_add_series(p,3,"Long legend name",0x123456+i,VP_SCATTER);CHECK(series);
            CHECK(vp_append(series,0,0));CHECK(vp_append(series,NAN,INFINITY));CHECK(vp_append(series,0,0));}
        CHECK(!vp_add_series(p,1,"overflow",0,VP_LINE));CHECK(vp_last_error==VP_CAPACITY);CHECK(vp_render(p,image));
        plot=(VPPlot*)vp_object(p,VP_OBJECT_PLOT);CHECK(plot->xmax>plot->xmin&&plot->ymax>plot->ymin);
        plot->legend=0;plot->grid=0;plot->thickness=7;plot->marker=8;plot->light=1;++plot->revision;CHECK(vp_render(p,image));
        CHECK(vp_destroy_plot(p));CHECK(vp_destroy_surface(image));CHECK(vp_stats.live==0);
    }
    for(int i=0;i<1000;++i){p=vp_create_plot("Lifecycle","","" );series=vp_add_series(p,1,"S",0,VP_LINE);CHECK(series);CHECK(vp_append(series,i,i));CHECK(vp_destroy_plot(p));CHECK(!vp_object(series,VP_OBJECT_SERIES));}
    CHECK(vp_stats.live==0);
    p=vp_create_plot("Hysteresis","","" );series=vp_add_series(p,3,"View",0xFF0000,VP_LINE);
    double hx[]={0,1,2},hy[]={1,2,3};CHECK(vp_borrow(series,hx,hy,3));CHECK(vp_autoscale(p,0,.5,0));plot=(VPPlot*)vp_object(p,VP_OBJECT_PLOT);
    CHECK(vp_prepare(plot));hy[0]=1.1;hy[2]=2.9;CHECK(vp_prepare(plot));CHECK(plot->ymin==1&&plot->ymax==3);
    hy[2]=10;CHECK(vp_prepare(plot));CHECK(plot->ymax>=10);CHECK(vp_destroy_plot(p));CHECK(vp_stats.live==0);
    printf("VPLOT_CORE_PASS %u checks\n",checks);return 0;
}
