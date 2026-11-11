# train_gpu.vy - explicit GPU ESPCN training proof with checkpoint save/reload.
#
# This is intentionally a V1 scope: ReLU ESPCN, batch size 1, MSE loss, AdamW,
# checkpoint save/reload, and an explicit device-resident forward/backward graph.
# It proves the runtime pieces needed for GPU training without claiming general
# tensor AD yet.
#
#   ../../../vynec --compile src/train_gpu.vy && ./src/train_gpu

use native vcore;
use native vfs;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use "model.vy";
use "device_tensor.vy";

ruleset { dynamic_casting };

INPUT :: String := "data/sample.png";
REFERENCE :: String := "data/reference.png";
MODEL :: String := "data/espcn_x2.vwy";
OUTPUT_MODEL :: String := "build/espcn_gpu_trained.vwt";
EPOCHS :: Int64 := 8;
LR :: Float64 := 0.001;
BETA1 :: Float64 := 0.9;
BETA2 :: Float64 := 0.999;
EPS :: Float64 := 1e-8;
WEIGHT_DECAY :: Float64 := 0.0;

fn append_tensor_f32(sb :: Int64, name :: String, dims :: String,
                     data :: Array<Float32>, n :: Int64) {
    vcore.sb_append(sb, name);
    vcore.sb_append(sb, " ");
    vcore.sb_append(sb, dims);
    vcore.sb_append(sb, "\n");
    through i :: 0..n-1 -> loop {
        if i != 0 { vcore.sb_append(sb, " "); }
        vcore.sb_append(sb, string(float64(data[i])));
    };
    vcore.sb_append(sb, "\n");
}

fn save_trained_model(path :: String, model :: ESPCN,
                      w1 :: Array<Float32>, b1 :: Array<Float32>,
                      w2 :: Array<Float32>, b2 :: Array<Float32>,
                      w3 :: Array<Float32>, b3 :: Array<Float32>) {
    sb :: Int64 := vcore.sb_create();
    vcore.sb_append(sb, "VYWT1\n7\n");
    w1_n :: Int64 := model.c1 * model.in_c * model.k1 * model.k1;
    w2_n :: Int64 := model.c2 * model.c1 * model.k2 * model.k2;
    w3_n :: Int64 := model.c3 * model.c2 * model.k3 * model.k3;
    append_tensor_f32(sb, "conv1.weight",
        "4 " + string(model.c1) + " " + string(model.in_c) + " " +
            string(model.k1) + " " + string(model.k1),
        w1, w1_n);
    append_tensor_f32(sb, "conv1.bias", "1 " + string(model.c1), b1, model.c1);
    append_tensor_f32(sb, "conv2.weight",
        "4 " + string(model.c2) + " " + string(model.c1) + " " +
            string(model.k2) + " " + string(model.k2),
        w2, w2_n);
    append_tensor_f32(sb, "conv2.bias", "1 " + string(model.c2), b2, model.c2);
    append_tensor_f32(sb, "conv3.weight",
        "4 " + string(model.c3) + " " + string(model.c2) + " " +
            string(model.k3) + " " + string(model.k3),
        w3, w3_n);
    append_tensor_f32(sb, "conv3.bias", "1 " + string(model.c3), b3, model.c3);
    vcore.sb_append(sb, "meta 1 9\n");
    vcore.sb_append(sb, string(float64(model.scale)) + " ");
    vcore.sb_append(sb, string(float64(model.in_c)) + " ");
    vcore.sb_append(sb, string(float64(model.c1)) + " ");
    vcore.sb_append(sb, string(float64(model.c2)) + " ");
    vcore.sb_append(sb, string(float64(model.c3)) + " ");
    vcore.sb_append(sb, string(float64(model.k1)) + " ");
    vcore.sb_append(sb, string(float64(model.k2)) + " ");
    vcore.sb_append(sb, string(float64(model.k3)) + " ");
    vcore.sb_append(sb, string(float64(model.act)) + "\n");
    vfs.write(path, vcore.sb_build(sb));
}

fn tensor_mse(a :: vconv.Tensor, b :: vimage.Image) -> Float64 {
    if a.width != b.width || a.height != b.height || a.channels != b.channels {
        throw "train_gpu.tensor_mse: shape mismatch";
    }
    s :: Float64 := 0.0;
    through i :: 0..a.data.size()-1 -> loop {
        d :: Float64 := a.data[i] - b.pixels[i];
        s = s + d * d;
    };
    return s / float64(a.data.size());
}

out("=== Vyne ESPCN GPU training v1 ===");

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

    d_m_w1 :: Int64 := device_alloc(w1f.size());
    d_v_w1 :: Int64 := device_alloc(w1f.size());
    d_m_b1 :: Int64 := device_alloc(model.c1);
    d_v_b1 :: Int64 := device_alloc(model.c1);
    d_m_w2 :: Int64 := device_alloc(w2f.size());
    d_v_w2 :: Int64 := device_alloc(w2f.size());
    d_m_b2 :: Int64 := device_alloc(model.c2);
    d_v_b2 :: Int64 := device_alloc(model.c2);
    d_m_w3 :: Int64 := device_alloc(w3f.size());
    d_v_w3 :: Int64 := device_alloc(w3f.size());
    d_m_b3 :: Int64 := device_alloc(model.c3);
    d_v_b3 :: Int64 := device_alloc(model.c3);

    x_dev :: DeviceTensor := DeviceTensor(d_x, h, w, model.in_c, in_n);

    device_upload(d_x, xf, in_n);
    device_upload(d_target, targetf, out_n);
    device_upload(d_w1, w1f, w1f.size());
    device_upload(d_b1, b1f, model.c1);
    device_upload(d_w2, w2f, w2f.size());
    device_upload(d_b2, b2f, model.c2);
    device_upload(d_w3, w3f, w3f.size());
    device_upload(d_b3, b3f, model.c3);

    device_fill(d_m_w1, 0.0, w1f.size());
    device_fill(d_v_w1, 0.0, w1f.size());
    device_fill(d_m_b1, 0.0, model.c1);
    device_fill(d_v_b1, 0.0, model.c1);
    device_fill(d_m_w2, 0.0, w2f.size());
    device_fill(d_v_w2, 0.0, w2f.size());
    device_fill(d_m_b2, 0.0, model.c2);
    device_fill(d_v_b2, 0.0, model.c2);
    device_fill(d_m_w3, 0.0, w3f.size());
    device_fill(d_v_w3, 0.0, w3f.size());
    device_fill(d_m_b3, 0.0, model.c3);
    device_fill(d_v_b3, 0.0, model.c3);

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

        step :: Int64 := epoch + 1;
        device_adamw_step(d_w1, d_m_w1, d_v_w1, gw1, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w1f.size());
        device_adamw_step(d_b1, d_m_b1, d_v_b1, gb1, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c1);
        device_adamw_step(d_w2, d_m_w2, d_v_w2, gw2, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w2f.size());
        device_adamw_step(d_b2, d_m_b2, d_v_b2, gb2, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c2);
        device_adamw_step(d_w3, d_m_w3, d_v_w3, gw3, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w3f.size());
        device_adamw_step(d_b3, d_m_b3, d_v_b3, gb3, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c3);
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
vfs.mkdir_p("build");
save_trained_model(OUTPUT_MODEL, model, w1f, b1f, w2f, b2f, w3f, b3f);
out("saved    : " + OUTPUT_MODEL);

reloaded :: ESPCN := load_espcn(OUTPUT_MODEL);
input_t :: vconv.Tensor := vconv.tensor(
    input.pixels, input.height, input.width, input.channels
);
pred_t :: vconv.Tensor := forward_cpu(reloaded, input_t);
reload_loss :: Float64 := tensor_mse(pred_t, reference);
out("reload mse: " + string(reload_loss));
if reload_loss <= first_loss {
    out("reload   : trained checkpoint verified");
} else {
    out("reload   : trained checkpoint did not improve over initial loss");
    exit(1);
}
out("time     : " + string(float64(t1 - t0) / 1000000.0) + " ms");
out("=== done ===");
