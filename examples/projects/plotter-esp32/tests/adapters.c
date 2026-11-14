#include "../../../../vyne/runtime/vplot/vplot_core.h"
#include <assert.h>

typedef struct {uint16_t output[320*320],band[320*320];int x,y,width,rows,band_height;} Band;
static uint16_t rgb565(uint32_t c){return (uint16_t)(((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31));}
static void band_pixel(void* context,int x,int y,uint32_t c){Band* b=(Band*)context;assert(x>=b->x&&x<b->x+b->width&&y>=b->y&&y<b->y+b->rows);b->band[(y-b->y)*b->width+x-b->x]=rgb565(c);}
static int draw_bands(VPPlot* p,VPSurface* surface,VPRect rect){Band* b=(Band*)surface->canvas.context;
    for(int y=rect.y;y<rect.y+rect.h;y+=b->band_height){b->x=rect.x;b->y=y;b->width=rect.w;b->rows=rect.y+rect.h-y;if(b->rows>b->band_height)b->rows=b->band_height;
        VPCanvas c=surface->canvas;c.clip=(VPRect){rect.x,y,rect.w,b->rows};vp_render_scene(p,&c,rect);
        for(int row=0;row<b->rows;++row)memcpy(b->output+(y+row)*surface->width+rect.x,b->band+row*rect.w,(size_t)rect.w*2);
    }return 1;
}
int main(void){
    unsigned scenarios=0;
    for(int portrait=0;portrait<2;++portrait)for(int kind=0;kind<4;++kind)for(int light=0;light<2;++light)for(int band=0;band<4;++band){
        int width=portrait?240:320,height=portrait?320:240;
        Band* b=(Band*)calloc(1,sizeof(Band));assert(b);b->band_height=(int[]){1,3,17,64}[band];
        int64_t p=vp_create_plot("Surface parity","Axis X","Axis Y"),image=vp_raster_surface(width,height,NULL,3);
        int64_t display=vp_custom_surface(width,height,b,band_pixel,NULL,NULL,0);assert(display);
        VPSurface* d=(VPSurface*)vp_object(display,VP_OBJECT_SURFACE);d->draw=draw_bands;
        int64_t id=vp_add_series(p,kind==VP_LINE?2048:20,"Values",0x38BDF8,kind);assert(id);
        VPSeries* s=(VPSeries*)vp_object(id,VP_OBJECT_SERIES);s->owner->light=light;
        if(kind==VP_HEATMAP){s->cols=5;s->rows=4;s->count=20;for(int i=0;i<20;++i)s->ys[i]=(i%7)-3;s->ys[4]=NAN;}
        else for(int i=0;i<(kind==VP_LINE?2048:20);++i)assert(vp_append(id,i,i==1313?4:sin(i*.04)));
        size_t allocations=vp_stats.allocations;
        for(int viewport=0;viewport<2;++viewport){VPRect rect=viewport?(VPRect){17,19,201,159}:(VPRect){0,0,width,height};
            assert(vp_render_at(p,image,rect));assert(vp_render_at(p,display,rect));
            VPRaster* r=(VPRaster*)((VPSurface*)vp_object(image,VP_OBJECT_SURFACE))->canvas.context;
            for(int i=0;i<width*height;++i){uint32_t c=((uint32_t)r->pixels[i*3]<<16)|((uint32_t)r->pixels[i*3+1]<<8)|r->pixels[i*3+2];
                if(b->output[i]!=rgb565(c)){fprintf(stderr,"parity: portrait=%d kind=%d light=%d band=%d viewport=%d x=%d y=%d actual=%x expected=%x\n",portrait,kind,light,b->band_height,viewport,i%width,i/width,b->output[i],rgb565(c));abort();}}
            ++scenarios;
        }
        assert(vp_stats.allocations==allocations);
        s->owner->transparent=1;++s->owner->revision;assert(!vp_render(p,display));assert(vp_last_error==VP_CAPABILITY);
        assert(vp_destroy_surface(display));assert(vp_destroy_surface(image));assert(vp_destroy_plot(p));free(b);assert(vp_stats.live==0);
    }
    printf("VPLOT_ADAPTERS_PASS %u pixel-equivalent band/viewport/chart/theme scenarios\n",scenarios);return 0;
}
