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
