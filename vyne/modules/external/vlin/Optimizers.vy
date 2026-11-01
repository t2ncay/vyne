# vlin/Optimizers.vy — in-place parameter update rules.

use "Types.vy";

ruleset { dynamic_casting };

module vlin;

# W -= lr * grad, element-wise, in place. W must live outside any
# region that gets rewound between calls — the mutation is what makes
# training work; a rebinding assignment would silently write into an
# arena slot that the caller never sees.
#
# Throws on shape mismatch between W and grad. Before this guard
# existed, a smaller grad produced a silent out-of-bounds read (the
# loop ran to W.row * W.col and indexed grad past its end) and a
# larger grad was silently truncated. Neither case was recoverable
# from user code because there was no signal that anything went
# wrong. The current behaviour is: either the update happens
# correctly, or the call throws before touching any element.
fn :: vlin sgd_update_inplace(W :: vlin.Types.Matrix,
                              grad :: vlin.Types.Matrix,
                              lr :: Float64) {
    if W.row != grad.row || W.col != grad.col {
        throw "vlin.sgd_update_inplace: shape mismatch: W is "
            + string(W.row) + "x" + string(W.col)
            + ", grad is " + string(grad.row) + "x" + string(grad.col);
    }
    n :: Int64 := W.row * W.col;
    through i :: 0..n-1 -> loop {
        W.data[i] = W.data[i] - lr * grad.data[i];
    };
    return;
}
