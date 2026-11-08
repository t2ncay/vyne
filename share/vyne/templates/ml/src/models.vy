# models.vy - the MLP definition for {{name}}
#
# Two dense layers: features -> hidden (tanh) -> classes (sigmoid).
# The one-hot targets with a sigmoid output and binary cross-entropy is
# a deliberate simplification: it keeps the backward pass readable and
# is enough for this dataset. Swap the last activation to softmax and
# the loss to categorical cross-entropy when you outgrow it.

use external "vlin/vlin.vy";
use external "vml/vml.vy";

ruleset { dynamic_casting };

module models;

interface MLP {
    seq      :: vml.Types.Sequential,
    W1       :: vlin.Types.Matrix,
    b1       :: Array<Float64>,
    W2       :: vlin.Types.Matrix,
    b2       :: Array<Float64>,
    hidden   :: Int64,
}

fn :: models make_mlp(
    in_features :: Int64,
    hidden :: Int64,
    classes :: Int64
) -> models.MLP {
    L1 :: vml.Types.Dense := vml.dense(in_features, hidden, "tanh");
    L2 :: vml.Types.Dense := vml.dense(hidden, classes, "sigmoid");
    seq :: vml.Types.Sequential := vml.sequential([L1, L2]);
    return models.MLP(seq, L1.W, L1.b, L2.W, L2.b, hidden);
}

# Multi-class accuracy from one-hot predictions and targets.
fn :: models accuracy_multi(
    pred :: vlin.Types.Matrix,
    target :: vlin.Types.Matrix,
    n :: Int64,
    classes :: Int64
) -> Float64 {
    correct :: Int64 := 0;
    through r :: 0..n-1 -> loop {
        pi :: Int64 := 0;
        ai :: Int64 := 0;
        pbest :: Float64 := 0.0;
        abest :: Float64 := 0.0;
        through c :: 0..classes-1 -> loop {
            p :: Float64 := pred.data[r * classes + c];
            a :: Float64 := target.data[r * classes + c];
            if p > pbest { pbest = p; pi = c; }
            if a > abest { abest = a; ai = c; }
        };
        if pi == ai {
            correct = correct + 1;
        }
    };
    return float64(correct) / float64(n);
}
