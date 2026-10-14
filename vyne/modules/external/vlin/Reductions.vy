# vlin/Reductions.vy — scalar results over a Matrix.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vlin;
use native vmath;

fn :: vlin sum(m :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := m.row * m.col;
    return vlin.k_sum(m.data, n);
}

fn :: vlin mean(m :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := m.row * m.col;
    if n == 0 { return 0.0; }
    return vlin.k_sum(m.data, n) / float64(n);
}

fn :: vlin minimum(m :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := m.row * m.col;
    return vlin.k_min(m.data, n);
}

fn :: vlin maximum(m :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := m.row * m.col;
    return vlin.k_max(m.data, n);
}

fn :: vlin norm_fro(m :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := m.row * m.col;
    return vmath.sqrt(vlin.k_norm_sq(m.data, n));
}

fn :: vlin trace(m :: vlin.Types.Matrix) -> Float64 {
    r :: Int64 := m.row;
    c :: Int64 := m.col;
    k :: Int64 := r;
    if c < k { k = c; }
    total :: Float64 := 0.0;
    through i :: 0..k-1 -> loop {
        total = total + m.data[i * c + i];
    };
    return total;
}

fn :: vlin argmax(m :: vlin.Types.Matrix) -> Int64 {
    n :: Int64 := m.row * m.col;
    if n == 0 { return 0; }
    best :: Float64 := m.data[0];
    best_i :: Int64 := 0;
    through i :: 1..n-1 -> loop {
        v :: Float64 := m.data[i];
        if v > best { best = v; best_i = i; }
    };
    return best_i;
}

fn :: vlin argmin(m :: vlin.Types.Matrix) -> Int64 {
    n :: Int64 := m.row * m.col;
    if n == 0 { return 0; }
    best :: Float64 := m.data[0];
    best_i :: Int64 := 0;
    through i :: 1..n-1 -> loop {
        v :: Float64 := m.data[i];
        if v < best { best = v; best_i = i; }
    };
    return best_i;
}
