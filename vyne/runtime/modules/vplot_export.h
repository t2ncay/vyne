// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include "vplot.h"
#include "vimage.h"
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static inline int vp_export_parents(const char* path) {
    char parents[4096];size_t length=strlen(path);
    if(!length||length>=sizeof(parents))return vp_fail(VP_ARGUMENT);
    memcpy(parents,path,length+1);
    for(size_t i=1;i<length;++i){
        if(parents[i]!='/'
#ifdef _WIN32
           &&parents[i]!='\\'
#endif
        )continue;
        char separator=parents[i];parents[i]=0;
#ifdef _WIN32
        int status=i==2&&parents[1]==':'?0:_mkdir(parents);
#else
        int status=mkdir(parents,0777);
#endif
        parents[i]=separator;
        if(status!=0&&errno!=EEXIST)return vp_fail(VP_ARGUMENT);
    }
    return 1;
}
static inline VyneValue vplot_save(VyneValue plot,VyneValue path) {
    VPPlot* p=(VPPlot*)vp_object(vp_b_int(plot),VP_OBJECT_PLOT);const char* filename=vp_b_text(path);
    if(!p)return vyne_bool(false);
    if(!filename||!vp_export_parents(filename))return vyne_bool(vp_fail(VP_ARGUMENT));
    if(!vp_managed_show(p,(uint32_t)VPLOT_CLOCK_MS(),1))return vyne_bool(false);
    VPSurface* s=(VPSurface*)vp_object(p->managed_surface,VP_OBJECT_SURFACE);
    if(!s||s->raster!=1)return vyne_bool(vp_fail(VP_CAPABILITY));
    VPRaster* r=(VPRaster*)s->canvas.context;
    if(!stbi_write_png(filename,r->width,r->height,3,r->pixels,r->width*3))return vyne_bool(vp_fail(VP_ARGUMENT));
    vp_last_error=VP_OK;return vyne_bool(true);
}
static inline VyneValue vp_quick_save(int64_t id,VyneValue path) {
    if(!id)return vyne_bool(false);
    VyneValue result=vplot_save(vyne_int(id),path);int error=vp_last_error;
    vp_destroy_plot(id);vp_last_error=error;return result;
}
static inline VyneValue vplot_save_line(VyneValue xs,VyneValue ys,VyneValue title,VyneValue path) {
    return vp_quick_save(vp_quick_plot(xs,ys,title,VP_LINE),path);
}
static inline VyneValue vplot_save_scatter(VyneValue xs,VyneValue ys,VyneValue title,VyneValue path) {
    return vp_quick_save(vp_quick_plot(xs,ys,title,VP_SCATTER),path);
}
static inline VyneValue vplot_save_bars(VyneValue xs,VyneValue ys,VyneValue title,VyneValue path) {
    return vp_quick_save(vp_quick_plot(xs,ys,title,VP_BAR),path);
}
static inline VyneValue vplot_save_histogram(VyneValue values,VyneValue title,VyneValue path) {
    return vp_quick_save(vp_quick_histogram(values,title),path);
}
static inline VyneValue vplot_save_heatmap(VyneValue values,VyneValue cols,VyneValue rows,VyneValue title,VyneValue path) {
    return vp_quick_save(vp_quick_heatmap(values,cols,rows,title),path);
}
