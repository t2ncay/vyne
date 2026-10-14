# vlin/Optimizers.vy — in-place parameter update rules.

use "Types.vy";

ruleset { dynamic_casting };

module vlin;

# W -= lr * grad, element-wise, in place. W must live outside any
# region that gets rewound between calls — the mutation is what makes
# training work; a rebinding assignment would silently write into an
# arena slot that the caller never sees.
fn :: vlin sgd_update_inplace(W :: vlin.Types.Matrix,
                              grad :: vlin.Types.Matrix,
                              lr :: Float64) -> Int64 {
    n :: Int64 := W.row * W.col;
    through i :: 0..n-1 -> loop {
        W.data[i] = W.data[i] - lr * grad.data[i];
    };
    return 0;
}
