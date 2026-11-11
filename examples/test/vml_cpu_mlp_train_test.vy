# vml_cpu_mlp_train_test.vy - CPU dense-model training smoke test.
#
# This keeps vml's role clear after vtensor/vnn: vml owns CPU dense/MLP
# workflows over vlin.Matrix. The test trains a one-layer sigmoid classifier on
# OR and checks that loss decreases and accuracy reaches 100%.

use external "vml/vml.vy";
use external "vlin/vlin.vy";
use native vmath;

ruleset { dynamic_casting };

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else { out("FAIL " + name); failures[0] = failures[0] + 1; }
}

out("=== vml CPU MLP train ===");
vmath.seed(7);

X :: vlin.Types.Matrix := vlin.from_flat(4, 2, [
    0.0, 0.0,
    0.0, 1.0,
    1.0, 0.0,
    1.0, 1.0
]);
Y :: vlin.Types.Matrix := vlin.from_flat(4, 1, [0.0, 1.0, 1.0, 1.0]);

layer :: vml.Types.Dense := vml.dense(2, 1, "sigmoid");
W :: vlin.Types.Matrix := layer.W;
b :: Array<Float64> := layer.b;
opt :: vml.Types.Adam := vml.adam(0.1);
state :: vml.Types.AdamState := vml.adam_state(2);

initial_pred :: vlin.Types.Matrix := vml.dense_forward_fused(layer, X);
initial_loss :: Float64 := vml.cross_entropy(initial_pred, Y);

t :: Int64 := 0;
through epoch :: 1..250 -> loop {
    pred :: vlin.Types.Matrix := vml.dense_forward_fused(layer, X);
    delta :: vlin.Types.Matrix := vlin.subtract(pred, Y);
    dW :: vlin.Types.Matrix := vlin.multiply(vlin.transpose(X), delta);

    t = t + 1;
    vml.adam_step(W, dW, state, opt, t);

    db :: Float64 := 0.0;
    through r :: 0..3 -> loop { db = db + delta.data[r]; };
    b[0] = b[0] - 0.1 * db / 4.0;
};

final_pred :: vlin.Types.Matrix := vml.dense_forward_fused(layer, X);
final_loss :: Float64 := vml.cross_entropy(final_pred, Y);
final_acc :: Float64 := vml.accuracy(final_pred, Y, 4);

out("initial loss: " + string(initial_loss));
out("final loss  : " + string(final_loss));
out("final acc   : " + string(final_acc));

check("loss-decreased", final_loss < initial_loss);
check("accuracy", final_acc == 1.0);

if failures[0] != 0 {
    out("vml CPU MLP train: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
