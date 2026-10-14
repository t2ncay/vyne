# vml/Reductions.vy — scalar losses and metrics.
ruleset { dynamic_casting };

use "Types.vy";
use native vmath;
use external "vlin/vlin.vy";

module vml;

fn :: vml cross_entropy(pred :: vlin.Types.Matrix,
                        target :: vlin.Types.Matrix) -> Float64 {
    r :: Int64 := pred.row;
    n :: Int64 := r * pred.col;
    if r == 0 { return 0.0; }
    eps :: Float64 := 0.000000001;
    total :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        p :: Float64 := vmath.clamp(pred.data[i], eps, 1.0 - eps);
        y :: Float64 := target.data[i];
        total = total - (y * vmath.log(p) + (1.0 - y) * vmath.log(1.0 - p));
    };
    return total / float64(r);
}

fn :: vml mse(pred :: vlin.Types.Matrix,
              target :: vlin.Types.Matrix) -> Float64 {
    n :: Int64 := pred.row * pred.col;
    if n == 0 { return 0.0; }
    total :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := pred.data[i] - target.data[i];
        total = total + d * d;
    };
    return total / float64(n);
}

fn :: vml accuracy(pred :: vlin.Types.Matrix,
                   target :: vlin.Types.Matrix,
                   n :: Int64) -> Float64 {
    correct :: Int64 := 0;
    through i :: 0..n-1 -> loop {
        p :: Float64 := pred.data[i];
        a :: Float64 := target.data[i];
        pi :: Int64 := 0;
        ai :: Int64 := 0;
        if p > 0.5 { pi = 1; }
        if a > 0.5 { ai = 1; }
        if pi == ai { correct = correct + 1; }
    };
    return float64(correct) / float64(n);
}
