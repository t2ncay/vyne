# vlinalg/Reductions.vy — top-level reductions over a Matrix.

use "Types.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

fn :: vlinalg sum(m :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    total :: Float64 = 0.0;
    through i :: 0..n-1 -> loop {
        total = total + md[i];
    };
    return total;
}

fn :: vlinalg mean(m :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = m.row * m.col;
    if n == 0 { return 0.0; }
    md :: Array<Float64> = m.data;
    total :: Float64 = 0.0;
    through i :: 0..n-1 -> loop {
        total = total + md[i];
    };
    return total / float64(n);
}

fn :: vlinalg minimum(m :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = m.row * m.col;
    if n == 0 { return 0.0; }
    md :: Array<Float64> = m.data;
    best :: Float64 = md[0];
    through i :: 1..n-1 -> loop {
        v :: Float64 = md[i];
        if v < best { best = v; }
    };
    return best;
}

fn :: vlinalg maximum(m :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = m.row * m.col;
    if n == 0 { return 0.0; }
    md :: Array<Float64> = m.data;
    best :: Float64 = md[0];
    through i :: 1..n-1 -> loop {
        v :: Float64 = md[i];
        if v > best { best = v; }
    };
    return best;
}

fn :: vlinalg norm_fro(m :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    acc :: Float64 = 0.0;
    through i :: 0..n-1 -> loop {
        x :: Float64 = md[i];
        acc = acc + x * x;
    };
    return vmath.sqrt(acc);
}

fn :: vlinalg trace(m :: vlinalg.Types.Matrix) -> Float64 {
    r :: Int64 = m.row;
    c :: Int64 = m.col;
    md :: Array<Float64> = m.data;
    k :: Int64 = r;
    if c < k { k = c; }
    total :: Float64 = 0.0;
    through i :: 0..k-1 -> loop {
        total = total + md[i * c + i];
    };
    return total;
}