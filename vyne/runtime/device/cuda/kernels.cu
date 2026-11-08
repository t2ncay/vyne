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
