# vdecomp_bench.vy - dense factorisation throughput.
#
#   ./vynec --compile examples/libs/vdecomp_bench.vy
#   ./examples/libs/vdecomp_bench

use external "vdecomp/vdecomp.vy";
use native vcore;

out("vdecomp benchmark");

fn spd_matrix(n :: Int64) -> vdecomp.Matrix {
    data :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through i :: 0..n-1 -> loop {
        through j :: 0..n-1 -> loop {
            if i == j {
                data[i * n + j] = float64(n) + 2.0;
            } else {
                data[i * n + j] = 1.0;
            }
        };
    };
    return vdecomp.Matrix(data, n, n);
}

fn run(n :: Int64, iters :: Int64) {
    M :: vdecomp.Matrix := spd_matrix(n);
    acc :: Float64 := 0.0;

    t0 :: Int64 := vcore.now_ns();
    through k :: 0..iters-1 -> loop {
        r := vdecomp.lu_matrix(M);
        acc = acc + r.det;
    };
    t1 :: Int64 := vcore.now_ns();
    out("lu " + string(n) + "x" + string(n) + " x " + string(iters) + " : " + string((t1 - t0) / 1000000) + " ms");

    t2 :: Int64 := vcore.now_ns();
    through k :: 0..iters-1 -> loop {
        q := vdecomp.qr_matrix(M);
        acc = acc + q.r[0];
    };
    t3 :: Int64 := vcore.now_ns();
    out("qr " + string(n) + "x" + string(n) + " x " + string(iters) + " : " + string((t3 - t2) / 1000000) + " ms");

    t4 :: Int64 := vcore.now_ns();
    through k :: 0..iters-1 -> loop {
        c := vdecomp.cholesky_matrix(M);
        acc = acc + c.l[0];
    };
    t5 :: Int64 := vcore.now_ns();
    out("chol " + string(n) + "x" + string(n) + " x " + string(iters) + " : " + string((t5 - t4) / 1000000) + " ms");

    t6 :: Int64 := vcore.now_ns();
    through k :: 0..iters-1 -> loop {
        e := vdecomp.eigen_matrix(M);
        acc = acc + e.values[0];
    };
    t7 :: Int64 := vcore.now_ns();
    out("eigen " + string(n) + "x" + string(n) + " x " + string(iters) + " : " + string((t7 - t6) / 1000000) + " ms");

    out("checksum " + string(acc));
}

run(64, 20);
run(128, 10);
out("done.");
