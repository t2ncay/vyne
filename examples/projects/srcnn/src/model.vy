# model.vy - residual SRCNN: 3->8 (5x5), 8->4 (1x1), 4->3 (5x5).
#
# The network predicts a residual added to the bilinear input. The last
# layer starts at zero, so the initial model is exactly bilinear; training
# can only learn a correction. Training uses L2 loss for stable PSNR
# convergence on the small scratch model; pure L1 was evaluated but stalled.
#
# Training uses the compiler's @backward region. `sr_loss_vjp` is registered
# by the `<fn>_vjp` convention and calls the explicit vconv backward kernels;
# full tensor AD for convolution is still deferred.

use native vmath;
use native vcore;
use native vfs;
use external "vconv/vconv.vy";

# Gradient storage for the compiler-registered `sr_loss_vjp` custom adjoint.
grad_w1 :: Array<Float64> := [];
grad_b1 :: Array<Float64> := [];
grad_w2 :: Array<Float64> := [];
grad_b2 :: Array<Float64> := [];
grad_w3 :: Array<Float64> := [];
grad_b3 :: Array<Float64> := [];
grad_loss :: Float64 := 0.0;

ruleset { dynamic_casting };

interface SRCNN {
    w1 :: Array<Float64>,
    b1 :: Array<Float64>,
    w2 :: Array<Float64>,
    b2 :: Array<Float64>,
    w3 :: Array<Float64>,
    b3 :: Array<Float64>,
}

interface Grads {
    w1 :: Array<Float64>,
    b1 :: Array<Float64>,
    w2 :: Array<Float64>,
    b2 :: Array<Float64>,
    w3 :: Array<Float64>,
    b3 :: Array<Float64>,
    loss :: Float64,
}

fn zeros(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return data;
}

fn rand_range(n :: Int64, lo :: Float64,
                 hi :: Float64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(vmath.random_float(lo, hi)); };
    return data;
}

fn init_model() -> SRCNN {
    vmath.seed(42);
    w1 :: Array<Float64> := rand_range(8 * 3 * 5 * 5, -0.08, 0.08);
    b1 :: Array<Float64> := zeros(8);
    w2 :: Array<Float64> := rand_range(4 * 8, -0.12, 0.12);
    b2 :: Array<Float64> := zeros(4);
    w3 :: Array<Float64> := zeros(3 * 4 * 5 * 5);
    b3 :: Array<Float64> := zeros(3);
    return SRCNN(w1, b1, w2, b2, w3, b3);
}

fn model_from_map(model :: Map) -> SRCNN {
    w1 :: Array<Float64> := vconv.get_tensor(model, "w1");
    b1 :: Array<Float64> := vconv.get_tensor(model, "b1");
    w2 :: Array<Float64> := vconv.get_tensor(model, "w2");
    b2 :: Array<Float64> := vconv.get_tensor(model, "b2");
    w3 :: Array<Float64> := vconv.get_tensor(model, "w3");
    b3 :: Array<Float64> := vconv.get_tensor(model, "b3");
    return SRCNN(w1, b1, w2, b2, w3, b3);
}

fn tensor_add(a :: vconv.Tensor, b :: vconv.Tensor) -> vconv.Tensor {
    if a.height != b.height || a.width != b.width ||
       a.channels != b.channels {
        throw "model.tensor_add: shape mismatch";
    }
    data :: Array<Float64> := [];
    n :: Int64 := a.data.size();
    through i :: 0..n-1 -> loop {
        data.push(a.data[i] + b.data[i]);
    };
    return vconv.Tensor(data, a.height, a.width, a.channels);
}

fn forward(model :: SRCNN, x :: vconv.Tensor) -> vconv.Tensor {
    h1 :: vconv.Tensor := vconv.conv2d(x, model.w1, model.b1, 8, 5, 1, 2);
    a1 :: vconv.Tensor := vconv.relu(h1);
    h2 :: vconv.Tensor := vconv.conv2d(a1, model.w2, model.b2, 4, 1, 1, 0);
    a2 :: vconv.Tensor := vconv.relu(h2);
    res :: vconv.Tensor := vconv.conv2d(a2, model.w3, model.b3, 3, 5, 1, 2);
    return tensor_add(x, res);
}

fn loss_and_grads(model :: SRCNN, x :: vconv.Tensor,
                  y :: vconv.Tensor) -> Grads {
    h1 :: vconv.Tensor := vconv.conv2d(x, model.w1, model.b1, 8, 5, 1, 2);
    a1 :: vconv.Tensor := vconv.relu(h1);
    h2 :: vconv.Tensor := vconv.conv2d(a1, model.w2, model.b2, 4, 1, 1, 0);
    a2 :: vconv.Tensor := vconv.relu(h2);
    res :: vconv.Tensor := vconv.conv2d(a2, model.w3, model.b3, 3, 5, 1, 2);
    pred :: vconv.Tensor := tensor_add(x, res);

    n :: Int64 := pred.data.size();
    loss :: Float64 := 0.0;
    gpred :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        d :: Float64 := pred.data[i] - y.data[i];
        loss = loss + d * d;
        gpred.push(2.0 * d / float64(n));
    };
    loss = loss / float64(n);

    g_out :: vconv.Tensor := vconv.Tensor(gpred, pred.height, pred.width, pred.channels);

    gw3 :: Array<Float64> := vconv.conv_backward_weight(a2, g_out, 5, 1, 2);
    gb3 :: Array<Float64> := vconv.conv_backward_bias(g_out);
    ga2_data :: Array<Float64> := vconv.conv_backward_input(g_out, model.w3, h2.height, h2.width, 4, 5, 1, 2);
    gh2_data :: Array<Float64> := vconv.relu_backward(ga2_data, h2.data);
    gh2 :: vconv.Tensor := vconv.Tensor(gh2_data, h2.height, h2.width, h2.channels);

    gw2 :: Array<Float64> := vconv.conv_backward_weight(a1, gh2, 1, 1, 0);
    gb2 :: Array<Float64> := vconv.conv_backward_bias(gh2);
    ga1_data :: Array<Float64> := vconv.conv_backward_input(gh2, model.w2, h1.height, h1.width, 8, 1, 1, 0);
    gh1_data :: Array<Float64> := vconv.relu_backward(ga1_data, h1.data);
    gh1 :: vconv.Tensor := vconv.Tensor(gh1_data, h1.height, h1.width, h1.channels);

    gw1 :: Array<Float64> := vconv.conv_backward_weight(x, gh1, 5, 1, 2);
    gb1 :: Array<Float64> := vconv.conv_backward_bias(gh1);

    return Grads(gw1, gb1, gw2, gb2, gw3, gb3, loss);
}

fn append_tensor(sb :: Int64, name :: String, data :: Array<Float64>,
                    n :: Int64) {
    vcore.sb_append(sb, name);
    vcore.sb_append(sb, " 1 ");
    vcore.sb_append(sb, string(n));
    vcore.sb_append(sb, "\n");
    through i :: 0..n-1 -> loop {
        vcore.sb_append(sb, string(data[i]));
        vcore.sb_append(sb, " ");
    };
    vcore.sb_append(sb, "\n");
}

@differentiable
fn sr_loss(x :: vconv.Tensor, y :: vconv.Tensor, model :: SRCNN) -> Float64 {
    pred :: vconv.Tensor := forward(model, x);
    n :: Int64 := pred.data.size();
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := pred.data[i] - y.data[i];
        s = s + d * d;
    };
    return s / float64(n);
}

# Registered by convention as the reverse pass for sr_loss.
fn sr_loss_vjp(x :: vconv.Tensor, y :: vconv.Tensor, model :: SRCNN) {
    g :: Grads := loss_and_grads(model, x, y);
    grad_w1 = g.w1;
    grad_b1 = g.b1;
    grad_w2 = g.w2;
    grad_b2 = g.b2;
    grad_w3 = g.w3;
    grad_b3 = g.b3;
    grad_loss = g.loss;
}

fn last_grads() -> Grads {
    return Grads(grad_w1, grad_b1, grad_w2, grad_b2, grad_w3, grad_b3, grad_loss);
}

fn save_model(path :: String, model :: SRCNN) {
    sb :: Int64 := vcore.sb_create();
    vcore.sb_append(sb, "VYWT1\n6\n");
    append_tensor(sb, "w1", model.w1, 8 * 3 * 5 * 5);
    append_tensor(sb, "b1", model.b1, 8);
    append_tensor(sb, "w2", model.w2, 4 * 8);
    append_tensor(sb, "b2", model.b2, 4);
    append_tensor(sb, "w3", model.w3, 3 * 4 * 5 * 5);
    append_tensor(sb, "b3", model.b3, 3);
    vfs.write(path, vcore.sb_build(sb));
}
