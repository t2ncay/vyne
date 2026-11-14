// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#define VYNE_VPLOT_MCU 1
#include "../vplot/vplot_bridge.h"

#ifdef VYNE_USE_PLOT_DISPLAY
#include "mcu_display.h"
typedef struct {uint16_t* pixels;size_t budget;int x,y,width,rows;} VPDisplay;
static inline uint16_t vp_rgb565(uint32_t color) {
    return (uint16_t)(((color>>8)&0xF800)|((color>>5)&0x7E0)|((color>>3)&0x1F));
}
static inline void vp_display_pixel(void* context,int x,int y,uint32_t color) {
    VPDisplay* d=(VPDisplay*)context;
    if(d->pixels)d->pixels[(y-d->y)*d->width+x-d->x]=vp_rgb565(color);
    else g_vyne_tft.drawPixel(x,y,vp_rgb565(color));
}
static inline void vp_display_span(void* context,int x,int y,int width,uint32_t color) {
    VPDisplay* d=(VPDisplay*)context;
    if(!d->pixels){g_vyne_tft.drawFastHLine(x,y,width,vp_rgb565(color));return;}
    uint16_t* target=d->pixels+(y-d->y)*d->width+x-d->x;
    uint16_t value=vp_rgb565(color);for(int i=0;i<width;++i)target[i]=value;
}
static inline void vp_display_dispose(void* context) {VPDisplay* d=(VPDisplay*)context;vp_free(d->pixels);vp_free(d);}
static inline void vp_display_refresh(VPSurface* surface) {surface->width=g_vyne_tft.width();surface->height=g_vyne_tft.height();}
static inline int vp_display_draw(VPPlot* plot,VPSurface* surface,VPRect outer) {
    VPDisplay* d=(VPDisplay*)surface->canvas.context;
    if(!d->pixels){VPCanvas canvas=surface->canvas;canvas.clip=outer;vp_render_scene(plot,&canvas,outer);return 1;}
    int band=(int)(d->budget/((size_t)outer.w*2));if(band<1)return vp_fail(VP_CAPACITY);
    bool previous=g_vyne_tft.getSwapBytes();g_vyne_tft.setSwapBytes(true);
    for(int y=outer.y;y<outer.y+outer.h;y+=band){
        d->x=outer.x;d->y=y;d->width=outer.w;d->rows=outer.y+outer.h-y;if(d->rows>band)d->rows=band;
        VPCanvas canvas=surface->canvas;VPRect clip={outer.x,y,outer.w,d->rows};canvas.clip=clip;
        vp_render_scene(plot,&canvas,outer);
        g_vyne_tft.pushImage(outer.x,y,outer.w,d->rows,d->pixels);
    }
    g_vyne_tft.setSwapBytes(previous);return 1;
}
static inline VyneValue vplot_display_surface(VyneValue buffer_bytes) {
    int64_t bytes=vp_b_int(buffer_bytes);
    if(!g_vyne_tft_inited||bytes<0||bytes>65536)return vyne_int(vp_fail(VP_ARGUMENT));
    VPDisplay* d=(VPDisplay*)vp_alloc(sizeof(VPDisplay));if(!d)return vyne_int(0);
    d->budget=(size_t)bytes;
    if(bytes){if(bytes<g_vyne_tft.width()*2){vp_free(d);return vyne_int(vp_fail(VP_CAPACITY));}
        d->pixels=(uint16_t*)vp_alloc((size_t)bytes);if(!d->pixels){vp_free(d);return vyne_int(0);}}
    int64_t handle=vp_custom_surface(g_vyne_tft.width(),g_vyne_tft.height(),d,vp_display_pixel,vp_display_span,vp_display_dispose,bytes==0);
    if(!handle){vp_display_dispose(d);return vyne_int(0);}
    VPSurface* s=(VPSurface*)vp_object(handle,VP_OBJECT_SURFACE);s->refresh=vp_display_refresh;s->draw=vp_display_draw;
    return vyne_int(handle);
}
#endif
