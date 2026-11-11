// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// CUDA device kernels for the Vyne @device backend.
//
// These are compiled to PTX by scripts/build_cuda_ptx.ps1 and embedded in
// runtime/detail/vyne_cuda_ptx.h. The driver API loads the PTX module at
// runtime, so the generated Vyne C does not link against CUDA directly.
//
// Data is Float64 throughout. Matmul is a 16x16 shared-memory tile.

extern "C" __global__ void vec_add_f64(double* c, const double* a,
                                       const double* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

extern "C" __global__ void vec_sub_f64(double* c, const double* a,
                                       const double* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] - b[i];
}

extern "C" __global__ void vec_mul_f64(double* c, const double* a,
                                       const double* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] * b[i];
}

extern "C" __global__ void vec_scale_f64(double* c, const double* a,
                                         double s, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] * s;
}

extern "C" __global__ void vec_relu_f64(double* c, const double* a,
                                        long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] > 0.0 ? a[i] : 0.0;
}

extern "C" __global__ void vec_fill_f64(double* c, double v, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = v;
}

#define VYNE_TILE 16

// C[M,N] = A[M,K] * B[K,N], all row-major.
extern "C" __global__ void matmul_f64(double* C, const double* A,
                                      const double* B, int M, int N, int K) {
    __shared__ double As[VYNE_TILE][VYNE_TILE];
    __shared__ double Bs[VYNE_TILE][VYNE_TILE];

    int row = blockIdx.y * VYNE_TILE + threadIdx.y;
    int col = blockIdx.x * VYNE_TILE + threadIdx.x;
    double sum = 0.0;

    int tiles = (K + VYNE_TILE - 1) / VYNE_TILE;
    for (int t = 0; t < tiles; ++t) {
        int aRow = row;
        int aCol = t * VYNE_TILE + threadIdx.x;
        As[threadIdx.y][threadIdx.x] =
            (aRow < M && aCol < K) ? A[aRow * K + aCol] : 0.0;

        int bRow = t * VYNE_TILE + threadIdx.y;
        int bCol = col;
        Bs[threadIdx.y][threadIdx.x] =
            (bRow < K && bCol < N) ? B[bRow * N + bCol] : 0.0;

        __syncthreads();
        for (int i = 0; i < VYNE_TILE; ++i)
            sum += As[threadIdx.y][i] * Bs[i][threadIdx.x];
        __syncthreads();
    }

    if (row < M && col < N) C[row * N + col] = sum;
}

// ============================================================================
// Float32 kernels (the default device dtype)
// ============================================================================

extern "C" __global__ void vec_add_f32(float* c, const float* a,
                                       const float* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

extern "C" __global__ void vec_sub_f32(float* c, const float* a,
                                       const float* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] - b[i];
}

extern "C" __global__ void vec_mul_f32(float* c, const float* a,
                                       const float* b, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] * b[i];
}

extern "C" __global__ void vec_scale_f32(float* c, const float* a,
                                         float s, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] * s;
}

extern "C" __global__ void vec_relu_f32(float* c, const float* a,
                                        long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] > 0.0f ? a[i] : 0.0f;
}

extern "C" __global__ void vec_fill_f32(float* c, float v, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = v;
}

extern "C" __global__ void reduce_sum_f32(float* out, const float* in,
                                          long long n) {
    __shared__ float sh[256];
    int tid = threadIdx.x;
    float acc = 0.0f;
    for (long long i = tid; i < n; i += blockDim.x) acc += in[i];
    sh[tid] = acc;
    __syncthreads();
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) sh[tid] += sh[tid + stride];
        __syncthreads();
    }
    if (tid == 0) out[0] = sh[0];
}

extern "C" __global__ void reduce_sum_f64(double* out, const double* in,
                                          long long n) {
    __shared__ double sh[256];
    int tid = threadIdx.x;
    double acc = 0.0;
    for (long long i = tid; i < n; i += blockDim.x) acc += in[i];
    sh[tid] = acc;
    __syncthreads();
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) sh[tid] += sh[tid + stride];
        __syncthreads();
    }
    if (tid == 0) out[0] = sh[0];
}

extern "C" __global__ void matmul_f32(float* C, const float* A,
                                      const float* B, int M, int N, int K) {
    __shared__ float As[VYNE_TILE][VYNE_TILE];
    __shared__ float Bs[VYNE_TILE][VYNE_TILE];

    int row = blockIdx.y * VYNE_TILE + threadIdx.y;
    int col = blockIdx.x * VYNE_TILE + threadIdx.x;
    float sum = 0.0f;

    int tiles = (K + VYNE_TILE - 1) / VYNE_TILE;
    for (int t = 0; t < tiles; ++t) {
        int aRow = row;
        int aCol = t * VYNE_TILE + threadIdx.x;
        As[threadIdx.y][threadIdx.x] =
            (aRow < M && aCol < K) ? A[aRow * K + aCol] : 0.0f;

        int bRow = t * VYNE_TILE + threadIdx.y;
        int bCol = col;
        Bs[threadIdx.y][threadIdx.x] =
            (bRow < K && bCol < N) ? B[bRow * N + bCol] : 0.0f;

        __syncthreads();
        for (int i = 0; i < VYNE_TILE; ++i)
            sum += As[threadIdx.y][i] * Bs[i][threadIdx.x];
        __syncthreads();
    }

    if (row < M && col < N) C[row * N + col] = sum;
}

// C[H,W,out_c] = conv2d(input[H,W,in_c], weight[out_c,in_c,k,k]), HWC layout.
// One thread per output element. Zero padding, square kernel.
extern "C" __global__ void conv2d_f32(
    float* C, const float* X, const float* W, const float* B,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad, int has_bias) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_h * out_w * out_c;
    if (idx >= total) return;

    int oc = idx % out_c;
    int t = idx / out_c;
    int ox = t % out_w;
    int oy = t / out_w;
    float acc = has_bias ? B[oc] : 0.0f;

    for (int ic = 0; ic < in_c; ++ic) {
        int wb = ((oc * in_c) + ic) * k * k;
        for (int ky = 0; ky < k; ++ky) {
            int iy = oy * stride - pad + ky;
            if (iy < 0 || iy >= in_h) continue;
            for (int kx = 0; kx < k; ++kx) {
                int ix = ox * stride - pad + kx;
                if (ix < 0 || ix >= in_w) continue;
                acc += X[(iy * in_w + ix) * in_c + ic] * W[wb + ky * k + kx];
            }
        }
    }
    C[idx] = acc;
}

extern "C" __global__ void conv2d_f64(
    double* C, const double* X, const double* W, const double* B,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad, int has_bias) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_h * out_w * out_c;
    if (idx >= total) return;

    int oc = idx % out_c;
    int t = idx / out_c;
    int ox = t % out_w;
    int oy = t / out_w;
    double acc = has_bias ? B[oc] : 0.0;

    for (int ic = 0; ic < in_c; ++ic) {
        int wb = ((oc * in_c) + ic) * k * k;
        for (int ky = 0; ky < k; ++ky) {
            int iy = oy * stride - pad + ky;
            if (iy < 0 || iy >= in_h) continue;
            for (int kx = 0; kx < k; ++kx) {
                int ix = ox * stride - pad + kx;
                if (ix < 0 || ix >= in_w) continue;
                acc += X[(iy * in_w + ix) * in_c + ic] * W[wb + ky * k + kx];
            }
        }
    }
    C[idx] = acc;
}

// ============================================================================
// conv2d backward kernels (Float32 + Float64)
//
// All three use the "gather" formulation: one thread owns one unique output
// element and sums every contribution into it. This is the exact reverse of
// the forward "one thread per output" mapping, so no atomics are needed and
// the result is bit-for-bit deterministic across schedules.
//
// Layouts match the forward path: HWC activations, OIHW weights, square
// kernel, zero padding.
//
//   backward_input  : grad_X[in_h,in_w,in_c]  from grad_out[out_h,out_w,out_c]
//   backward_weight : grad_W[out_c,in_c,k,k]  from input + grad_out
//   backward_bias   : grad_B[out_c] = sum_{oy,ox} grad_out[oy,ox,oc]
// ============================================================================

extern "C" __global__ void conv2d_backward_input_f32(
    float* grad_X, const float* grad_out, const float* W,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = in_h * in_w * in_c;
    if (idx >= total) return;

    int ic = idx % in_c;
    int t = idx / in_c;
    int ix = t % in_w;
    int iy = t / in_w;

    float acc = 0.0f;
    for (int oc = 0; oc < out_c; ++oc) {
        for (int ky = 0; ky < k; ++ky) {
            int oy_num = iy + pad - ky;
            if (oy_num < 0 || (oy_num % stride) != 0) continue;
            int oy = oy_num / stride;
            if (oy >= out_h) continue;
            for (int kx = 0; kx < k; ++kx) {
                int ox_num = ix + pad - kx;
                if (ox_num < 0 || (ox_num % stride) != 0) continue;
                int ox = ox_num / stride;
                if (ox >= out_w) continue;
                acc += grad_out[(oy * out_w + ox) * out_c + oc] *
                       W[((oc * in_c) + ic) * k * k + ky * k + kx];
            }
        }
    }
    grad_X[idx] = acc;
}

extern "C" __global__ void conv2d_backward_input_f64(
    double* grad_X, const double* grad_out, const double* W,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = in_h * in_w * in_c;
    if (idx >= total) return;

    int ic = idx % in_c;
    int t = idx / in_c;
    int ix = t % in_w;
    int iy = t / in_w;

    double acc = 0.0;
    for (int oc = 0; oc < out_c; ++oc) {
        for (int ky = 0; ky < k; ++ky) {
            int oy_num = iy + pad - ky;
            if (oy_num < 0 || (oy_num % stride) != 0) continue;
            int oy = oy_num / stride;
            if (oy >= out_h) continue;
            for (int kx = 0; kx < k; ++kx) {
                int ox_num = ix + pad - kx;
                if (ox_num < 0 || (ox_num % stride) != 0) continue;
                int ox = ox_num / stride;
                if (ox >= out_w) continue;
                acc += grad_out[(oy * out_w + ox) * out_c + oc] *
                       W[((oc * in_c) + ic) * k * k + ky * k + kx];
            }
        }
    }
    grad_X[idx] = acc;
}

extern "C" __global__ void conv2d_backward_weight_f32(
    float* grad_W, const float* X, const float* grad_out,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_c * in_c * k * k;
    if (idx >= total) return;

    int kx = idx % k;
    int t1 = idx / k;
    int ky = t1 % k;
    int t2 = t1 / k;
    int ic = t2 % in_c;
    int oc = t2 / in_c;

    float acc = 0.0f;
    for (int oy = 0; oy < out_h; ++oy) {
        int iy = oy * stride - pad + ky;
        if (iy < 0 || iy >= in_h) continue;
        for (int ox = 0; ox < out_w; ++ox) {
            int ix = ox * stride - pad + kx;
            if (ix < 0 || ix >= in_w) continue;
            acc += grad_out[(oy * out_w + ox) * out_c + oc] *
                   X[(iy * in_w + ix) * in_c + ic];
        }
    }
    grad_W[idx] = acc;
}

extern "C" __global__ void conv2d_backward_weight_f64(
    double* grad_W, const double* X, const double* grad_out,
    int in_h, int in_w, int in_c,
    int out_h, int out_w, int out_c,
    int k, int stride, int pad) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_c * in_c * k * k;
    if (idx >= total) return;

    int kx = idx % k;
    int t1 = idx / k;
    int ky = t1 % k;
    int t2 = t1 / k;
    int ic = t2 % in_c;
    int oc = t2 / in_c;

    double acc = 0.0;
    for (int oy = 0; oy < out_h; ++oy) {
        int iy = oy * stride - pad + ky;
        if (iy < 0 || iy >= in_h) continue;
        for (int ox = 0; ox < out_w; ++ox) {
            int ix = ox * stride - pad + kx;
            if (ix < 0 || ix >= in_w) continue;
            acc += grad_out[(oy * out_w + ox) * out_c + oc] *
                   X[(iy * in_w + ix) * in_c + ic];
        }
    }
    grad_W[idx] = acc;
}

extern "C" __global__ void conv2d_backward_bias_f32(
    float* grad_B, const float* grad_out,
    int out_h, int out_w, int out_c) {
    int oc = blockIdx.x * blockDim.x + threadIdx.x;
    if (oc >= out_c) return;

    float acc = 0.0f;
    for (int oy = 0; oy < out_h; ++oy)
        for (int ox = 0; ox < out_w; ++ox)
            acc += grad_out[(oy * out_w + ox) * out_c + oc];
    grad_B[oc] = acc;
}

extern "C" __global__ void conv2d_backward_bias_f64(
    double* grad_B, const double* grad_out,
    int out_h, int out_w, int out_c) {
    int oc = blockIdx.x * blockDim.x + threadIdx.x;
    if (oc >= out_c) return;

    double acc = 0.0;
    for (int oy = 0; oy < out_h; ++oy)
        for (int ox = 0; ox < out_w; ++ox)
            acc += grad_out[(oy * out_w + ox) * out_c + oc];
    grad_B[oc] = acc;
}

// ReLU backward: out[i] = pre[i] > 0 ? grad[i] : 0.
extern "C" __global__ void vec_relu_grad_f32(float* out, const float* grad,
                                             const float* pre, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) out[i] = pre[i] > 0.0f ? grad[i] : 0.0f;
}

extern "C" __global__ void vec_relu_grad_f64(double* out, const double* grad,
                                             const double* pre, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) out[i] = pre[i] > 0.0 ? grad[i] : 0.0;
}

// ESPCN depth-to-space, HWC layout. One thread per output element.
//   input  : [h][w][c*r*r]
//   output : [h*r][w*r][c]  with  out[y*r+i][x*r+j][oc] = in[y][x][oc*r*r+i*r+j]
extern "C" __global__ void pixel_shuffle_f32(
    float* out, const float* in,
    int h, int w, int c, int r) {
    int rr = r * r;
    int out_c = c / rr;
    int out_h = h * r;
    int out_w = w * r;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_h * out_w * out_c;
    if (idx >= total) return;

    int oc = idx % out_c;
    int t = idx / out_c;
    int oxt = t % out_w;
    int oy = t / out_w;
    int i = oy % r;
    int y = oy / r;
    int j = oxt % r;
    int x = oxt / r;
    out[idx] = in[((y * w + x) * c) + oc * rr + i * r + j];
}

extern "C" __global__ void pixel_shuffle_f64(
    double* out, const double* in,
    int h, int w, int c, int r) {
    int rr = r * r;
    int out_c = c / rr;
    int out_h = h * r;
    int out_w = w * r;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_h * out_w * out_c;
    if (idx >= total) return;

    int oc = idx % out_c;
    int t = idx / out_c;
    int oxt = t % out_w;
    int oy = t / out_w;
    int i = oy % r;
    int y = oy / r;
    int j = oxt % r;
    int x = oxt / r;
    out[idx] = in[((y * w + x) * c) + oc * rr + i * r + j];
}

// Inverse gradient for ESPCN depth-to-space. One thread owns one input-gradient
// element and gathers from exactly one output-gradient element.
extern "C" __global__ void pixel_shuffle_backward_f32(
    float* grad_in, const float* grad_out,
    int h, int w, int c, int r) {
    int rr = r * r;
    int out_c = c / rr;
    int out_w = w * r;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = h * w * c;
    if (idx >= total) return;

    int sc = idx % c;
    int t = idx / c;
    int x = t % w;
    int y = t / w;
    int oc = sc / rr;
    int rem = sc % rr;
    int i = rem / r;
    int j = rem % r;
    grad_in[idx] = grad_out[((y * r + i) * out_w + (x * r + j)) * out_c + oc];
}

extern "C" __global__ void pixel_shuffle_backward_f64(
    double* grad_in, const double* grad_out,
    int h, int w, int c, int r) {
    int rr = r * r;
    int out_c = c / rr;
    int out_w = w * r;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = h * w * c;
    if (idx >= total) return;

    int sc = idx % c;
    int t = idx / c;
    int x = t % w;
    int y = t / w;
    int oc = sc / rr;
    int rem = sc % rr;
    int i = rem / r;
    int j = rem % r;
    grad_in[idx] = grad_out[((y * r + i) * out_w + (x * r + j)) * out_c + oc];
}

// MSE training helper. `partial[i]` is the per-element contribution to the mean
// loss, so callers can download and sum it for reporting while keeping `grad`
// device-resident for backpropagation.
extern "C" __global__ void mse_grad_f32(
    float* grad, float* partial, const float* pred, const float* target,
    long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float d = pred[i] - target[i];
    float inv = n > 0 ? 1.0f / (float)n : 0.0f;
    grad[i] = 2.0f * d * inv;
    partial[i] = d * d * inv;
}

extern "C" __global__ void mse_grad_f64(
    double* grad, double* partial, const double* pred, const double* target,
    long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    double d = pred[i] - target[i];
    double inv = n > 0 ? 1.0 / (double)n : 0.0;
    grad[i] = 2.0 * d * inv;
    partial[i] = d * d * inv;
}

extern "C" __global__ void sgd_step_f32(float* param, const float* grad,
                                        float lr, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) param[i] -= lr * grad[i];
}

extern "C" __global__ void sgd_step_f64(double* param, const double* grad,
                                        double lr, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) param[i] -= lr * grad[i];
}

extern "C" __global__ void adamw_step_f32(
    float* param, float* m, float* v, const float* grad,
    float lr, float beta1, float beta2, float eps, float weight_decay,
    int step, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float g = grad[i];
    float mi = beta1 * m[i] + (1.0f - beta1) * g;
    float vi = beta2 * v[i] + (1.0f - beta2) * g * g;
    m[i] = mi;
    v[i] = vi;
    float bc1 = 1.0f - powf(beta1, (float)step);
    float bc2 = 1.0f - powf(beta2, (float)step);
    float mh = mi / bc1;
    float vh = vi / bc2;
    float update = mh / (sqrtf(vh) + eps) + weight_decay * param[i];
    param[i] -= lr * update;
}

extern "C" __global__ void adamw_step_f64(
    double* param, double* m, double* v, const double* grad,
    double lr, double beta1, double beta2, double eps, double weight_decay,
    int step, long long n) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    double g = grad[i];
    double mi = beta1 * m[i] + (1.0 - beta1) * g;
    double vi = beta2 * v[i] + (1.0 - beta2) * g * g;
    m[i] = mi;
    v[i] = vi;
    double bc1 = 1.0 - pow(beta1, (double)step);
    double bc2 = 1.0 - pow(beta2, (double)step);
    double mh = mi / bc1;
    double vh = vi / bc2;
    double update = mh / (sqrt(vh) + eps) + weight_decay * param[i];
    param[i] -= lr * update;
}
