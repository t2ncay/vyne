// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

/* vyne/runtime/modules/vconv.h
 * -------------------------------------------------------------------
 * Vyne runtime vconv module - transpiler target.
 *
 * CPU forward primitives for convolutional image models. This is the
 * numerical core behind the ESPCN inference example; the Vyne-side
 * `vconv` library wraps it with a Tensor type and validates shapes.
 *
 * Tensor convention
 * -----------------
 *   Tensors are stored HWC (row-major, channel-last):
 *     index = ((y * width) + x) * channels + c
 *
 *   Conv2d weights use PyTorch's OIHW layout:
 *     index = (((oc * in_c) + ic) * kh + ky) * kw + kx
 *
 *   That makes exporting a `torch.nn.Conv2d` state dict a straight
 *   reshape with no transposes. Bias is a length-out_c vector.
 *
 * Primitives
 * ----------
 *   conv2d_fwd         square-kernel convolution, stride/pad, optional bias
 *   relu_fwd           max(0, x)
 *   leaky_relu_fwd     x >= 0 ? x : alpha*x
 *   tanh_fwd           tanh(x)
 *   sigmoid_fwd        1 / (1 + exp(-x))
 *   pixel_shuffle_fwd  ESPCN depth-to-space, HWC equivalent of nn.PixelShuffle
 *   load_model         read a .vwy (binary) or .vwt (text) weight bundle
 *
 * This is deliberately a forward-only first slice. Backward passes,
 * dilation, groups, and device execution are tracked separately; the
 * first SR milestone only needs deterministic inference.
 * ------------------------------------------------------------------- */

#ifndef VYNE_VCONV_RT_H
#define VYNE_VCONV_RT_H

#include "../vyne_runtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- */
/* Argument helpers                                                  */
/* ---------------------------------------------------------------- */

static inline int64_t _vconv_int(VyneValue v, const char* fn, const char* arg) {
    if (v.type == V_INT64) return v.as.i64;
    if (v.type == V_FLOAT64 || v.type == V_FLOAT32) return (int64_t)v.as.f64;
    fprintf(stderr, "Runtime error: vconv.%s() expects an integer for %s.\n", fn, arg);
    exit(1);
}

static inline VyneArray_f64 _vconv_array(VyneValue v) {
    return vyne_value_to_array_f64(v);
}

static inline const char* _vconv_path(VyneValue v, const char* fn) {
    if (v.type != V_STRING || !v.as.str) {
        fprintf(stderr, "Runtime error: vconv.%s() expects a String path.\n", fn);
        exit(1);
    }
    return v.as.str;
}

static inline VyneValue _vconv_empty(void) {
    VyneArray_f64 empty = vyne_array_f64_create(0);
    return vyne_array_f64_to_value(&empty);
}

/* ---------------------------------------------------------------- */
/* Conv2d forward                                                    */
/* ---------------------------------------------------------------- */

static inline VyneValue vconv_conv2d_fwd(
    VyneValue input_val,
    VyneValue in_h_val,
    VyneValue in_w_val,
    VyneValue in_c_val,
    VyneValue weight_val,
    VyneValue out_c_val,
    VyneValue kh_val,
    VyneValue kw_val,
    VyneValue stride_val,
    VyneValue pad_val,
    VyneValue bias_val
) {
    VyneArray_f64 in  = _vconv_array(input_val);
    VyneArray_f64 wgt = _vconv_array(weight_val);

    int64_t in_h  = _vconv_int(in_h_val,  "conv2d_fwd", "in_height");
    int64_t in_w  = _vconv_int(in_w_val,  "conv2d_fwd", "in_width");
    int64_t in_c  = _vconv_int(in_c_val,  "conv2d_fwd", "in_channels");
    int64_t out_c = _vconv_int(out_c_val, "conv2d_fwd", "out_channels");
    int64_t kh    = _vconv_int(kh_val,    "conv2d_fwd", "kernel_height");
    int64_t kw    = _vconv_int(kw_val,    "conv2d_fwd", "kernel_width");
    int64_t stride = _vconv_int(stride_val, "conv2d_fwd", "stride");
    int64_t pad    = _vconv_int(pad_val,    "conv2d_fwd", "pad");

    if (in_h <= 0 || in_w <= 0 || in_c <= 0 ||
        out_c <= 0 || kh <= 0 || kw <= 0) {
        return _vconv_empty();
    }
    if (stride < 1) stride = 1;
    if (pad < 0) pad = 0;

    int64_t out_h = (in_h + 2 * pad - kh) / stride + 1;
    int64_t out_w = (in_w + 2 * pad - kw) / stride + 1;
    if (out_h <= 0 || out_w <= 0) return _vconv_empty();

    if (in.size < in_h * in_w * in_c) return _vconv_empty();
    if (wgt.size < out_c * in_c * kh * kw) return _vconv_empty();

    int has_bias = 0;
    VyneArray_f64 bias;
    memset(&bias, 0, sizeof(bias));
    if (bias_val.type != V_NULL) {
        bias = _vconv_array(bias_val);
        if (bias.size >= out_c) has_bias = 1;
    }

    VyneArray_f64 out = vyne_array_f64_create(out_h * out_w * out_c);

    for (int64_t oy = 0; oy < out_h; ++oy) {
        for (int64_t ox = 0; ox < out_w; ++ox) {
            int64_t out_base = (oy * out_w + ox) * out_c;

            for (int64_t oc = 0; oc < out_c; ++oc) {
                double acc = has_bias ? bias.data[oc] : 0.0;

                for (int64_t ic = 0; ic < in_c; ++ic) {
                    int64_t w_ci = ((oc * in_c) + ic) * kh * kw;
                    for (int64_t ky = 0; ky < kh; ++ky) {
                        int64_t iy = oy * stride - pad + ky;
                        if (iy < 0 || iy >= in_h) continue;
                        int64_t in_row = iy * in_w;
                        int64_t w_row = w_ci + ky * kw;

                        for (int64_t kx = 0; kx < kw; ++kx) {
                            int64_t ix = ox * stride - pad + kx;
                            if (ix < 0 || ix >= in_w) continue;

                            double x = in.data[(in_row + ix) * in_c + ic];
                            double w = wgt.data[w_row + kx];
                            acc += x * w;
                        }
                    }
                }

                out.data[out_base + oc] = acc;
            }
        }
    }

    return vyne_array_f64_to_value(&out);
}

/* ---------------------------------------------------------------- */
/* Elementwise activations                                          */
/* ---------------------------------------------------------------- */

static inline VyneValue vconv_relu_fwd(VyneValue x_val) {
    VyneArray_f64 x = _vconv_array(x_val);
    VyneArray_f64 out = vyne_array_f64_create(x.size);
    for (int64_t i = 0; i < x.size; ++i)
        out.data[i] = x.data[i] > 0.0 ? x.data[i] : 0.0;
    return vyne_array_f64_to_value(&out);
}

static inline VyneValue vconv_leaky_relu_fwd(VyneValue x_val, VyneValue alpha_val) {
    VyneArray_f64 x = _vconv_array(x_val);
    double alpha = (alpha_val.type == V_FLOAT64 || alpha_val.type == V_FLOAT32)
                       ? alpha_val.as.f64
                       : 0.01;
    VyneArray_f64 out = vyne_array_f64_create(x.size);
    for (int64_t i = 0; i < x.size; ++i)
        out.data[i] = x.data[i] >= 0.0 ? x.data[i] : alpha * x.data[i];
    return vyne_array_f64_to_value(&out);
}

static inline VyneValue vconv_tanh_fwd(VyneValue x_val) {
    VyneArray_f64 x = _vconv_array(x_val);
    VyneArray_f64 out = vyne_array_f64_create(x.size);
    for (int64_t i = 0; i < x.size; ++i)
        out.data[i] = tanh(x.data[i]);
    return vyne_array_f64_to_value(&out);
}

static inline VyneValue vconv_sigmoid_fwd(VyneValue x_val) {
    VyneArray_f64 x = _vconv_array(x_val);
    VyneArray_f64 out = vyne_array_f64_create(x.size);
    for (int64_t i = 0; i < x.size; ++i)
        out.data[i] = 1.0 / (1.0 + exp(-x.data[i]));
    return vyne_array_f64_to_value(&out);
}

/* ---------------------------------------------------------------- */
/* Pixel shuffle (ESPCN depth-to-space)                             */
/* ---------------------------------------------------------------- */

/* Input:  [h][w][c * r * r]   (HWC)
 * Output: [h*r][w*r][c]
 *
 * For output position (y*r + i, x*r + j) and output channel oc, the
 * source is input[y][x][oc*r*r + i*r + j]. This is the HWC equivalent
 * of PyTorch's PixelShuffle over a CHW tensor. */
static inline VyneValue vconv_pixel_shuffle_fwd(
    VyneValue input_val,
    VyneValue h_val,
    VyneValue w_val,
    VyneValue c_val,
    VyneValue r_val
) {
    VyneArray_f64 in = _vconv_array(input_val);
    int64_t h = _vconv_int(h_val, "pixel_shuffle_fwd", "height");
    int64_t w = _vconv_int(w_val, "pixel_shuffle_fwd", "width");
    int64_t c = _vconv_int(c_val, "pixel_shuffle_fwd", "channels");
    int64_t r = _vconv_int(r_val, "pixel_shuffle_fwd", "scale");

    if (h <= 0 || w <= 0 || c <= 0 || r <= 0) return _vconv_empty();
    int64_t rr = r * r;
    if (c % rr != 0) return _vconv_empty();

    int64_t out_c = c / rr;
    int64_t out_h = h * r;
    int64_t out_w = w * r;
    if (in.size < h * w * c) return _vconv_empty();

    VyneArray_f64 out = vyne_array_f64_create(out_h * out_w * out_c);

    for (int64_t y = 0; y < h; ++y) {
        for (int64_t x = 0; x < w; ++x) {
            int64_t in_base = (y * w + x) * c;
            for (int64_t i = 0; i < r; ++i) {
                for (int64_t j = 0; j < r; ++j) {
                    int64_t out_base =
                        ((y * r + i) * out_w + (x * r + j)) * out_c;
                    for (int64_t oc = 0; oc < out_c; ++oc) {
                        out.data[out_base + oc] =
                            in.data[in_base + oc * rr + i * r + j];
                    }
                }
            }
        }
    }

    return vyne_array_f64_to_value(&out);
}

/* ---------------------------------------------------------------- */
/* Weight bundles                                                   */
/* ---------------------------------------------------------------- */
/* Two on-disk formats are accepted:
 *
 * Binary .vwy (preferred; what scripts/export_espcn_weights.py emits)
 *   char[4]  "VYW1"
 *   u32      tensor_count
 *   repeat tensor_count times:
 *     u32    name_length
 *     u8[]   name (no NUL)
 *     u32    ndim
 *     u32[]  dims (ndim entries)
 *     f32[]  values (product(dims) entries)
 *
 * Text .vwt (handy for tests and hand-authored fixtures)
 *   VYWT1
 *   <tensor_count>
 *   <name> <ndim> <d0> <d1> ... <v0> <v1> ...
 *
 * All integers/floats in the binary format are little-endian. f32
 * values are widened to Float64 after load. */

static inline uint32_t _vconv_le_u32(const unsigned char* p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline float _vconv_le_f32(const unsigned char* p) {
    uint32_t bits = _vconv_le_u32(p);
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static inline unsigned char* _vconv_read_file(
    const char* path,
    size_t* out_size
) {
    *out_size = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long end = ftell(f);
    if (end < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    size_t size = (size_t)end;
    unsigned char* buf = (unsigned char*)malloc(size ? size : 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, size, f);
    fclose(f);
    if (got != size) {
        free(buf);
        return NULL;
    }
    *out_size = size;
    return buf;
}

static inline VyneValue _vconv_empty_model(void) {
    return vyne_map_create();
}

static inline VyneValue _vconv_load_binary(
    const unsigned char* buf,
    size_t size
) {
    if (size < 8) return _vconv_empty_model();

    size_t pos = 4;
    uint32_t count = _vconv_le_u32(buf + pos);
    pos += 4;

    VyneValue map = vyne_map_create();

    for (uint32_t t = 0; t < count; ++t) {
        if (pos + 4 > size) return _vconv_empty_model();
        uint32_t name_len = _vconv_le_u32(buf + pos);
        pos += 4;
        if (name_len == 0 || name_len > 4096) return _vconv_empty_model();
        if (pos + name_len > size) return _vconv_empty_model();

        char* name = (char*)arena_alloc((size_t)name_len + 1);
        memcpy(name, buf + pos, name_len);
        name[name_len] = '\0';
        pos += name_len;

        if (pos + 4 > size) return _vconv_empty_model();
        uint32_t ndim = _vconv_le_u32(buf + pos);
        pos += 4;
        if (ndim == 0 || ndim > 8) return _vconv_empty_model();
        if (pos + (size_t)ndim * 4 > size) return _vconv_empty_model();

        int64_t n = 1;
        for (uint32_t d = 0; d < ndim; ++d) {
            uint32_t dim = _vconv_le_u32(buf + pos);
            pos += 4;
            n *= (int64_t)dim;
        }

        /* Reject counts that cannot fit in the remaining bytes before
         * doing any multiplication that could overflow size_t. */
        if (n <= 0 || (uint64_t)n > (uint64_t)((size - pos) / 4))
            return _vconv_empty_model();

        VyneArray_f64 arr = vyne_array_f64_create(n);
        for (int64_t i = 0; i < n; ++i) {
            arr.data[i] = (double)_vconv_le_f32(buf + pos);
            pos += 4;
        }

        vyne_map_set(map, vyne_string(name), vyne_array_f64_to_value(&arr));
    }

    return map;
}

static inline VyneValue _vconv_load_text(FILE* f) {
    char magic[8];
    if (fscanf(f, "%7s", magic) != 1) return _vconv_empty_model();
    if (strcmp(magic, "VYWT1") != 0) return _vconv_empty_model();

    int count = 0;
    if (fscanf(f, "%d", &count) != 1 || count < 0) return _vconv_empty_model();

    VyneValue map = vyne_map_create();

    for (int t = 0; t < count; ++t) {
        char name[256];
        int ndim = 0;
        if (fscanf(f, "%255s %d", name, &ndim) != 2) return _vconv_empty_model();
        if (ndim <= 0 || ndim > 8) return _vconv_empty_model();

        int64_t n = 1;
        for (int d = 0; d < ndim; ++d) {
            int dim = 0;
            if (fscanf(f, "%d", &dim) != 1 || dim <= 0)
                return _vconv_empty_model();
            n *= (int64_t)dim;
        }

        VyneArray_f64 arr = vyne_array_f64_create(n);
        for (int64_t i = 0; i < n; ++i) {
            double v = 0.0;
            if (fscanf(f, "%lf", &v) != 1) return _vconv_empty_model();
            arr.data[i] = v;
        }

        vyne_map_set(map, vyne_string(name), vyne_array_f64_to_value(&arr));
    }

    return map;
}

static inline VyneValue vconv_load_model(VyneValue path_val) {
    const char* path = _vconv_path(path_val, "load_model");

    FILE* probe = fopen(path, "rb");
    if (!probe) return _vconv_empty_model();

    unsigned char head[5] = {0, 0, 0, 0, 0};
    size_t got = fread(head, 1, 5, probe);
    fclose(probe);
    if (got < 4) return _vconv_empty_model();

    if (memcmp(head, "VYWT1", 5) == 0) {
        FILE* f = fopen(path, "r");
        if (!f) return _vconv_empty_model();
        VyneValue map = _vconv_load_text(f);
        fclose(f);
        return map;
    }

    if (memcmp(head, "VYW1", 4) == 0) {
        size_t size = 0;
        unsigned char* buf = _vconv_read_file(path, &size);
        if (!buf) return _vconv_empty_model();
        VyneValue map = _vconv_load_binary(buf, size);
        free(buf);
        return map;
    }

    return _vconv_empty_model();
}

#endif /* VYNE_VCONV_RT_H */
