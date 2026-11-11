# train_gpu.vy - explicit GPU ESPCN training proof.
#
# This is intentionally a v0: ReLU ESPCN, batch size 1, MSE loss, SGD, and an
# explicit device-resident forward/backward graph. It proves the runtime pieces
# needed for GPU training without claiming general tensor AD yet.
#
#   ../../../vynec --compile src/train_gpu.vy && ./src/train_gpu

use native vcore;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use "model.vy";
use "device_tensor.vy";

ruleset { dynamic_casting };

INPUT :: String := "data/sample.png";
REFERENCE :: String := "data/reference.png";
MODEL :: String := "data/espcn_x2.vwy";
EPOCHS :: Int64 := 8;
LR :: Float64 := 0.05;

out("=== Vyne ESPCN GPU training v0 ===");

input :: vimage.Image := vimage.load(INPUT);
reference :: vimage.Image := vimage.load(REFERENCE);
model :: ESPCN := load_espcn(MODEL);

if !gpu_supported(model) {
    throw "train_gpu: only ReLU ESPCN models are supported in GPU training v0";
}
if reference.width != input.width * model.scale ||
    reference.height != input.height * model.scale ||
    reference.channels != input.channels {
    throw "train_gpu: reference shape must match input scaled by model.scale";
}

h :: Int64 := input.height;
w :: Int64 := input.width;
in_n :: Int64 := h * w * model.in_c;
h1_n :: Int64 := h * w * model.c1;
h2_n :: Int64 := h * w * model.c2;
h3_n :: Int64 := h * w * model.c3;
out_c :: Int64 := model.c3 / (model.scale * model.scale);
out_n :: Int64 := (h * model.scale) * (w * model.scale) * out_c;

xf :: Array<Float32> := [];
through xi :: 0..in_n-1 -> loop { xf.push(float32(input.pixels[xi])); };
targetf :: Array<Float32> := [];
through ti :: 0..out_n-1 -> loop { targetf.push(float32(reference.pixels[ti])); };

w1f :: Array<Float32> := [];
through w1i :: 0..model.w1.size()-1 -> loop { w1f.push(float32(model.w1[w1i])); };
b1f :: Array<Float32> := [];
through b1i :: 0..model.c1-1 -> loop { b1f.push(float32(model.b1[b1i])); };
w2f :: Array<Float32> := [];
through w2i :: 0..model.w2.size()-1 -> loop { w2f.push(float32(model.w2[w2i])); };
b2f :: Array<Float32> := [];
through b2i :: 0..model.c2-1 -> loop { b2f.push(float32(model.b2[b2i])); };
w3f :: Array<Float32> := [];
through w3i :: 0..model.w3.size()-1 -> loop { w3f.push(float32(model.w3[w3i])); };
b3f :: Array<Float32> := [];
through b3i :: 0..model.c3-1 -> loop { b3f.push(float32(model.b3[b3i])); };

loss_part :: Array<Float32> := [];
through li :: 0..out_n-1 -> loop { loss_part.push(0.0f); };

first_loss :: Float64 := 0.0;
last_loss :: Float64 := 0.0;
t0 :: Int64 := vcore.now_ns();

@device<cuda> region train {
    out("device    : " + device_backend());

    d_x :: Int64 := device_alloc(in_n);
    d_target :: Int64 := device_alloc(out_n);
    d_w1 :: Int64 := device_alloc(w1f.size());
    d_b1 :: Int64 := device_alloc(model.c1);
    d_w2 :: Int64 := device_alloc(w2f.size());
    d_b2 :: Int64 := device_alloc(model.c2);
    d_w3 :: Int64 := device_alloc(w3f.size());
    d_b3 :: Int64 := device_alloc(model.c3);

    x_dev :: DeviceTensor := DeviceTensor(d_x, h, w, model.in_c, in_n);

    device_upload(d_x, xf, in_n);
    device_upload(d_target, targetf, out_n);
    device_upload(d_w1, w1f, w1f.size());
    device_upload(d_b1, b1f, model.c1);
    device_upload(d_w2, w2f, w2f.size());
    device_upload(d_b2, b2f, model.c2);
    device_upload(d_w3, w3f, w3f.size());
    device_upload(d_b3, b3f, model.c3);

    through epoch :: 0..EPOCHS-1 -> loop {
        h1_pre :: Int64 := vconv_conv2d_fwd_device(
            x_dev.handle, d_w1, d_b1,
            h, w, model.in_c,
            model.c1, model.k1, model.k1, 1, model.k1 / 2
        );
        a1 :: Int64 := vconv_relu_fwd_device(h1_pre, h1_n);

        h2_pre :: Int64 := vconv_conv2d_fwd_device(
            a1, d_w2, d_b2,
            h, w, model.c1,
            model.c2, model.k2, model.k2, 1, model.k2 / 2
        );
        a2 :: Int64 := vconv_relu_fwd_device(h2_pre, h2_n);

        h3 :: Int64 := vconv_conv2d_fwd_device(
            a2, d_w3, d_b3,
            h, w, model.c2,
            model.c3, model.k3, model.k3, 1, model.k3 / 2
        );
        pred :: Int64 := vconv_pixel_shuffle_fwd_device(h3, h, w, model.c3, model.scale);

        d_loss_grad :: Int64 := device_alloc(out_n);
        d_loss_part :: Int64 := device_alloc(out_n);
        device_mse_grad(d_loss_grad, d_loss_part, pred, d_target, out_n);
        device_download(d_loss_part, loss_part, out_n);

        loss :: Float64 := 0.0;
        through si :: 0..out_n-1 -> loop { loss = loss + float64(loss_part[si]); };
        if epoch == 0 { first_loss = loss; }
        last_loss = loss;
        out("epoch " + string(epoch) + " loss " + string(loss));

        gh3 :: Int64 := vconv_pixel_shuffle_backward_device(
            d_loss_grad, h, w, model.c3, model.scale
        );

        gw3 :: Int64 := vconv_conv2d_backward_weight_device(
            a2, gh3, h, w, model.c2, h, w, model.c3,
            model.k3, model.k3, 1, model.k3 / 2
        );
        gb3 :: Int64 := vconv_conv2d_backward_bias_device(gh3, h, w, model.c3);
        ga2 :: Int64 := vconv_conv2d_backward_input_device(
            gh3, d_w3, h, w, model.c2, h, w, model.c3,
            model.k3, model.k3, 1, model.k3 / 2
        );
        gh2 :: Int64 := vconv_relu_backward_device(ga2, h2_pre, h2_n);

        gw2 :: Int64 := vconv_conv2d_backward_weight_device(
            a1, gh2, h, w, model.c1, h, w, model.c2,
            model.k2, model.k2, 1, model.k2 / 2
        );
        gb2 :: Int64 := vconv_conv2d_backward_bias_device(gh2, h, w, model.c2);
        ga1 :: Int64 := vconv_conv2d_backward_input_device(
            gh2, d_w2, h, w, model.c1, h, w, model.c2,
            model.k2, model.k2, 1, model.k2 / 2
        );
        gh1 :: Int64 := vconv_relu_backward_device(ga1, h1_pre, h1_n);

        gw1 :: Int64 := vconv_conv2d_backward_weight_device(
            x_dev.handle, gh1, h, w, model.in_c, h, w, model.c1,
            model.k1, model.k1, 1, model.k1 / 2
        );
        gb1 :: Int64 := vconv_conv2d_backward_bias_device(gh1, h, w, model.c1);

        device_sgd_step(d_w1, gw1, LR, w1f.size());
        device_sgd_step(d_b1, gb1, LR, model.c1);
        device_sgd_step(d_w2, gw2, LR, w2f.size());
        device_sgd_step(d_b2, gb2, LR, model.c2);
        device_sgd_step(d_w3, gw3, LR, w3f.size());
        device_sgd_step(d_b3, gb3, LR, model.c3);
    };

    device_download(d_w1, w1f, w1f.size());
    device_download(d_b1, b1f, model.c1);
    device_download(d_w2, w2f, w2f.size());
    device_download(d_b2, b2f, model.c2);
    device_download(d_w3, w3f, w3f.size());
    device_download(d_b3, b3f, model.c3);
    device_sync();
    out("dev peak  : " + string(device_peak_bytes()) + " bytes");
};

t1 :: Int64 := vcore.now_ns();
out("first loss: " + string(first_loss));
out("last loss : " + string(last_loss));
if last_loss < first_loss {
    out("training : loss decreased");
} else {
    out("training : loss did not decrease");
    exit(1);
}
out("time     : " + string(float64(t1 - t0) / 1000000.0) + " ms");
out("=== done ===");
