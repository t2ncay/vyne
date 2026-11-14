// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include "vplot_core.h"

// Fixed-arity native bindings use stack VyneValues. Stateful objects and their
// reserved buffers live in the native core, never in rewindable Vyne regions.
static inline int64_t vp_b_int(VyneValue v) {
    if(v.type==V_INT64||v.type==V_BOOL)return v.as.i64;
    vp_fail(VP_ARGUMENT);return -1;
}
static inline int vp_b_numeric(VyneValue v) {return v.type==V_INT64||v.type==V_FLOAT64||v.type==V_FLOAT32;}
static inline double vp_b_number(VyneValue v) {return v.type==V_INT64?(double)v.as.i64:v.as.f64;}
static inline const char* vp_b_text(VyneValue v) {return v.type==V_STRING?v.as.str:NULL;}
typedef struct {void* data;int64_t size,capacity;} VPArrayHeader;
static inline VPArrayHeader vp_b_header(VyneValue v) {
    VPArrayHeader header={NULL,0,0};
    if(v.as.ptr)memcpy(&header,v.as.ptr,sizeof(header));
    return header;
}
static inline int64_t vp_b_length(VyneValue v) {
    if(v.type==V_ARRAY)return v.as.arr?v.as.arr->size:-1;
    if(v.type==V_F64_ARRAY||v.type==V_I64_ARRAY||v.type==V_F32_ARRAY)return v.as.ptr?vp_b_header(v).size:-1;
    return -1;
}
static inline double vp_b_at(VyneValue v,int64_t index) {
    if(v.type==V_F64_ARRAY)return ((double*)vp_b_header(v).data)[index];
    if(v.type==V_I64_ARRAY)return (double)((int64_t*)vp_b_header(v).data)[index];
    if(v.type==V_F32_ARRAY)return ((float*)vp_b_header(v).data)[index];
    return vp_b_number(v.as.arr->elements[index]);
}
static inline int vp_b_array_valid(VyneValue v,int64_t count) {
    if(count<0||count>VPLOT_MAX_POINTS)return vp_fail(VP_ARGUMENT);
    if(v.type==V_ARRAY){if(count&&!v.as.arr->elements)return vp_fail(VP_ARGUMENT);
        for(int64_t i=0;i<count;++i)if(!vp_b_numeric(v.as.arr->elements[i]))return vp_fail(VP_ARGUMENT);
    }else {VPArrayHeader header=vp_b_header(v);
        if(header.capacity<header.size||(count&&!header.data))return vp_fail(VP_ARGUMENT);
    }
    return 1;
}
static inline VyneValue vplot_new_plot(VyneValue title,VyneValue x,VyneValue y) {
    return vyne_int(vp_create_plot(vp_b_text(title),vp_b_text(x),vp_b_text(y)));
}
static inline VyneValue vplot_destroy(VyneValue handle) {
    int64_t id=vp_b_int(handle);
    if(vp_object(id,VP_OBJECT_PLOT))return vyne_bool(vp_destroy_plot(id));
    if(vp_object(id,VP_OBJECT_SURFACE))return vyne_bool(vp_destroy_surface(id));
    // Series are owned by their plot; destroying one independently is invalid.
    return vyne_bool(vp_fail(VP_HANDLE));
}
static inline VyneValue vplot_live_series(VyneValue plot,VyneValue capacity,VyneValue label,VyneValue color) {
    int64_t n=vp_b_int(capacity);if(n<1||n>VPLOT_MAX_POINTS)return vyne_int(vp_fail(VP_ARGUMENT));
    return vyne_int(vp_add_series(vp_b_int(plot),(int)n,vp_b_text(label),(uint32_t)vp_b_int(color),VP_LINE));
}
static inline VyneValue vplot_append(VyneValue series,VyneValue x,VyneValue y) {
    if(!vp_b_numeric(x)||!vp_b_numeric(y))return vyne_bool(vp_fail(VP_ARGUMENT));
    return vyne_bool(vp_append(vp_b_int(series),vp_b_number(x),vp_b_number(y)));
}
static inline VyneValue vplot_clear(VyneValue series) {return vyne_bool(vp_clear(vp_b_int(series)));}
static inline int vp_b_set_data(int64_t handle,VyneValue xs,VyneValue ys) {
    VPSeries* s=(VPSeries*)vp_object(handle,VP_OBJECT_SERIES);if(!s)return 0;
    int64_t n=vp_b_length(xs),m=vp_b_length(ys);
    if(n!=m||!vp_b_array_valid(xs,n)||!vp_b_array_valid(ys,m))return vp_fail(VP_ARGUMENT);
    if(s->kind==VP_HEATMAP)return vp_fail(VP_ARGUMENT);
    if(n>s->capacity)return vp_fail(VP_CAPACITY);
    vp_clear(handle);for(int64_t i=0;i<n;++i)vp_append(handle,vp_b_at(xs,i),vp_b_at(ys,i));
    return 1;
}
static inline VyneValue vplot_set_data(VyneValue series,VyneValue xs,VyneValue ys) {return vyne_bool(vp_b_set_data(vp_b_int(series),xs,ys));}
static inline VyneValue vp_b_make_series(VyneValue plot,VyneValue xs,VyneValue ys,VyneValue label,VyneValue color,int kind,int borrowed) {
    int64_t n=vp_b_length(xs),m=vp_b_length(ys);
    if(n!=m||!vp_b_array_valid(xs,n)||!vp_b_array_valid(ys,m)||!vp_b_text(label))return vyne_int(vp_fail(VP_ARGUMENT));
    if(borrowed&&(xs.type!=V_F64_ARRAY||ys.type!=V_F64_ARRAY))return vyne_int(vp_fail(VP_ARGUMENT));
    int64_t id=vp_add_series(vp_b_int(plot),borrowed?1:(n?(int)n:1),vp_b_text(label),(uint32_t)vp_b_int(color),kind);
    if(!id)return vyne_int(0);
    if(borrowed)vp_borrow(id,(double*)vp_b_header(xs).data,(double*)vp_b_header(ys).data,(int)n);
    else vp_b_set_data(id,xs,ys);
    return vyne_int(id);
}
static inline VyneValue vplot_line(VyneValue p,VyneValue x,VyneValue y,VyneValue label,VyneValue color) {return vp_b_make_series(p,x,y,label,color,VP_LINE,0);}
static inline VyneValue vplot_line_view(VyneValue p,VyneValue x,VyneValue y,VyneValue label,VyneValue color) {return vp_b_make_series(p,x,y,label,color,VP_LINE,1);}
static inline VyneValue vplot_scatter(VyneValue p,VyneValue x,VyneValue y,VyneValue label,VyneValue color) {return vp_b_make_series(p,x,y,label,color,VP_SCATTER,0);}
static inline VyneValue vplot_bars(VyneValue p,VyneValue x,VyneValue y,VyneValue label,VyneValue color) {return vp_b_make_series(p,x,y,label,color,VP_BAR,0);}
static inline VyneValue vplot_bar_width(VyneValue series,VyneValue width) {
    VPSeries* s=(VPSeries*)vp_object(vp_b_int(series),VP_OBJECT_SERIES);
    if(!s)return vyne_bool(false);
    if(s->kind!=VP_BAR||!vp_b_numeric(width)||!isfinite(vp_b_number(width))||vp_b_number(width)<=0)return vyne_bool(vp_fail(VP_ARGUMENT));
    s->bar_width=vp_b_number(width);++s->owner->revision;vp_last_error=VP_OK;return vyne_bool(true);
}
static inline int vp_b_histogram_update(int64_t id,VyneValue values) {
    VPSeries* s=(VPSeries*)vp_object(id,VP_OBJECT_SERIES);if(!s)return 0;
    int64_t n=vp_b_length(values);if(s->kind!=VP_BAR||!vp_b_array_valid(values,n))return vp_fail(VP_ARGUMENT);
    double lo=DBL_MAX,hi=-DBL_MAX;
    for(int64_t i=0;i<n;++i){double v=vp_b_at(values,i);if(isfinite(v)){lo=fmin(lo,v);hi=fmax(hi,v);}}
    if(lo==DBL_MAX){lo=0;hi=1;}if(!vp_expand(&lo,&hi,0))return vp_fail(VP_RANGE);
    double step=(hi-lo)/s->capacity;if(!isfinite(step)||step<=0)return vp_fail(VP_RANGE);
    vp_clear(id);s->count=s->capacity;s->bar_width=step*.95;
    for(int i=0;i<s->count;++i){s->xs[i]=lo+(i+.5)*step;s->ys[i]=0;}
    for(int64_t i=0;i<n;++i){double value=vp_b_at(values,i);if(!isfinite(value))continue;
        int bin=value==hi?s->count-1:(int)((value-lo)/step);if(bin>=0&&bin<s->count)++s->ys[bin];
    }
    ++s->owner->revision;vp_last_error=VP_OK;return 1;
}
static inline VyneValue vplot_histogram(VyneValue plot,VyneValue values,VyneValue bins,VyneValue label,VyneValue color) {
    int64_t n=vp_b_length(values),count=vp_b_int(bins);
    if(count<1||count>4096||!vp_b_array_valid(values,n))return vyne_int(vp_fail(VP_ARGUMENT));
    int64_t id=vp_add_series(vp_b_int(plot),(int)count,vp_b_text(label),(uint32_t)vp_b_int(color),VP_BAR);
    if(!id)return vyne_int(0);
    if(!vp_b_histogram_update(id,values)){
        VPSeries* s=(VPSeries*)vp_object(id,VP_OBJECT_SERIES);--s->owner->count;vp_release_series(s);return vyne_int(0);
    }
    return vyne_int(id);
}
static inline VyneValue vplot_histogram_update(VyneValue series,VyneValue data) {return vyne_bool(vp_b_histogram_update(vp_b_int(series),data));}
static inline int vp_b_heat_update(int64_t id,VyneValue values) {
    VPSeries* s=(VPSeries*)vp_object(id,VP_OBJECT_SERIES);if(!s)return 0;
    int64_t n=vp_b_length(values);
    if(s->kind!=VP_HEATMAP||n!=(int64_t)s->cols*s->rows||!vp_b_array_valid(values,n))return vp_fail(VP_ARGUMENT);
    for(int64_t i=0;i<n;++i)s->ys[i]=vp_b_at(values,i);
    s->count=(int)n;++s->owner->revision;vp_last_error=VP_OK;return 1;
}
static inline VyneValue vplot_heatmap(VyneValue plot,VyneValue values,VyneValue cols,VyneValue rows,VyneValue label) {
    int64_t width=vp_b_int(cols),height=vp_b_int(rows),n=vp_b_length(values);
    if(width<1||height<1||width>VPLOT_MAX_DIMENSION||height>VPLOT_MAX_DIMENSION||width*height!=n||!vp_b_array_valid(values,n))
        return vyne_int(vp_fail(VP_ARGUMENT));
    int64_t id=vp_add_series(vp_b_int(plot),(int)n,vp_b_text(label),0x21918C,VP_HEATMAP);
    if(!id)return vyne_int(0);
    VPSeries* s=(VPSeries*)vp_object(id,VP_OBJECT_SERIES);s->cols=(int)width;s->rows=(int)height;
    vp_b_heat_update(id,values);return vyne_int(id);
}
static inline VyneValue vplot_heatmap_update(VyneValue series,VyneValue data) {return vyne_bool(vp_b_heat_update(vp_b_int(series),data));}
static inline VyneValue vplot_limits(VyneValue plot,VyneValue a,VyneValue b,VyneValue c,VyneValue d) {
    if(!vp_b_numeric(a)||!vp_b_numeric(b)||!vp_b_numeric(c)||!vp_b_numeric(d))return vyne_bool(vp_fail(VP_ARGUMENT));
    return vyne_bool(vp_limits(vp_b_int(plot),vp_b_number(a),vp_b_number(b),vp_b_number(c),vp_b_number(d)));
}
static inline VyneValue vplot_autoscale(VyneValue plot,VyneValue padding,VyneValue hysteresis,VyneValue window) {
    if(!vp_b_numeric(padding)||!vp_b_numeric(hysteresis)||!vp_b_numeric(window))return vyne_bool(vp_fail(VP_ARGUMENT));
    return vyne_bool(vp_autoscale(vp_b_int(plot),vp_b_number(padding),vp_b_number(hysteresis),vp_b_number(window)));
}
#define VP_OPTION(name,field,minimum,maximum) \
static inline VyneValue vplot_##name(VyneValue plot,VyneValue value) { \
    VPPlot* p=(VPPlot*)vp_object(vp_b_int(plot),VP_OBJECT_PLOT);if(!p)return vyne_bool(false); \
    int64_t n=vp_b_int(value);if(n<minimum||n>maximum)return vyne_bool(vp_fail(VP_ARGUMENT)); \
    p->field=(int)n;++p->revision;vp_last_error=VP_OK;return vyne_bool(true); }
VP_OPTION(theme,light,0,1)
VP_OPTION(grid,grid,0,1)
VP_OPTION(minor_grid,minor,0,1)
VP_OPTION(legend,legend,0,1)
VP_OPTION(transparent,transparent,0,1)
VP_OPTION(decimation,decimate,0,1)
VP_OPTION(thickness,thickness,1,7)
VP_OPTION(marker_size,marker,1,8)
#undef VP_OPTION
static inline VyneValue vplot_invalidate(VyneValue plot) {
    VPPlot* p=(VPPlot*)vp_object(vp_b_int(plot),VP_OBJECT_PLOT);if(!p)return vyne_bool(false);
    ++p->revision;vp_last_error=VP_OK;return vyne_bool(true);
}
static inline VyneValue vplot_rgb(VyneValue red,VyneValue green,VyneValue blue) {
    int64_t r=vp_b_int(red),g=vp_b_int(green),b=vp_b_int(blue);
    if(r<0||r>255||g<0||g>255||b<0||b>255){vp_fail(VP_ARGUMENT);return vyne_int(-1);}
    vp_last_error=VP_OK;return vyne_int((r<<16)|(g<<8)|b);
}
static inline VyneValue vplot_render(VyneValue plot,VyneValue surface) {return vyne_bool(vp_render(vp_b_int(plot),vp_b_int(surface)));}
static inline VyneValue vplot_render_at(VyneValue plot,VyneValue surface,VyneValue x,VyneValue y,VyneValue width,VyneValue height) {
    int64_t a=vp_b_int(x),b=vp_b_int(y),w=vp_b_int(width),h=vp_b_int(height);
    if(a<0||b<0||a>VPLOT_MAX_DIMENSION||b>VPLOT_MAX_DIMENSION||w<0||h<0||w>VPLOT_MAX_DIMENSION||h>VPLOT_MAX_DIMENSION)
        return vyne_bool(vp_fail(VP_ARGUMENT));
    VPRect rect={(int)a,(int)b,(int)w,(int)h};return vyne_bool(vp_render_at(vp_b_int(plot),vp_b_int(surface),rect));
}
static inline VyneValue vplot_surface_width(VyneValue surface) {
    VPSurface* s=(VPSurface*)vp_object(vp_b_int(surface),VP_OBJECT_SURFACE);if(!s)return vyne_int(0);
    if(s->refresh)s->refresh(s);return vyne_int(s->width);
}
static inline VyneValue vplot_surface_height(VyneValue surface) {
    VPSurface* s=(VPSurface*)vp_object(vp_b_int(surface),VP_OBJECT_SURFACE);if(!s)return vyne_int(0);
    if(s->refresh)s->refresh(s);return vyne_int(s->height);
}
static inline VyneValue vplot_frame_count(VyneValue surface) {
    VPSurface* s=(VPSurface*)vp_object(vp_b_int(surface),VP_OBJECT_SURFACE);return vyne_int(s?(int64_t)s->frames:0);
}
static inline VyneValue vplot_series_count(VyneValue series) {
    VPSeries* s=(VPSeries*)vp_object(vp_b_int(series),VP_OBJECT_SERIES);return vyne_int(s?s->count:0);
}
static inline VyneValue vplot_sample_x(VyneValue series,VyneValue index) {
    VPSeries* s=(VPSeries*)vp_object(vp_b_int(series),VP_OBJECT_SERIES);int64_t i=vp_b_int(index);
    if(!s||s->kind==VP_HEATMAP||i<0||i>=s->count){vp_fail(VP_ARGUMENT);return vyne_float(NAN);}
    double x,y;vp_point(s,(int)i,&x,&y);return vyne_float(x);
}
static inline VyneValue vplot_sample_y(VyneValue series,VyneValue index) {
    VPSeries* s=(VPSeries*)vp_object(vp_b_int(series),VP_OBJECT_SERIES);int64_t i=vp_b_int(index);
    if(!s||i<0||i>=s->count){vp_fail(VP_ARGUMENT);return vyne_float(NAN);}
    if(s->kind==VP_HEATMAP)return vyne_float(s->ys[i]);
    double x,y;vp_point(s,(int)i,&x,&y);return vyne_float(y);
}
static inline VyneValue vplot_memory_bytes(void) {return vyne_int((int64_t)vp_stats.live);}
static inline VyneValue vplot_peak_bytes(void) {return vyne_int((int64_t)vp_stats.peak);}
static inline VyneValue vplot_allocation_count(void) {return vyne_int((int64_t)vp_stats.allocations);}
static inline VyneValue vplot_last_error(void) {return vyne_string_static(vp_error_text());}

#ifndef VYNE_VPLOT_MCU
static inline VyneValue vplot_image_surface(VyneValue width,VyneValue height) {
    int64_t w=vp_b_int(width),h=vp_b_int(height);
    if(w<1||h<1||w>VPLOT_MAX_DIMENSION||h>VPLOT_MAX_DIMENSION)return vyne_int(vp_fail(VP_ARGUMENT));
    return vyne_int(vp_raster_surface((int)w,(int)h,NULL,3));
}
static inline VyneValue vplot_image_bind(VyneValue pixels,VyneValue width,VyneValue height,VyneValue channels) {
    int64_t w=vp_b_int(width),h=vp_b_int(height),c=vp_b_int(channels);
    if(pixels.type!=V_F64_ARRAY||w<1||h<1||w>VPLOT_MAX_DIMENSION||h>VPLOT_MAX_DIMENSION||
       (c!=1&&c!=3&&c!=4)||vp_b_length(pixels)!=w*h*c||vp_b_header(pixels).capacity<w*h*c||!vp_b_header(pixels).data)return vyne_int(vp_fail(VP_ARGUMENT));
    return vyne_int(vp_raster_surface((int)w,(int)h,(double*)vp_b_header(pixels).data,(int)c));
}
static inline VyneValue vplot_image_pixels(VyneValue surface) {
    VPSurface* s=(VPSurface*)vp_object(vp_b_int(surface),VP_OBJECT_SURFACE);
    if(!s||!s->raster){vp_fail(VP_CAPABILITY);return vyne_null();}
    VPRaster* r=(VPRaster*)s->canvas.context;
    VyneArray_f64 result=vyne_array_f64_create((int64_t)r->width*r->height*3);
    for(int64_t i=0;i<(int64_t)r->width*r->height;++i)for(int c=0;c<3;++c){
        result.data[i*3+c]=r->floating?r->floating[i*r->channels+(r->channels==1?0:c)]:r->pixels[i*3+c]/255.;
    }
    return vyne_array_f64_to_value(&result);
}
static inline VyneValue vplot_pixel_rgb(VyneValue surface,VyneValue x,VyneValue y) {
    VPSurface* s=(VPSurface*)vp_object(vp_b_int(surface),VP_OBJECT_SURFACE);int64_t a=vp_b_int(x),b=vp_b_int(y);
    if(!s||!s->raster||a<0||b<0||a>=s->width||b>=s->height){vp_fail(VP_ARGUMENT);return vyne_int(-1);}
    VPRaster* r=(VPRaster*)s->canvas.context;size_t i=(size_t)b*r->width+a;uint32_t color=0;
    for(int c=0;c<3;++c){double value=r->floating?r->floating[i*r->channels+(r->channels==1?0:c)]:r->pixels[i*3+c]/255.;
        if(!isfinite(value))value=0;value=fmax(0,fmin(1,value));color|=(uint32_t)lround(value*255)<<(16-c*8);}
    return vyne_int(color);
}
#endif
