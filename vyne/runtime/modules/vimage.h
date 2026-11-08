// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

/* vyne/runtime/modules/vimage.h
 * -------------------------------------------------------------------
 * Vyne runtime vimage module - transpiler target.
 *
 * Image decode/encode on top of the vendored stb single-header
 * libraries. This header is included from generated C, so the stb
 * implementations are compiled in exactly one translation unit (the
 * generated program). No new link dependency; stb is header-only.
 *
 * Pixel convention
 * ---------------
 *   - Images are stored HWC (row-major, then channel).
 *   - Samples are Float64 in [0, 1]. 8-bit source data is divided by
 *     255 on load and multiplied back on save.
 *   - `load_raw` returns a flat array whose first three elements are
 *     width, height, channels, followed by width*height*channels
 *     samples. The Vyne facade (`vimage.Image`) unpacks this.
 *
 * Naming convention: vimage_<name>
 *
 *   Decode : load_raw
 *   Encode : save_raw
 *   Helper : pixels  (view of load_raw with the 3-element header removed)
 *   Resample: resize_bilinear, crop, grayscale
 *
 * Decode failures return null. The Vyne wrapper turns that into a
 * thrown error with the path in the message. Encode failures return
 * false (missing directory, unsupported extension).
 * ------------------------------------------------------------------- */

#ifndef VYNE_VIMAGE_RT_H
#define VYNE_VIMAGE_RT_H

#include "../vyne_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
/* Keep the translation unit small. HDR and linear-light conversion are
 * out of scope for SDR image super-resolution. */
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../../../vendor/stb/stb_image.h"
#include "../../../vendor/stb/stb_image_write.h"

/* ---------------------------------------------------------------- */
/* Argument helpers                                                  */
/* ---------------------------------------------------------------- */

static inline int64_t _vimage_int(VyneValue v, const char* fn, const char* arg) {
    if (v.type == V_INT64) return v.as.i64;
    if (v.type == V_FLOAT64 || v.type == V_FLOAT32) return (int64_t)v.as.f64;
    fprintf(stderr, "Runtime error: vimage.%s() expects an integer for %s.\n", fn, arg);
    exit(1);
}

static inline const char* _vimage_path(VyneValue v, const char* fn) {
    if (v.type != V_STRING || !v.as.str) {
        fprintf(stderr, "Runtime error: vimage.%s() expects a String path.\n", fn);
        exit(1);
    }
    return v.as.str;
}

static inline VyneArray_f64 _vimage_array(VyneValue v) {
    return vyne_value_to_array_f64(v);
}

static inline int _vimage_has_suffix(const char* path, const char* suffix) {
    size_t plen = strlen(path);
    size_t slen = strlen(suffix);
    if (plen < slen) return 0;
    const char* p = path + (plen - slen);
    for (size_t i = 0; i < slen; ++i) {
        char a = p[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* Decode                                                            */
/* ---------------------------------------------------------------- */

/* Returns [width, height, channels, samples...] or null on failure. */
static inline VyneValue vimage_load_raw(VyneValue path_val) {
    const char* path = _vimage_path(path_val, "load_raw");

    int w = 0, h = 0, c = 0;
    unsigned char* src = stbi_load(path, &w, &h, &c, 0);
    if (!src || w <= 0 || h <= 0 || c <= 0) {
        if (src) stbi_image_free(src);
        return vyne_null();
    }

    int64_t n = (int64_t)w * (int64_t)h * (int64_t)c;
    VyneArray_f64 out = vyne_array_f64_create(3 + n);
    out.data[0] = (double)w;
    out.data[1] = (double)h;
    out.data[2] = (double)c;

    const double inv = 1.0 / 255.0;
    for (int64_t i = 0; i < n; ++i)
        out.data[3 + i] = (double)src[i] * inv;

    stbi_image_free(src);
    return vyne_array_f64_to_value(&out);
}

/* O(1) view of `load_raw`'s payload with the 3-element header removed.
 * The returned array aliases the input buffer; do not outlive the raw
 * array. */
static inline VyneValue vimage_pixels(VyneValue raw_val) {
    VyneArray_f64 src = _vimage_array(raw_val);
    if (src.size < 3) return vyne_array_f64_to_value(&src);

    VyneArray_f64 view;
    view.data = src.data + 3;
    view.size = src.size - 3;
    view.cap  = (src.cap > 3) ? src.cap - 3 : view.size;
    return vyne_array_f64_to_value(&view);
}

/* ---------------------------------------------------------------- */
/* Encode                                                            */
/* ---------------------------------------------------------------- */

static inline VyneValue vimage_save_raw(
    VyneValue path_val,
    VyneValue data_val,
    VyneValue w_val,
    VyneValue h_val,
    VyneValue c_val,
    VyneValue quality_val
) {
    const char* path = _vimage_path(path_val, "save_raw");
    VyneArray_f64 data = _vimage_array(data_val);
    int64_t w = _vimage_int(w_val, "save_raw", "width");
    int64_t h = _vimage_int(h_val, "save_raw", "height");
    int64_t c = _vimage_int(c_val, "save_raw", "channels");
    int64_t quality = _vimage_int(quality_val, "save_raw", "quality");

    if (w <= 0 || h <= 0 || c <= 0 || c > 4) return vyne_bool(false);
    if (data.size < w * h * c) return vyne_bool(false);
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;

    int64_t n = w * h * c;
    unsigned char* bytes = (unsigned char*)malloc((size_t)n);
    if (!bytes) return vyne_bool(false);

    for (int64_t i = 0; i < n; ++i) {
        double v = data.data[i];
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        bytes[i] = (unsigned char)(v * 255.0 + 0.5);
    }

    int ok = 0;
    if (_vimage_has_suffix(path, ".jpg") || _vimage_has_suffix(path, ".jpeg")) {
        ok = stbi_write_jpg(path, (int)w, (int)h, (int)c, bytes, (int)quality);
    } else if (_vimage_has_suffix(path, ".bmp")) {
        ok = stbi_write_bmp(path, (int)w, (int)h, (int)c, bytes);
    } else if (_vimage_has_suffix(path, ".tga")) {
        ok = stbi_write_tga(path, (int)w, (int)h, (int)c, bytes);
    } else {
        ok = stbi_write_png(
            path,
            (int)w,
            (int)h,
            (int)c,
            bytes,
            (int)(w * c)
        );
    }

    free(bytes);
    return vyne_bool(ok != 0);
}

/* ---------------------------------------------------------------- */
/* Resampling helpers                                                */
/* ---------------------------------------------------------------- */

/* Bilinear resample. HWC in, HWC out. */
static inline VyneValue vimage_resize_bilinear(
    VyneValue data_val,
    VyneValue w_val,
    VyneValue h_val,
    VyneValue c_val,
    VyneValue nw_val,
    VyneValue nh_val
) {
    VyneArray_f64 src = _vimage_array(data_val);
    int64_t w  = _vimage_int(w_val,  "resize_bilinear", "width");
    int64_t h  = _vimage_int(h_val,  "resize_bilinear", "height");
    int64_t c  = _vimage_int(c_val,  "resize_bilinear", "channels");
    int64_t nw = _vimage_int(nw_val, "resize_bilinear", "new_width");
    int64_t nh = _vimage_int(nh_val, "resize_bilinear", "new_height");

    if (w <= 0 || h <= 0 || c <= 0 || nw <= 0 || nh <= 0 ||
        src.size < w * h * c) {
        return vyne_array_f64_to_value(&src);
    }

    VyneArray_f64 out = vyne_array_f64_create(nw * nh * c);

    /* Map destination centres back to source coordinates. Using
     * (dst + 0.5) * src/dst - 0.5 avoids the half-pixel shift of the
     * naive mapping. */
    double sx = (double)w / (double)nw;
    double sy = (double)h / (double)nh;

    for (int64_t y = 0; y < nh; ++y) {
        double fy = ((double)y + 0.5) * sy - 0.5;
        if (fy < 0.0) fy = 0.0;
        if (fy > (double)(h - 1)) fy = (double)(h - 1);
        int64_t y0 = (int64_t)fy;
        int64_t y1 = (y0 + 1 < h) ? y0 + 1 : y0;
        double wy = fy - (double)y0;

        for (int64_t x = 0; x < nw; ++x) {
            double fx = ((double)x + 0.5) * sx - 0.5;
            if (fx < 0.0) fx = 0.0;
            if (fx > (double)(w - 1)) fx = (double)(w - 1);
            int64_t x0 = (int64_t)fx;
            int64_t x1 = (x0 + 1 < w) ? x0 + 1 : x0;
            double wx = fx - (double)x0;

            for (int64_t ch = 0; ch < c; ++ch) {
                double v00 = src.data[(y0 * w + x0) * c + ch];
                double v01 = src.data[(y0 * w + x1) * c + ch];
                double v10 = src.data[(y1 * w + x0) * c + ch];
                double v11 = src.data[(y1 * w + x1) * c + ch];
                double top = v00 * (1.0 - wx) + v01 * wx;
                double bot = v10 * (1.0 - wx) + v11 * wx;
                out.data[(y * nw + x) * c + ch] =
                    top * (1.0 - wy) + bot * wy;
            }
        }
    }

    return vyne_array_f64_to_value(&out);
}

/* Crop a rectangle. Coordinates are clamped to the image bounds. */
static inline VyneValue vimage_crop(
    VyneValue data_val,
    VyneValue w_val,
    VyneValue h_val,
    VyneValue c_val,
    VyneValue x_val,
    VyneValue y_val,
    VyneValue cw_val,
    VyneValue ch_val
) {
    VyneArray_f64 src = _vimage_array(data_val);
    int64_t w  = _vimage_int(w_val,  "crop", "width");
    int64_t h  = _vimage_int(h_val,  "crop", "height");
    int64_t c  = _vimage_int(c_val,  "crop", "channels");
    int64_t x0 = _vimage_int(x_val,  "crop", "x");
    int64_t y0 = _vimage_int(y_val,  "crop", "y");
    int64_t cw = _vimage_int(cw_val, "crop", "crop_width");
    int64_t ch = _vimage_int(ch_val, "crop", "crop_height");

    if (w <= 0 || h <= 0 || c <= 0 || cw <= 0 || ch <= 0 ||
        src.size < w * h * c) {
        return vyne_array_f64_to_value(&src);
    }
    if (x0 < 0) { cw += x0; x0 = 0; }
    if (y0 < 0) { ch += y0; y0 = 0; }
    if (x0 + cw > w) cw = w - x0;
    if (y0 + ch > h) ch = h - y0;
    if (cw <= 0 || ch <= 0) {
        VyneArray_f64 empty = vyne_array_f64_create(0);
        return vyne_array_f64_to_value(&empty);
    }

    VyneArray_f64 out = vyne_array_f64_create(cw * ch * c);
    for (int64_t y = 0; y < ch; ++y) {
        for (int64_t x = 0; x < cw; ++x) {
            for (int64_t k = 0; k < c; ++k) {
                out.data[(y * cw + x) * c + k] =
                    src.data[((y0 + y) * w + (x0 + x)) * c + k];
            }
        }
    }
    return vyne_array_f64_to_value(&out);
}

/* Rec.601 luma. Single-channel output. */
static inline VyneValue vimage_grayscale(
    VyneValue data_val,
    VyneValue w_val,
    VyneValue h_val,
    VyneValue c_val
) {
    VyneArray_f64 src = _vimage_array(data_val);
    int64_t w = _vimage_int(w_val, "grayscale", "width");
    int64_t h = _vimage_int(h_val, "grayscale", "height");
    int64_t c = _vimage_int(c_val, "grayscale", "channels");

    if (w <= 0 || h <= 0 || c <= 0 || src.size < w * h * c)
        return vyne_array_f64_to_value(&src);

    VyneArray_f64 out = vyne_array_f64_create(w * h);
    for (int64_t i = 0; i < w * h; ++i) {
        double v;
        if (c >= 3) {
            v = 0.299 * src.data[i * c + 0] +
                0.587 * src.data[i * c + 1] +
                0.114 * src.data[i * c + 2];
        } else {
            v = src.data[i * c];
        }
        out.data[i] = v;
    }
    return vyne_array_f64_to_value(&out);
}

#endif /* VYNE_VIMAGE_RT_H */
