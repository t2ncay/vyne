// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
// Portable C11 plotting core. Platform adapters own surfaces, not chart math.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include "vplot_font.h"

#ifndef VPLOT_MAX_HANDLES
#define VPLOT_MAX_HANDLES 64
#endif
#ifndef VPLOT_MAX_SERIES
#define VPLOT_MAX_SERIES 8
#endif
#define VPLOT_MAX_DIMENSION 4096
#define VPLOT_MAX_POINTS 1048576
#define VPLOT_TEXT_SIZE 96

enum { VP_LINE, VP_SCATTER, VP_BAR, VP_HEATMAP };
enum { VP_OBJECT_PLOT=1, VP_OBJECT_SERIES, VP_OBJECT_SURFACE };
enum { VP_OK, VP_ARGUMENT, VP_MEMORY, VP_HANDLE, VP_CAPACITY, VP_RANGE, VP_CAPABILITY, VP_CONFIGURATION };
static int vp_last_error = VP_OK;
static inline int vp_fail(int error) { vp_last_error=error; return 0; }
static inline const char* vp_error_text(void) {
    static const char* text[] = {"ok","invalid plotting argument","plot allocation failed",
        "invalid or destroyed plot handle","plotting capacity exceeded","invalid axis range",
        "surface does not support this operation",
        "configure a display in hardware.toml (for HW-950, use --preset hw950)"};
    return text[vp_last_error];
}

typedef struct { size_t live, peak, allocations, frees; } VPStats;
static VPStats vp_stats;
typedef union { size_t bytes; long double alignment; void* pointer; } VPAllocation;
#ifdef VPLOT_TESTING
static long vp_fail_allocation_after = -1;
#endif
static inline void* vp_alloc(size_t bytes) {
    if (bytes > SIZE_MAX-sizeof(VPAllocation)) { vp_fail(VP_MEMORY); return NULL; }
#ifdef VPLOT_TESTING
    if (vp_fail_allocation_after == 0) { vp_fail(VP_MEMORY); return NULL; }
    if (vp_fail_allocation_after > 0) --vp_fail_allocation_after;
#endif
    VPAllocation* block=(VPAllocation*)malloc(sizeof(VPAllocation)+bytes);
    if (!block) { vp_fail(VP_MEMORY); return NULL; }
    block->bytes=bytes;
    vp_stats.live+=bytes; ++vp_stats.allocations;
    if (vp_stats.live>vp_stats.peak) vp_stats.peak=vp_stats.live;
    memset(block+1,0,bytes);
    return block+1;
}
static inline void vp_free(void* pointer) {
    if (!pointer) return;
    VPAllocation* block=(VPAllocation*)pointer-1;
    vp_stats.live-=block->bytes; ++vp_stats.frees; free(block);
}

typedef struct { int x,y,w,h; } VPRect;
typedef struct {
    void* context;
    void (*pixel)(void*,int,int,uint32_t);
    void (*span)(void*,int,int,int,uint32_t);
    VPRect clip;
    VPRect geometry; // logical clipping frame, independent of an output band
} VPCanvas;
typedef struct VPPlot VPPlot;
typedef struct VPSeries VPSeries;
typedef struct VPSurface VPSurface;
struct VPSeries {
    VPPlot* owner;
    int64_t handle;
    int kind,capacity,count,head,borrowed,monotonic;
    int cols,rows;
    double *storage,*xs,*ys;
    double bar_width;
    uint32_t color;
    char label[VPLOT_TEXT_SIZE];
};
struct VPPlot {
    int64_t handle;
    VPSeries* series[VPLOT_MAX_SERIES];
    int count,light,grid,minor,legend,transparent,decimate,fixed,have_bounds;
    int thickness,marker;
    uint64_t revision;
    double xmin,xmax,ymin,ymax,padding,hysteresis,window;
    char title[VPLOT_TEXT_SIZE],xlabel[VPLOT_TEXT_SIZE],ylabel[VPLOT_TEXT_SIZE];
    int64_t managed_surface,managed_series;
    uint32_t last_sample_ms,last_show_ms;
    uint64_t elapsed_ms;
    int managed_live,has_shown,automatic_legend;
};
struct VPSurface {
    int64_t handle,last_plot;
    int width,height,preserve,volatile_pixels;
    uint64_t last_revision,frames;
    VPRect last_rect;
    VPCanvas canvas;
    void (*dispose)(void*);
    void (*refresh)(VPSurface*);
    int (*draw)(VPPlot*,VPSurface*,VPRect);
    int raster; // 1 = owned RGB888, 2 = borrowed HWC Float64
};
typedef struct { void* object; uint32_t generation; int kind; } VPSlot;
static VPSlot vp_slots[VPLOT_MAX_HANDLES];

static inline int64_t vp_register(void* object,int kind) {
    for (int i=0;i<VPLOT_MAX_HANDLES;++i) if (!vp_slots[i].object) {
        if (++vp_slots[i].generation == 0) ++vp_slots[i].generation;
        vp_slots[i].object=object; vp_slots[i].kind=kind;
        return ((int64_t)vp_slots[i].generation<<16)|(i+1);
    }
    vp_fail(VP_CAPACITY); return 0;
}
static inline void* vp_peek(int64_t handle,int kind) {
    unsigned slot=(unsigned)((uint64_t)handle&65535);
    uint64_t generation=(uint64_t)handle>>16;
    if (handle<=0 || !slot || slot>VPLOT_MAX_HANDLES ||
        !vp_slots[slot-1].object || vp_slots[slot-1].kind!=kind ||
        generation!=vp_slots[slot-1].generation) return NULL;
    return vp_slots[slot-1].object;
}
static inline void* vp_object(int64_t handle,int kind) {
    void* object=vp_peek(handle,kind);if(!object)vp_fail(VP_HANDLE);return object;
}
static inline void vp_unregister(int64_t handle) {
    unsigned slot=(unsigned)((uint64_t)handle&65535);
    if (slot && slot<=VPLOT_MAX_HANDLES) vp_slots[slot-1].object=NULL;
}
static inline int vp_copy_text(char* destination,const char* source) {
    if (!source || strlen(source)>=VPLOT_TEXT_SIZE) return vp_fail(VP_ARGUMENT);
    strcpy(destination,source); return 1;
}
static inline VPRect vp_intersection(VPRect a,VPRect b) {
    int x=a.x>b.x?a.x:b.x,y=a.y>b.y?a.y:b.y;
    int right=a.x+a.w<b.x+b.w?a.x+a.w:b.x+b.w;
    int bottom=a.y+a.h<b.y+b.h?a.y+a.h:b.y+b.h;
    VPRect result={x,y,right>x?right-x:0,bottom>y?bottom-y:0}; return result;
}
static inline void vp_pixel(VPCanvas* canvas,int x,int y,uint32_t color) {
    VPRect r=canvas->clip;
    if (x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h && canvas->pixel)
        canvas->pixel(canvas->context,x,y,color);
}
static inline void vp_span(VPCanvas* canvas,int x,int y,int width,uint32_t color) {
    VPRect r=canvas->clip;
    if (width<=0 || y<r.y || y>=r.y+r.h) return;
    int end=x+width;
    if (x<r.x) x=r.x;
    if (end>r.x+r.w) end=r.x+r.w;
    if (end<=x) return;
    if (canvas->span) canvas->span(canvas->context,x,y,end-x,color);
    else for (;x<end;++x) vp_pixel(canvas,x,y,color);
}
static inline void vp_fill(VPCanvas* canvas,VPRect r,uint32_t color) {
    r=vp_intersection(r,canvas->clip);
    for (int y=r.y;y<r.y+r.h;++y) vp_span(canvas,r.x,y,r.w,color);
}
static inline void vp_text(VPCanvas* c,int x,int y,const char* text,int scale,uint32_t color) {
    for (size_t i=0;text[i] && i<VPLOT_TEXT_SIZE;++i) {
        const uint8_t* glyph=vp_glyph((unsigned char)text[i]);
        for (int row=0;row<7;++row) for (int col=0;col<5;++col) if (glyph[row]&(16>>col)) {
            VPRect r={x+(int)i*6*scale+col*scale,y+row*scale,scale,scale}; vp_fill(c,r,color);
        }
    }
}
static inline void vp_vertical_text(VPCanvas* c,int x,int y,const char* text,uint32_t color) {
    int length=(int)strlen(text)*6;
    for (int i=0;text[i] && i<VPLOT_TEXT_SIZE;++i) {
        const uint8_t* glyph=vp_glyph((unsigned char)text[i]);
        for (int row=0;row<7;++row) for (int col=0;col<5;++col)
            if (glyph[row]&(16>>col)) vp_pixel(c,x+row,y+length-1-i*6-col,color);
    }
}
static inline int vp_outcode(double x,double y,VPRect r) {
    return (x<r.x?1:x>r.x+r.w-1?2:0)|(y<r.y?4:y>r.y+r.h-1?8:0);
}
static inline int vp_clip_line(VPRect r,double* x0,double* y0,double* x1,double* y1) {
    if (r.w<=0 || r.h<=0 || !isfinite(*x0) || !isfinite(*y0) || !isfinite(*x1) || !isfinite(*y1)) return 0;
    for (int iteration=0;iteration<12;++iteration) {
        int a=vp_outcode(*x0,*y0,r),b=vp_outcode(*x1,*y1,r);
        if (!(a|b)) return 1;
        if (a&b) return 0;
        int out=a?a:b; double x=0,y=0,dx=*x1-*x0,dy=*y1-*y0;
        if (!isfinite(dx)||!isfinite(dy)) return 0;
        if (out&8) { y=r.y+r.h-1; if (dy==0) return 0; x=*x0+dx*((y-*y0)/dy); }
        else if (out&4) { y=r.y; if (dy==0) return 0; x=*x0+dx*((y-*y0)/dy); }
        else if (out&2) { x=r.x+r.w-1; if (dx==0) return 0; y=*y0+dy*((x-*x0)/dx); }
        else { x=r.x; if (dx==0) return 0; y=*y0+dy*((x-*x0)/dx); }
        if (!isfinite(x)||!isfinite(y)) return 0;
        if (out==a) { *x0=x;*y0=y; } else { *x1=x;*y1=y; }
    }
    return 0;
}
static inline void vp_line(VPCanvas* c,double x0,double y0,double x1,double y1,uint32_t color,int thickness) {
    VPRect logical=c->geometry.w>0&&c->geometry.h>0?c->geometry:c->clip;
    if (!vp_clip_line(logical,&x0,&y0,&x1,&y1)) return;
    int x=(int)lround(x0),y=(int)lround(y0),endx=(int)lround(x1),endy=(int)lround(y1);
    int dx=abs(endx-x),sx=x<endx?1:-1,dy=-abs(endy-y),sy=y<endy?1:-1,error=dx+dy;
    for (;;) {
        VPRect dot={x-thickness/2,y-thickness/2,thickness,thickness}; vp_fill(c,dot,color);
        if (x==endx&&y==endy) break;
        int twice=2*error;
        if (twice>=dy) { error+=dy;x+=sx; }
        if (twice<=dx) { error+=dx;y+=sy; }
    }
}

static inline int64_t vp_create_plot(const char* title,const char* xlabel,const char* ylabel) {
    if (!title||!xlabel||!ylabel||strlen(title)>=VPLOT_TEXT_SIZE||strlen(xlabel)>=VPLOT_TEXT_SIZE||strlen(ylabel)>=VPLOT_TEXT_SIZE)
        return vp_fail(VP_ARGUMENT);
    VPPlot* p=(VPPlot*)vp_alloc(sizeof(VPPlot)); if (!p) return 0;
    vp_copy_text(p->title,title);vp_copy_text(p->xlabel,xlabel);vp_copy_text(p->ylabel,ylabel);
    p->grid=1;p->minor=1;p->legend=1;p->decimate=1;p->thickness=1;p->marker=2;p->padding=.05;p->revision=1;
    p->handle=vp_register(p,VP_OBJECT_PLOT);
    if (!p->handle) { vp_free(p);return 0; }
    vp_last_error=VP_OK;return p->handle;
}
static inline void vp_release_series(VPSeries* series) {
    vp_unregister(series->handle);vp_free(series->storage);vp_free(series);
}
static inline int vp_destroy_surface(int64_t handle);
static inline int vp_destroy_plot(int64_t handle) {
    VPPlot* p=(VPPlot*)vp_object(handle,VP_OBJECT_PLOT);if(!p)return 0;
    for(int i=0;i<p->count;++i)vp_release_series(p->series[i]);
    if(vp_peek(p->managed_surface,VP_OBJECT_SURFACE))vp_destroy_surface(p->managed_surface);
    vp_unregister(handle);vp_free(p);vp_last_error=VP_OK;return 1;
}

// Reusable automatic buffering policy. Backends supply construction; failures
// unrelated to memory/capacity remain actionable rather than being hidden.
static inline int64_t vp_auto_surface(int64_t (*create)(size_t)) {
    static const size_t budgets[]={16384,8192,4096,0};
    for(unsigned i=0;i<sizeof(budgets)/sizeof(budgets[0]);++i){
        int64_t surface=create(budgets[i]);if(surface)return surface;
        if(vp_last_error!=VP_MEMORY&&vp_last_error!=VP_CAPACITY)return 0;
    }
    return 0;
}
static inline int64_t vp_add_series(int64_t plot,int capacity,const char* label,uint32_t color,int kind) {
    VPPlot* p=(VPPlot*)vp_object(plot,VP_OBJECT_PLOT);if(!p)return 0;
    if(capacity<1||capacity>VPLOT_MAX_POINTS||kind<VP_LINE||kind>VP_HEATMAP||color>0xFFFFFF||!label||strlen(label)>=VPLOT_TEXT_SIZE)
        return vp_fail(VP_ARGUMENT);
    if(p->count>=VPLOT_MAX_SERIES)return vp_fail(VP_CAPACITY);
    VPSeries* s=(VPSeries*)vp_alloc(sizeof(VPSeries));if(!s)return 0;
    size_t values=(size_t)capacity*(kind==VP_HEATMAP?1:2);
    s->storage=(double*)vp_alloc(values*sizeof(double));
    if(!s->storage){vp_free(s);return 0;}
    s->owner=p;s->kind=kind;s->capacity=capacity;s->color=color&0xFFFFFF;
    s->xs=kind==VP_HEATMAP?NULL:s->storage;s->ys=kind==VP_HEATMAP?s->storage:s->storage+capacity;
    s->monotonic=1;s->bar_width=.8;vp_copy_text(s->label,label);
    s->handle=vp_register(s,VP_OBJECT_SERIES);
    if(!s->handle){vp_free(s->storage);vp_free(s);return 0;}
    p->series[p->count++]=s;++p->revision;vp_last_error=VP_OK;return s->handle;
}
static inline int vp_append(int64_t handle,double x,double y) {
    VPSeries* s=(VPSeries*)vp_object(handle,VP_OBJECT_SERIES);if(!s)return 0;
    if(s->borrowed||s->kind==VP_HEATMAP)return vp_fail(VP_ARGUMENT);
    if(s->count) { int last=(s->head+s->count-1)%s->capacity;if(x<s->xs[last])s->monotonic=0; }
    int position=(s->head+s->count)%s->capacity;
    if(s->count==s->capacity){position=s->head;s->head=(s->head+1)%s->capacity;}else ++s->count;
    s->xs[position]=x;s->ys[position]=y;++s->owner->revision;vp_last_error=VP_OK;return 1;
}
static inline int vp_clear(int64_t handle) {
    VPSeries* s=(VPSeries*)vp_object(handle,VP_OBJECT_SERIES);if(!s)return 0;
    s->xs=s->kind==VP_HEATMAP?NULL:s->storage;s->ys=s->kind==VP_HEATMAP?s->storage:s->storage+s->capacity;
    s->borrowed=0;s->count=s->head=0;s->monotonic=1;++s->owner->revision;vp_last_error=VP_OK;return 1;
}
static inline void vp_point(const VPSeries* s,int index,double* x,double* y) {
    int position=s->borrowed?index:(s->head+index)%s->capacity;
    *x=s->xs[position];*y=s->ys[position];
}
static inline int vp_borrow(int64_t handle,const double* xs,const double* ys,int count) {
    VPSeries* s=(VPSeries*)vp_object(handle,VP_OBJECT_SERIES);if(!s)return 0;
    if(s->kind==VP_HEATMAP||count<0||count>VPLOT_MAX_POINTS||(count&&(!xs||!ys)))return vp_fail(VP_ARGUMENT);
    s->xs=(double*)xs;s->ys=(double*)ys;s->count=count;s->head=0;s->borrowed=1;s->monotonic=1;
    for(int i=1;i<count;++i)if(xs[i]<xs[i-1])s->monotonic=0;
    ++s->owner->revision;vp_last_error=VP_OK;return 1;
}
static inline int vp_limits(int64_t handle,double xmin,double xmax,double ymin,double ymax) {
    VPPlot* p=(VPPlot*)vp_object(handle,VP_OBJECT_PLOT);if(!p)return 0;
    if(!isfinite(xmin)||!isfinite(xmax)||!isfinite(ymin)||!isfinite(ymax)||xmin>=xmax||ymin>=ymax||
       !isfinite(xmax-xmin)||!isfinite(ymax-ymin))return vp_fail(VP_RANGE);
    p->xmin=xmin;p->xmax=xmax;p->ymin=ymin;p->ymax=ymax;p->fixed=1;p->have_bounds=1;++p->revision;vp_last_error=VP_OK;return 1;
}
static inline int vp_autoscale(int64_t handle,double padding,double hysteresis,double window) {
    VPPlot* p=(VPPlot*)vp_object(handle,VP_OBJECT_PLOT);if(!p)return 0;
    if(!isfinite(padding)||!isfinite(hysteresis)||!isfinite(window)||padding<0||padding>1||hysteresis<0||hysteresis>=1||window<0)
        return vp_fail(VP_ARGUMENT);
    p->fixed=0;p->have_bounds=0;p->padding=padding;p->hysteresis=hysteresis;p->window=window;++p->revision;vp_last_error=VP_OK;return 1;
}
static inline int vp_expand(double* lo,double* hi,double padding) {
    if(*lo==*hi) { double delta=fmax(fabs(*lo)*.05,.5);double a=*lo-delta,b=*hi+delta;
        if(isfinite(a))*lo=a;if(isfinite(b))*hi=b;
    } else { double delta=(*hi-*lo)*padding;double a=*lo-delta,b=*hi+delta;
        if(isfinite(a))*lo=a;if(isfinite(b))*hi=b;
    }
    return *hi>*lo&&isfinite(*hi-*lo);
}
static inline int vp_prepare(VPPlot* p) {
    if(p->fixed)return 1;
    double xmin=DBL_MAX,xmax=-DBL_MAX,ymin=DBL_MAX,ymax=-DBL_MAX;
    for(int k=0;k<p->count;++k) {
        VPSeries* s=p->series[k];
        if(s->kind==VP_HEATMAP&&s->count) {
            xmin=fmin(xmin,0);xmax=fmax(xmax,s->cols);ymin=fmin(ymin,0);ymax=fmax(ymax,s->rows);continue;
        }
        for(int i=0;i<s->count;++i){double x,y;vp_point(s,i,&x,&y);if(!isfinite(x)||!isfinite(y))continue;
            double half=s->kind==VP_BAR?s->bar_width/2:0;
            xmin=fmin(xmin,x-half);xmax=fmax(xmax,x+half);ymin=fmin(ymin,y);ymax=fmax(ymax,y);
        }
        if(s->kind==VP_BAR&&s->count){ymin=fmin(ymin,0);ymax=fmax(ymax,0);}
    }
    if(xmin==DBL_MAX){xmin=ymin=0;xmax=ymax=1;}
    if(p->window>0){
        xmin=xmax-p->window;if(xmin==xmax)return vp_fail(VP_RANGE);
        ymin=DBL_MAX;ymax=-DBL_MAX;
        for(int k=0;k<p->count;++k){VPSeries* s=p->series[k];int visible=0;
            if(s->kind==VP_HEATMAP&&s->count){ymin=fmin(ymin,0);ymax=fmax(ymax,s->rows);continue;}
            for(int i=0;i<s->count;++i){double x,y;vp_point(s,i,&x,&y);
                if(!isfinite(x)||!isfinite(y)||x<xmin||x>xmax)continue;
                ymin=fmin(ymin,y);ymax=fmax(ymax,y);visible=1;
            }
            if(s->kind==VP_BAR&&visible){ymin=fmin(ymin,0);ymax=fmax(ymax,0);}
        }
        if(ymin==DBL_MAX){ymin=0;ymax=1;}
    }
    else if(!vp_expand(&xmin,&xmax,p->padding))return vp_fail(VP_RANGE);
    if(!vp_expand(&ymin,&ymax,p->padding))return vp_fail(VP_RANGE);
    if(p->have_bounds&&p->hysteresis>0){
        double oldspan=p->ymax-p->ymin,newspan=ymax-ymin;
        if(ymin>=p->ymin&&ymax<=p->ymax&&newspan>=oldspan*(1-p->hysteresis)){ymin=p->ymin;ymax=p->ymax;}
    }
    p->xmin=xmin;p->xmax=xmax;p->ymin=ymin;p->ymax=ymax;p->have_bounds=1;return 1;
}
static inline double vp_nice_step(double span,int target) {
    double raw=span/(target>0?target:1);if(!isfinite(raw)||raw<=0)return 1;
    double magnitude=pow(10,floor(log10(raw)));if(magnitude==0||!isfinite(magnitude))return raw;
    double unit=raw/magnitude;
    return (unit<=1?1:unit<=2?2:unit<=2.5?2.5:unit<=5?5:10)*magnitude;
}
static inline void vp_number(char* buffer,size_t size,double value) {
    if(value==0)value=0;
    double a=fabs(value);
    if(a>=1e6&&a<1e9)snprintf(buffer,size,"%.3gM",value/1e6);
    else if(a>=1e3&&a<1e6)snprintf(buffer,size,"%.3gk",value/1e3);
    else snprintf(buffer,size,"%.3g",value);
}
static inline double vp_map(double value,double lo,double hi,int extent) { return (value-lo)/(hi-lo)*(extent-1); }
static inline uint32_t vp_mix(uint32_t a,uint32_t b,double t) {
    if(t<0)t=0;if(t>1)t=1;uint32_t color=0;
    for(int shift=0;shift<=16;shift+=8){int x=(a>>shift)&255,y=(b>>shift)&255;color|=(uint32_t)lround(x+(y-x)*t)<<shift;}
    return color;
}
static inline uint32_t vp_heat_color(double normalized,int diverging) {
    static const uint32_t viridis[]={0x440154,0x3B528B,0x21918C,0x5EC962,0xFDE725};
    if(normalized<0)normalized=0;if(normalized>1)normalized=1;
    if(diverging)return normalized<.5?vp_mix(0x2563EB,0xF8FAFC,normalized*2):vp_mix(0xF8FAFC,0xEF4444,(normalized-.5)*2);
    double scaled=normalized*4;int index=(int)scaled;if(index>=4)return viridis[4];
    return vp_mix(viridis[index],viridis[index+1],scaled-index);
}
static inline VPRect vp_data_rect(VPPlot* p,VPRect outer) {
    if(outer.w<160||outer.h<112){
        int left=outer.w>=96&&outer.h>=80?36:4,top=p->title[0]&&outer.h>=64?18:3;
        int bottom=outer.w>=96&&outer.h>=80?16:4;
        VPRect compact={outer.x+left,outer.y+top,outer.w-left-4,outer.h-top-bottom};return compact;
    }
    int heat=0;for(int i=0;i<p->count;++i)if(p->series[i]->kind==VP_HEATMAP)heat=1;
    int left=outer.w>=180?52:36,right=heat?54:12;
    int top=p->title[0]?30:10;
    if(p->legend&&p->count)top+=p->count>2?24:12;
    VPRect r={outer.x+left,outer.y+top,outer.w-left-right,outer.h-top-(outer.h>=120?30:20)};
    return r;
}
static inline void vp_marker(VPCanvas* c,double x,double y,int radius,uint32_t color) {
    if(!isfinite(x)||!isfinite(y)||x<c->clip.x-radius-.5||x>c->clip.x+c->clip.w+radius+.5||y<c->clip.y-radius-.5||y>c->clip.y+c->clip.h+radius+.5)return;
    int cx=(int)lround(x),cy=(int)lround(y);
    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)
        if(dx*dx+dy*dy<=radius*radius)vp_pixel(c,cx+dx,cy+dy,color);
}
typedef struct { int previous;double x,y; } VPStroke;
static inline void vp_emit_point(VPPlot* p,VPSeries* s,int index,VPRect r,VPCanvas* c,VPStroke* stroke) {
    double x,y;vp_point(s,index,&x,&y);
    if(!isfinite(x)||!isfinite(y)){stroke->previous=0;return;}
    double px=r.x+vp_map(x,p->xmin,p->xmax,r.w),py=r.y+r.h-1-vp_map(y,p->ymin,p->ymax,r.h);
    if(s->kind==VP_SCATTER)vp_marker(c,px,py,p->marker,s->color);
    else if(stroke->previous==1)vp_line(c,stroke->x,stroke->y,px,py,s->color,p->thickness);
    else if(stroke->previous==0)vp_marker(c,px,py,1,s->color);
    stroke->previous=1;stroke->x=px;stroke->y=py;
}
static inline void vp_flush_bucket(VPPlot* p,VPSeries* s,int first,int minimum,int maximum,int last,VPRect r,VPCanvas* c,VPStroke* stroke) {
    int indices[4]={first,minimum,maximum,last};
    for(int i=1;i<4;++i){int value=indices[i],j=i;while(j&&indices[j-1]>value){indices[j]=indices[j-1];--j;}indices[j]=value;}
    for(int i=0;i<4;++i)if(i==0||indices[i]!=indices[i-1])vp_emit_point(p,s,indices[i],r,c,stroke);
}
static inline void vp_draw_series(VPPlot* p,VPSeries* s,VPRect r,VPCanvas* c) {
    int begin=0,end=s->count;
    if(s->kind==VP_LINE&&s->monotonic){
        // Keep one endpoint on either side for boundary interpolation, but
        // avoid projecting the entire off-window ring for every output band.
        while(begin+1<end){double x,y,nextx,nexty;vp_point(s,begin,&x,&y);vp_point(s,begin+1,&nextx,&nexty);
            if(!isfinite(x)||!isfinite(nextx)||x>=p->xmin||nextx>=p->xmin)break;++begin;}
        while(end-1>begin){double x,y,previousx,previousy;vp_point(s,end-1,&x,&y);vp_point(s,end-2,&previousx,&previousy);
            if(!isfinite(x)||!isfinite(previousx)||x<=p->xmax||previousx<=p->xmax)break;--end;}
    }
    VPStroke stroke={begin>0?-1:0,0,0};
    if(s->kind==VP_LINE&&s->monotonic&&p->decimate&&end-begin>r.w*4){
        int first=-1,last=-1,minimum=-1,maximum=-1,key=0;double miny=0,maxy=0;
        for(int i=begin;i<end;++i){double x,y;vp_point(s,i,&x,&y);
            if(!isfinite(x)||!isfinite(y)){
                if(first>=0)vp_flush_bucket(p,s,first,minimum,maximum,last,r,c,&stroke);
                first=-1;stroke.previous=0;continue;
            }
            double mapped=vp_map(x,p->xmin,p->xmax,r.w);int column=mapped<0?-1:mapped>=r.w?r.w:(int)mapped;
            if(first>=0&&column!=key){vp_flush_bucket(p,s,first,minimum,maximum,last,r,c,&stroke);first=-1;}
            if(first<0){first=minimum=maximum=i;miny=maxy=y;key=column;}
            if(y<miny){miny=y;minimum=i;}if(y>maxy){maxy=y;maximum=i;}last=i;
        }
        if(first>=0)vp_flush_bucket(p,s,first,minimum,maximum,last,r,c,&stroke);
    } else for(int i=begin;i<end;++i)vp_emit_point(p,s,i,r,c,&stroke);
}
static inline void vp_float_rect(VPCanvas* c,double x0,double y0,double x1,double y1,uint32_t color) {
    if(!isfinite(x0)||!isfinite(y0)||!isfinite(x1)||!isfinite(y1))return;
    double left=fmax(fmin(x0,x1),c->clip.x),right=fmin(fmax(x0,x1),c->clip.x+c->clip.w);
    double top=fmax(fmin(y0,y1),c->clip.y),bottom=fmin(fmax(y0,y1),c->clip.y+c->clip.h);
    if(right<=left||bottom<=top)return;
    VPRect r={(int)floor(left),(int)floor(top),(int)ceil(right)-(int)floor(left),(int)ceil(bottom)-(int)floor(top)};vp_fill(c,r,color);
}
static inline void vp_heat_bounds(VPSeries* s,double* minimum,double* maximum) {
    *minimum=DBL_MAX;*maximum=-DBL_MAX;
    for(int i=0;i<s->count;++i)if(isfinite(s->ys[i])){*minimum=fmin(*minimum,s->ys[i]);*maximum=fmax(*maximum,s->ys[i]);}
    if(*minimum==DBL_MAX){*minimum=0;*maximum=1;}
}
static inline void vp_render_scene(VPPlot* p,VPCanvas* canvas,VPRect outer) {
    canvas->geometry=outer;
    uint32_t background=p->light?0xF8FAFC:0x0F172A,panel=p->light?0xFFFFFF:0x162238;
    uint32_t foreground=p->light?0x172033:0xE8F1FF,muted=p->light?0x64748B:0x94A3B8,grid=p->light?0xDCE4EF:0x304158;
    VPRect r=vp_data_rect(p,outer);
    if(!p->transparent){vp_fill(canvas,outer,background);vp_fill(canvas,r,panel);}
    int title_scale=outer.w>=280?2:1;
    if(outer.h>=64)vp_text(canvas,outer.x+8,outer.y+8,p->title,title_scale,foreground);
    if(p->legend&&outer.w>=160&&outer.h>=112){int x=outer.x+8,y=outer.y+(p->title[0]?28:8),row=0;
        for(int i=0;i<p->count;++i){VPSeries* s=p->series[i];char label[17];snprintf(label,sizeof(label),"%.14s",s->label);
            int width=(int)strlen(label)*6+16;if(x+width>outer.x+outer.w-8&&x>outer.x+8){x=outer.x+8;y+=12;++row;}
            if(row>=2){char more[12];snprintf(more,sizeof(more),"+%d",p->count-i);vp_text(canvas,x,y,more,1,muted);break;}
            VPRect swatch={x,y+1,6,5};vp_fill(canvas,swatch,s->color);vp_text(canvas,x+10,y,label,1,muted);x+=width;
        }
    }
    double xstep=vp_nice_step(p->xmax-p->xmin,r.w/55),ystep=vp_nice_step(p->ymax-p->ymin,r.h/28);
    if(p->grid&&p->minor){
        uint32_t minor_color=p->light?0xEEF2F7:0x233148;
        double step=xstep/5,start=ceil(p->xmin/step)*step;
        if(step>0&&isfinite(step))for(int i=0;i<80;++i){double value=start+i*step;if(value>p->xmax)break;
            double x=r.x+vp_map(value,p->xmin,p->xmax,r.w);vp_line(canvas,x,r.y,x,r.y+r.h-1,minor_color,1);}
        step=ystep/5;start=ceil(p->ymin/step)*step;
        if(step>0&&isfinite(step))for(int i=0;i<80;++i){double value=start+i*step;if(value>p->ymax)break;
            double y=r.y+r.h-1-vp_map(value,p->ymin,p->ymax,r.h);vp_line(canvas,r.x,y,r.x+r.w-1,y,minor_color,1);}
    }
    char text[32];double first=ceil(p->xmin/xstep)*xstep;
    for(int i=0;i<16;++i){double value=first+i*xstep;if(value>p->xmax+xstep*1e-9)break;
        double x=r.x+vp_map(value,p->xmin,p->xmax,r.w);if(!isfinite(x)||x<r.x-1||x>r.x+r.w)continue;
        if(p->grid)vp_line(canvas,x,r.y,x,r.y+r.h-1,grid,1);
        if(fabs(value)<xstep*1e-9)value=0;vp_number(text,sizeof(text),value);
        if(outer.w>=96&&outer.h>=80)vp_text(canvas,(int)lround(x)-(int)strlen(text)*3,r.y+r.h+4,text,1,muted);
    }
    first=ceil(p->ymin/ystep)*ystep;
    for(int i=0;i<16;++i){double value=first+i*ystep;if(value>p->ymax+ystep*1e-9)break;
        double y=r.y+r.h-1-vp_map(value,p->ymin,p->ymax,r.h);if(!isfinite(y)||y<r.y-1||y>r.y+r.h)continue;
        if(p->grid)vp_line(canvas,r.x,y,r.x+r.w-1,y,grid,1);
        if(fabs(value)<ystep*1e-9)value=0;vp_number(text,sizeof(text),value);
        if(outer.w>=96&&outer.h>=80)vp_text(canvas,r.x-5-(int)strlen(text)*6,(int)lround(y)-3,text,1,muted);
    }
    vp_line(canvas,r.x,r.y,r.x,r.y+r.h-1,muted,1);vp_line(canvas,r.x,r.y+r.h-1,r.x+r.w-1,r.y+r.h-1,muted,1);
    if(outer.w>=160&&outer.h>=112){
        vp_text(canvas,r.x+(r.w-(int)strlen(p->xlabel)*6)/2,outer.y+outer.h-9,p->xlabel,1,muted);
        vp_vertical_text(canvas,outer.x+3,r.y+(r.h-(int)strlen(p->ylabel)*6)/2,p->ylabel,muted);
    }
    VPCanvas data=*canvas;data.clip=vp_intersection(canvas->clip,r);data.geometry=r;
    for(int k=0;k<p->count;++k){VPSeries* s=p->series[k];
        if(s->kind==VP_LINE||s->kind==VP_SCATTER)vp_draw_series(p,s,r,&data);
        else if(s->kind==VP_BAR){for(int i=0;i<s->count;++i){double x,y;vp_point(s,i,&x,&y);if(!isfinite(x)||!isfinite(y))continue;
            double x0=r.x+vp_map(x-s->bar_width/2,p->xmin,p->xmax,r.w),x1=r.x+vp_map(x+s->bar_width/2,p->xmin,p->xmax,r.w);
            double y0=r.y+r.h-1-vp_map(0,p->ymin,p->ymax,r.h),y1=r.y+r.h-1-vp_map(y,p->ymin,p->ymax,r.h);
            vp_float_rect(&data,x0,y0,x1,y1,s->color);
        }}else if(s->kind==VP_HEATMAP&&s->count){double minimum,maximum;vp_heat_bounds(s,&minimum,&maximum);
            int diverging=minimum<0&&maximum>0;double extent=fmax(fabs(minimum),fabs(maximum));
            for(int row=0;row<s->rows;++row)for(int col=0;col<s->cols;++col){double value=s->ys[row*s->cols+col];if(!isfinite(value))continue;
                double norm=diverging?(value/extent+1)/2:maximum==minimum?.5:(value-minimum)/(maximum-minimum);
                double x0=r.x+vp_map(col,p->xmin,p->xmax,r.w),x1=r.x+vp_map(col+1,p->xmin,p->xmax,r.w);
                double y0=r.y+r.h-1-vp_map(s->rows-row,p->ymin,p->ymax,r.h),y1=r.y+r.h-1-vp_map(s->rows-row-1,p->ymin,p->ymax,r.h);
                vp_float_rect(&data,x0,y0,x1+1,y1+1,vp_heat_color(norm,diverging));
            }
            for(int i=0;i<r.h;++i){VPRect strip={r.x+r.w+8,r.y+i,8,1};vp_fill(canvas,strip,vp_heat_color(1-(double)i/(r.h-1),diverging));}
            vp_number(text,sizeof(text),maximum);vp_text(canvas,r.x+r.w+19,r.y,text,1,muted);
            vp_number(text,sizeof(text),minimum);vp_text(canvas,r.x+r.w+19,r.y+r.h-7,text,1,muted);
        }
    }
}
static inline int vp_render_at(int64_t plot,int64_t surface,VPRect outer) {
    VPPlot* p=(VPPlot*)vp_object(plot,VP_OBJECT_PLOT);VPSurface* s=(VPSurface*)vp_object(surface,VP_OBJECT_SURFACE);if(!p||!s)return 0;
    if(s->refresh)s->refresh(s);
    if(outer.x<0||outer.y<0||outer.w<32||outer.h<32||outer.w>s->width||outer.h>s->height||
       outer.x>s->width-outer.w||outer.y>s->height-outer.h)return vp_fail(VP_ARGUMENT);
    VPRect data=vp_data_rect(p,outer);if(data.w<8||data.h<8)return vp_fail(VP_ARGUMENT);
    if(p->transparent&&!s->preserve)return vp_fail(VP_CAPABILITY);
    int borrowed=0;for(int i=0;i<p->count;++i)borrowed|=p->series[i]->borrowed;
    if(!borrowed&&!s->volatile_pixels&&s->last_plot==plot&&s->last_revision==p->revision&&
       memcmp(&outer,&s->last_rect,sizeof(outer))==0){vp_last_error=VP_OK;return 1;}
    if(!vp_prepare(p))return 0;
    if(s->draw){if(!s->draw(p,s,outer))return 0;}
    else {VPCanvas c=s->canvas;c.clip=outer;vp_render_scene(p,&c,outer);}
    s->last_plot=plot;s->last_revision=p->revision;s->last_rect=outer;++s->frames;vp_last_error=VP_OK;return 1;
}
static inline int vp_render(int64_t plot,int64_t surface) {
    VPSurface* s=(VPSurface*)vp_object(surface,VP_OBJECT_SURFACE);if(!s)return 0;
    if(s->refresh)s->refresh(s);VPRect rect={0,0,s->width,s->height};return vp_render_at(plot,surface,rect);
}
static inline int64_t vp_custom_surface(int width,int height,void* context,
    void (*pixel)(void*,int,int,uint32_t),void (*span)(void*,int,int,int,uint32_t),void (*dispose)(void*),int preserve) {
    if(width<1||height<1||width>VPLOT_MAX_DIMENSION||height>VPLOT_MAX_DIMENSION||!pixel)return vp_fail(VP_ARGUMENT);
    VPSurface* s=(VPSurface*)vp_alloc(sizeof(VPSurface));if(!s)return 0;
    s->width=width;s->height=height;s->canvas.context=context;s->canvas.pixel=pixel;s->canvas.span=span;s->dispose=dispose;s->preserve=preserve;
    s->handle=vp_register(s,VP_OBJECT_SURFACE);if(!s->handle){vp_free(s);return 0;}
    vp_last_error=VP_OK;return s->handle;
}
static inline int vp_destroy_surface(int64_t handle) {
    VPSurface* s=(VPSurface*)vp_object(handle,VP_OBJECT_SURFACE);if(!s)return 0;
    if(s->dispose)s->dispose(s->canvas.context);vp_unregister(handle);vp_free(s);vp_last_error=VP_OK;return 1;
}

typedef struct { int width,height;unsigned char* pixels;double* floating;int channels; } VPRaster;
static inline void vp_raster_pixel(void* context,int x,int y,uint32_t color) {
    VPRaster* r=(VPRaster*)context;size_t offset=((size_t)y*r->width+x)*(r->floating?r->channels:3);
    double red=((color>>16)&255)/255.,green=((color>>8)&255)/255.,blue=(color&255)/255.;
    if(r->floating){if(r->channels==1)r->floating[offset]=.299*red+.587*green+.114*blue;
        else {r->floating[offset]=red;r->floating[offset+1]=green;r->floating[offset+2]=blue;if(r->channels==4)r->floating[offset+3]=1;}}
    else {r->pixels[offset]=(unsigned char)(color>>16);r->pixels[offset+1]=(unsigned char)(color>>8);r->pixels[offset+2]=(unsigned char)color;}
}
static inline void vp_raster_span(void* context,int x,int y,int width,uint32_t color) {
    for(int i=0;i<width;++i)vp_raster_pixel(context,x+i,y,color);
}
static inline void vp_raster_dispose(void* context) {VPRaster* r=(VPRaster*)context;vp_free(r->pixels);vp_free(r);}
static inline int64_t vp_raster_surface(int width,int height,double* pixels,int channels) {
    if(width<1||height<1||width>VPLOT_MAX_DIMENSION||height>VPLOT_MAX_DIMENSION||
       (pixels&&channels!=1&&channels!=3&&channels!=4))return vp_fail(VP_ARGUMENT);
    VPRaster* r=(VPRaster*)vp_alloc(sizeof(VPRaster));if(!r)return 0;
    r->width=width;r->height=height;r->floating=pixels;r->channels=channels;
    if(!pixels){r->pixels=(unsigned char*)vp_alloc((size_t)width*height*3);if(!r->pixels){vp_free(r);return 0;}}
    int64_t handle=vp_custom_surface(width,height,r,vp_raster_pixel,vp_raster_span,vp_raster_dispose,1);
    if(!handle){vp_raster_dispose(r);return 0;}
    VPSurface* s=(VPSurface*)vp_object(handle,VP_OBJECT_SURFACE);s->raster=pixels?2:1;s->volatile_pixels=pixels!=NULL;return handle;
}
