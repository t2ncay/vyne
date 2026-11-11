# device_layers.vy - project-local ESPCN device layer wrappers.

use "model.vy";

@device_fn
fn espcn_forward_device(
    model :: ESPCN,
    d_x :: Int64,
    h :: Int64,
    w :: Int64,
    d_w1 :: Int64,
    d_b1 :: Int64,
    d_w2 :: Int64,
    d_b2 :: Int64,
    d_w3 :: Int64,
    d_b3 :: Int64,
    d_h1_pre :: Int64,
    d_a1 :: Int64,
    d_h2_pre :: Int64,
    d_a2 :: Int64,
    d_h3 :: Int64,
    d_pred :: Int64
) {
    h1_n :: Int64 := h * w * model.c1;
    h2_n :: Int64 := h * w * model.c2;
    device_conv2d(d_h1_pre, d_x, d_w1, d_b1,
        h, w, model.in_c, h, w, model.c1, model.k1, 1, model.k1 / 2, 1);
    device_relu(d_a1, d_h1_pre, h1_n);
    device_conv2d(d_h2_pre, d_a1, d_w2, d_b2,
        h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2, 1);
    device_relu(d_a2, d_h2_pre, h2_n);
    device_conv2d(d_h3, d_a2, d_w3, d_b3,
        h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2, 1);
    device_pixel_shuffle(d_pred, d_h3, h, w, model.c3, model.scale);
}

@device_fn
fn espcn_backward_device(
    model :: ESPCN,
    d_x :: Int64,
    h :: Int64,
    w :: Int64,
    d_w2 :: Int64,
    d_w3 :: Int64,
    d_h1_pre :: Int64,
    d_a1 :: Int64,
    d_h2_pre :: Int64,
    d_a2 :: Int64,
    d_loss_grad :: Int64,
    d_gh3 :: Int64,
    d_gw3 :: Int64,
    d_gb3 :: Int64,
    d_ga2 :: Int64,
    d_gh2 :: Int64,
    d_gw2 :: Int64,
    d_gb2 :: Int64,
    d_ga1 :: Int64,
    d_gh1 :: Int64,
    d_gw1 :: Int64,
    d_gb1 :: Int64
) {
    h1_n :: Int64 := h * w * model.c1;
    h2_n :: Int64 := h * w * model.c2;
    device_pixel_shuffle_backward(d_gh3, d_loss_grad, h, w, model.c3, model.scale);
    device_conv2d_backward_weight(d_gw3, d_a2, d_gh3,
        h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2);
    device_conv2d_backward_bias(d_gb3, d_gh3, h, w, model.c3);
    device_conv2d_backward_input(d_ga2, d_gh3, d_w3,
        h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2);
    device_relu_grad(d_gh2, d_ga2, d_h2_pre, h2_n);
    device_conv2d_backward_weight(d_gw2, d_a1, d_gh2,
        h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2);
    device_conv2d_backward_bias(d_gb2, d_gh2, h, w, model.c2);
    device_conv2d_backward_input(d_ga1, d_gh2, d_w2,
        h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2);
    device_relu_grad(d_gh1, d_ga1, d_h1_pre, h1_n);
    device_conv2d_backward_weight(d_gw1, d_x, d_gh1,
        h, w, model.in_c, h, w, model.c1, model.k1, 1, model.k1 / 2);
    device_conv2d_backward_bias(d_gb1, d_gh1, h, w, model.c1);
}
