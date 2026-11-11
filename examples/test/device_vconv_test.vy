# device_vconv_test.vy - device-native vconv primitives (@device backend).
#
# Exercises `vconv_conv2d_fwd_device`, the backward primitives, the device
# ReLU and the device pixel shuffle against the host `vconv` CPU reference.
#
#   ./vynec --compile examples/test/device_vconv_test.vy
#   ./examples/test/device_vconv_test
#
# CUDA is used when available; otherwise @device<cuda> falls back to the CPU
# device backend and the identical comparison still runs.

use external "vconv/vconv.vy";

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

out("=== device vconv ===");

# ---------------------------------------------------------------
# conv2d forward: 3x3x1 input, 3x3x1 kernel, pad 1 -> 3x3x1.
# ---------------------------------------------------------------
x :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0];
w :: Array<Float64> := [0.5, 0.0, -0.5, 0.25, 0.0, -0.25, 0.1, 0.0, -0.1];
b :: Array<Float64> := [0.2];

xt :: vconv.Tensor := vconv.tensor(x, 3, 3, 1);
yt :: vconv.Tensor := vconv.conv2d(xt, w, b, 1, 3, 1, 1);

xf :: Array<Float32> := [];
through i :: 0..8 -> loop { xf.push(float32(x[i])); };
wf :: Array<Float32> := [];
through i2 :: 0..8 -> loop { wf.push(float32(w[i2])); };
bf :: Array<Float32> := [0.2f];
of :: Array<Float32> := [];
through i3 :: 0..8 -> loop { of.push(0.0f); };

@device<cuda> region conv {
    d_x :: Int64 := device_alloc(9);
    d_w :: Int64 := device_alloc(9);
    d_b :: Int64 := device_alloc(1);
    device_upload(d_x, xf, 9);
    device_upload(d_w, wf, 9);
    device_upload(d_b, bf, 1);

    d_o :: Int64 := vconv_conv2d_fwd_device(d_x, d_w, d_b, 3, 3, 1, 1, 3, 3, 1, 1);
    device_download(d_o, of, 9);
    device_sync();
};

through i4 :: 0..8 -> loop {
    check("fwd-" + string(i4), close(yt.data[i4], float64(of[i4]), 1e-5));
};

# ---------------------------------------------------------------
# ReLU forward device vs host.
# ---------------------------------------------------------------
relu_in :: Array<Float64> := [-2.0, 0.0, 3.0, -0.5];
rt :: vconv.Tensor := vconv.tensor(relu_in, 1, 4, 1);
rr :: vconv.Tensor := vconv.relu(rt);

rf_in :: Array<Float32> := [-2.0f, 0.0f, 3.0f, -0.5f];
rf_out :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];

@device<cuda> region act {
    d_a :: Int64 := device_alloc(4);
    device_upload(d_a, rf_in, 4);
    d_r :: Int64 := vconv_relu_fwd_device(d_a, 4);
    device_download(d_r, rf_out, 4);
    device_sync();
};

through i5 :: 0..3 -> loop {
    check("relu-" + string(i5), close(rr.data[i5], float64(rf_out[i5]), 1e-6));
};

# ---------------------------------------------------------------
# pixel shuffle forward device vs host (1x1x4 -> 2x2x1, scale 2).
# ---------------------------------------------------------------
ps_in :: Array<Float64> := [0.1, 0.2, 0.3, 0.4];
pt :: vconv.Tensor := vconv.tensor(ps_in, 1, 1, 4);
ps :: vconv.Tensor := vconv.pixel_shuffle(pt, 2);

ps_inf :: Array<Float32> := [0.1f, 0.2f, 0.3f, 0.4f];
ps_outf :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];

@device<cuda> region shuffle {
    d_p :: Int64 := device_alloc(4);
    device_upload(d_p, ps_inf, 4);
    d_s :: Int64 := vconv_pixel_shuffle_fwd_device(d_p, 1, 1, 4, 2);
    device_download(d_s, ps_outf, 4);
    device_sync();
};

through i6 :: 0..3 -> loop {
    check("shuffle-" + string(i6), close(ps.data[i6], float64(ps_outf[i6]), 1e-5));
};

# ---------------------------------------------------------------
# backward w.r.t. input: finite differences on the conv above.
#   dL/dx = conv2d_backward_input(dL/dy, w)
# Uses a gradient that is a unit vector at position p, compared against
# the host reference `vconv.conv_backward_input`.
# ---------------------------------------------------------------
grad :: Array<Float64> := [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
gt :: vconv.Tensor := vconv.tensor(grad, 3, 3, 1);
host_gi :: Array<Float64> := vconv.conv_backward_input(gt, w, 3, 3, 1, 3, 1, 1);

gradf :: Array<Float32> := [];
through i7 :: 0..8 -> loop { gradf.push(float32(grad[i7])); };
gi_outf :: Array<Float32> := [];
through i8 :: 0..8 -> loop { gi_outf.push(0.0f); };

@device<cuda> region bwd_in {
    d_g :: Int64 := device_alloc(9);
    d_w :: Int64 := device_alloc(9);
    device_upload(d_g, gradf, 9);
    device_upload(d_w, wf, 9);
    d_gi :: Int64 := vconv_conv2d_backward_input_device(d_g, d_w, 3, 3, 1, 3, 3, 1, 3, 3, 1, 1);
    device_download(d_gi, gi_outf, 9);
    device_sync();
};

through i9 :: 0..8 -> loop {
    check("bwd-in-" + string(i9), close(host_gi[i9], float64(gi_outf[i9]), 1e-5));
};

# ---------------------------------------------------------------
# backward w.r.t. weight: compare against host reference.
# ---------------------------------------------------------------
host_gw :: Array<Float64> := vconv.conv_backward_weight(xt, gt, 3, 1, 1);
gw_outf :: Array<Float32> := [];
through i10 :: 0..8 -> loop { gw_outf.push(0.0f); };

@device<cuda> region bwd_w {
    d_x :: Int64 := device_alloc(9);
    d_g :: Int64 := device_alloc(9);
    device_upload(d_x, xf, 9);
    device_upload(d_g, gradf, 9);
    d_gw :: Int64 := vconv_conv2d_backward_weight_device(d_x, d_g, 3, 3, 1, 3, 3, 1, 3, 3, 1, 1);
    device_download(d_gw, gw_outf, 9);
    device_sync();
};

through i11 :: 0..8 -> loop {
    check("bwd-w-" + string(i11), close(host_gw[i11], float64(gw_outf[i11]), 1e-5));
};

# ---------------------------------------------------------------
# backward w.r.t. bias: compare against host reference.
# ---------------------------------------------------------------
host_gb :: Array<Float64> := vconv.conv_backward_bias(gt);
gb_outf :: Array<Float32> := [0.0f];

@device<cuda> region bwd_b {
    d_g :: Int64 := device_alloc(9);
    device_upload(d_g, gradf, 9);
    d_gb :: Int64 := vconv_conv2d_backward_bias_device(d_g, 3, 3, 1);
    device_download(d_gb, gb_outf, 1);
    device_sync();
};

check("bwd-b-0", close(host_gb[0], float64(gb_outf[0]), 1e-5));

# ---------------------------------------------------------------
# ReLU backward on device: pre > 0 selects the upstream gradient.
# ---------------------------------------------------------------
rb_grad :: Array<Float32> := [1.0f, 2.0f, 3.0f, 4.0f];
rb_pre :: Array<Float32> := [-2.0f, 0.0f, 3.0f, -0.5f];
rb_out :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];
rb_expect :: Array<Float64> := [0.0, 0.0, 3.0, 0.0];

@device<cuda> region bwd_relu {
    d_grad :: Int64 := device_alloc(4);
    d_pre :: Int64 := device_alloc(4);
    device_upload(d_grad, rb_grad, 4);
    device_upload(d_pre, rb_pre, 4);
    d_rb :: Int64 := vconv_relu_backward_device(d_grad, d_pre, 4);
    device_download(d_rb, rb_out, 4);
    device_sync();
};

through i12 :: 0..3 -> loop {
    check("bwd-relu-" + string(i12), close(rb_expect[i12], float64(rb_out[i12]), 1e-6));
};

# ---------------------------------------------------------------
# Pixel shuffle backward maps output gradients back to sub-pixel channels.
# ---------------------------------------------------------------
ps_grad_out :: Array<Float32> := [1.0f, 2.0f, 3.0f, 4.0f];
ps_grad_in :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];
ps_grad_expect :: Array<Float64> := [1.0, 2.0, 3.0, 4.0];

@device<cuda> region bwd_shuffle {
    d_go :: Int64 := device_alloc(4);
    device_upload(d_go, ps_grad_out, 4);
    d_gi :: Int64 := vconv_pixel_shuffle_backward_device(d_go, 1, 1, 4, 2);
    device_download(d_gi, ps_grad_in, 4);
    device_sync();
};

through i14 :: 0..3 -> loop {
    check("bwd-shuffle-" + string(i14), close(ps_grad_expect[i14], float64(ps_grad_in[i14]), 1e-6));
};

# ---------------------------------------------------------------
# MSE gradient and in-place SGD update, the primitive training pair.
# ---------------------------------------------------------------
mse_pred :: Array<Float32> := [1.0f, 3.0f];
mse_target :: Array<Float32> := [0.0f, 1.0f];
mse_grad :: Array<Float32> := [0.0f, 0.0f];
mse_part :: Array<Float32> := [0.0f, 0.0f];
sgd_param :: Array<Float32> := [1.0f, 2.0f];

@device<cuda> region train_prims {
    d_pred :: Int64 := device_alloc(2);
    d_target :: Int64 := device_alloc(2);
    d_grad :: Int64 := device_alloc(2);
    d_part :: Int64 := device_alloc(2);
    d_param :: Int64 := device_alloc(2);
    device_upload(d_pred, mse_pred, 2);
    device_upload(d_target, mse_target, 2);
    device_upload(d_param, sgd_param, 2);
    device_mse_grad(d_grad, d_part, d_pred, d_target, 2);
    device_sgd_step(d_param, d_grad, 0.1, 2);
    device_download(d_grad, mse_grad, 2);
    device_download(d_part, mse_part, 2);
    device_download(d_param, sgd_param, 2);
    device_sync();
};

check("mse-grad-0", close(float64(mse_grad[0]), 1.0, 1e-6));
check("mse-grad-1", close(float64(mse_grad[1]), 2.0, 1e-6));
check("mse-part-0", close(float64(mse_part[0]), 0.5, 1e-6));
check("mse-part-1", close(float64(mse_part[1]), 2.0, 1e-6));
check("sgd-step-0", close(float64(sgd_param[0]), 0.9, 1e-6));
check("sgd-step-1", close(float64(sgd_param[1]), 1.8, 1e-6));

adam_param :: Array<Float32> := [1.0f, 2.0f];
adam_grad :: Array<Float32> := [1.0f, 2.0f];
adam_m :: Array<Float32> := [0.0f, 0.0f];
adam_v :: Array<Float32> := [0.0f, 0.0f];

@device<cuda> region adamw_prim {
    d_param :: Int64 := device_alloc(2);
    d_grad :: Int64 := device_alloc(2);
    d_m :: Int64 := device_alloc(2);
    d_v :: Int64 := device_alloc(2);
    device_upload(d_param, adam_param, 2);
    device_upload(d_grad, adam_grad, 2);
    device_upload(d_m, adam_m, 2);
    device_upload(d_v, adam_v, 2);
    device_adamw_step(d_param, d_m, d_v, d_grad, 0.1, 0.9, 0.999, 1e-8, 0.0, 1, 2);
    device_download(d_param, adam_param, 2);
    device_download(d_m, adam_m, 2);
    device_download(d_v, adam_v, 2);
    device_sync();
};

check("adamw-step-0", close(float64(adam_param[0]), 0.9, 1e-5));
check("adamw-step-1", close(float64(adam_param[1]), 1.9, 1e-5));
check("adamw-m-0", close(float64(adam_m[0]), 0.1, 1e-6));
check("adamw-v-1", close(float64(adam_v[1]), 0.004, 1e-6));

sum_in :: Array<Float32> := [0.5f, 1.5f, 2.0f];
sum_out :: Array<Float32> := [0.0f];
slice_out :: Array<Float32> := [0.0f, 0.0f];
direct_shuffle_out :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];
direct_shuffle_grad :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];

@device<cuda> region direct_prims {
    d_sum_in :: Int64 := device_alloc(3);
    d_sum_out :: Int64 := device_alloc(1);
    d_slice_out :: Int64 := device_alloc(2);
    d_ps_in :: Int64 := device_alloc(4);
    d_ps_out :: Int64 := device_alloc(4);
    d_ps_grad :: Int64 := device_alloc(4);
    device_upload(d_sum_in, sum_in, 3);
    device_upload(d_ps_in, ps_inf, 4);
    device_sum(d_sum_out, d_sum_in, 3);
    device_copy_slice(d_slice_out, d_sum_in, 1, 2);
    device_pixel_shuffle(d_ps_out, d_ps_in, 1, 1, 4, 2);
    device_pixel_shuffle_backward(d_ps_grad, d_ps_out, 1, 1, 4, 2);
    device_download(d_sum_out, sum_out, 1);
    device_download(d_slice_out, slice_out, 2);
    device_download(d_ps_out, direct_shuffle_out, 4);
    device_download(d_ps_grad, direct_shuffle_grad, 4);
    device_sync();
};

check("device-sum", close(float64(sum_out[0]), 4.0, 1e-6));
check("device-copy-slice-0", close(float64(slice_out[0]), 1.5, 1e-6));
check("device-copy-slice-1", close(float64(slice_out[1]), 2.0, 1e-6));
through i15 :: 0..3 -> loop {
    check("direct-shuffle-" + string(i15), close(ps.data[i15], float64(direct_shuffle_out[i15]), 1e-5));
    check("direct-shuffle-bwd-" + string(i15), close(float64(ps_inf[i15]), float64(direct_shuffle_grad[i15]), 1e-5));
};

# ---------------------------------------------------------------
# Float64 device handles select the f64 PTX path end-to-end.
# ---------------------------------------------------------------
of64 :: Array<Float64> := [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];

@device<cuda> region fwd_f64 {
    d_x :: Int64 := device_alloc_f64(9);
    d_w :: Int64 := device_alloc_f64(9);
    d_b :: Int64 := device_alloc_f64(1);
    device_upload(d_x, x, 9);
    device_upload(d_w, w, 9);
    device_upload(d_b, b, 1);
    d_o :: Int64 := vconv_conv2d_fwd_device(d_x, d_w, d_b, 3, 3, 1, 1, 3, 3, 1, 1);
    device_download(d_o, of64, 9);
    device_sync();
};

through i13 :: 0..8 -> loop {
    check("fwd-f64-" + string(i13), close(yt.data[i13], of64[i13], 1e-9));
};

if failures[0] != 0 {
    out("device vconv: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
