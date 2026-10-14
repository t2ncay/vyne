# benchmarks/matmul_1024_vlin.vy — matmul through vlin, exercises the
# BLAS dispatch path when --blas is on.
#
# Same source, three ways to run:
#   vynec --compile matmul_1024_vlin.vy                    → emitted C triple loop
#   vynec --compile matmul_1024_vlin.vy --blas             → cblas_dgemm
#   vynec --compile matmul_1024_vlin.vy --blas --native    → cblas_dgemm + AVX host code
#
# The point of the --blas run is that the source is unchanged. The flag
# flips the lowering of vlin.multiply from the emitted C triple loop
# to cblas_dgemm. Same checksum, different kernel.

ruleset { dynamic_casting };

use external "vlin/vlin.vy";
use native vmath;
use native vmem;

vmath.seed(42);

N     :: Int64 := 1024;
ITERS :: Int64 := 100;
CONFIG :: Int64 := 1;   # 0 = no region, 1 = region

out("config=" + string(CONFIG) + " N=" + string(N) + " iters=" + string(ITERS));

# ---------------------------------------------------------------------------
# Build A and B once, at top level.
#
# Both live outside every region below, so their storage survives every
# rewind. Only the output buffer that vlin.multiply allocates internally
# is scoped to the region in config 1.
#
# The loop writes A and B in row-major order with the same RNG call
# sequence the hand-rolled benchmark uses: A[0,0], B[0,0], A[0,1], B[0,1],
# ... So the resulting matrices are bit-identical to the ones in
# matmul_1024.vy.
# ---------------------------------------------------------------------------
A_flat :: Array<Float64> := vlin.zeros_f64(N * N);
B_flat :: Array<Float64> := vlin.zeros_f64(N * N);

through r :: 0..N-1 -> loop {
    through c :: 0..N-1 -> loop {
        A_flat[r * N + c] = vmath.random_float(-1.0, 1.0);
        B_flat[r * N + c] = vmath.random_float(-1.0, 1.0);
    };
};

A :: vlin.Types.Matrix := vlin.Types.Matrix(N, N, A_flat);
B :: vlin.Types.Matrix := vlin.Types.Matrix(N, N, B_flat);

# ---------------------------------------------------------------------------
# Config 0 — no region.
#
# Every vlin.multiply allocates a fresh output buffer that stays alive
# until process exit. The arena grows by ITERS × (N·N doubles), which
# for N=1024 and ITERS=100 is ~840 MB.
# ---------------------------------------------------------------------------
if CONFIG == 0 {
    through iter :: 1..ITERS -> loop {
        C :: vlin.Types.Matrix := vlin.multiply(A, B);
        if iter == ITERS {
            out("checksum: " + string(C.get(0, 0)));
        }
    };
}

# ---------------------------------------------------------------------------
# Config 1 — region-wrapped.
#
# Every allocation inside the region — including the output buffer that
# vlin.multiply allocates internally, deep in Ops.vy — is freed at the
# region's closing brace. Peak RSS stays flat regardless of ITERS.
#
# The checkpoint and the rewind are global arena operations. They don't
# care that the allocation happened inside a called function; if it was
# made after the checkpoint, it's freed at the rewind.
# ---------------------------------------------------------------------------
if CONFIG == 1 {
    through iter :: 1..ITERS -> loop {
        region step {
            C :: vlin.Types.Matrix := vlin.multiply(A, B);
            if iter == ITERS {
                out("checksum: " + string(C.get(0, 0)));
            }
        };
    };
}
