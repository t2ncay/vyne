# train_gpu.vy - explicit GPU ESPCN crop training with checkpoint save/reload.
#
# V2B scope: ReLU ESPCN, deterministic crop dataset, batch size 1, MSE loss,
# AdamW, preallocated device buffers, device loss reduction, checkpoint
# save/reload, and explicit device-resident forward/backward graph.
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
BEST_MODEL :: String := "build/espcn_gpu_best.vwt";
LOSS_CSV :: String := "build/train_loss.csv";
VAL_CSV :: String := "build/val_metrics.csv";
EPOCHS :: Int64 := 8;
TRAIN_CROPS :: Int64 := 3;
CROP_LR :: Int64 := 32;
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
    append_tensor_f32(sb, "conv1.weight", "4 " + string(model.c1) + " " + string(model.in_c) + " " + string(model.k1) + " " + string(model.k1), w1, w1_n);
    append_tensor_f32(sb, "conv1.bias", "1 " + string(model.c1), b1, model.c1);
    append_tensor_f32(sb, "conv2.weight", "4 " + string(model.c2) + " " + string(model.c1) + " " + string(model.k2) + " " + string(model.k2), w2, w2_n);
    append_tensor_f32(sb, "conv2.bias", "1 " + string(model.c2), b2, model.c2);
    append_tensor_f32(sb, "conv3.weight", "4 " + string(model.c3) + " " + string(model.c2) + " " + string(model.k3) + " " + string(model.k3), w3, w3_n);
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

fn save_loss_csv(path :: String, losses :: Array<Float64>, n :: Int64) {
    sb :: Int64 := vcore.sb_create();
    vcore.sb_append(sb, "epoch,loss\n");
    through i :: 0..n-1 -> loop {
        vcore.sb_append(sb, string(i));
        vcore.sb_append(sb, ",");
        vcore.sb_append(sb, string(losses[i]));
        vcore.sb_append(sb, "\n");
    };
    vfs.write(path, vcore.sb_build(sb));
}

fn save_val_csv(path :: String, losses :: Array<Float64>, n :: Int64) {
    sb :: Int64 := vcore.sb_create();
    vcore.sb_append(sb, "epoch,val_mse\n");
    through i :: 0..n-1 -> loop {
        vcore.sb_append(sb, string(i));
        vcore.sb_append(sb, ",");
        vcore.sb_append(sb, string(losses[i]));
        vcore.sb_append(sb, "\n");
    };
    vfs.write(path, vcore.sb_build(sb));
}

fn tensor_mse(a :: vconv.Tensor, target :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := a.data[i] - target[i];
        s = s + d * d;
    };
    return s / float64(n);
}

out("=== Vyne ESPCN GPU crop training V2B ===");

input :: vimage.Image := vimage.load(INPUT);
reference :: vimage.Image := vimage.load(REFERENCE);
model :: ESPCN := load_espcn(MODEL);

if !gpu_supported(model) {
    throw "train_gpu: only ReLU ESPCN models are supported in GPU training V2B";
}
if reference.width != input.width * model.scale || reference.height != input.height * model.scale || reference.channels != input.channels {
    throw "train_gpu: reference shape must match input scaled by model.scale";
}
if input.width < CROP_LR * 2 || input.height < CROP_LR * 2 {
    throw "train_gpu: bundled deterministic crop layout expects at least 2x2 LR crops";
}

h :: Int64 := CROP_LR;
w :: Int64 := CROP_LR;
hr_h :: Int64 := CROP_LR * model.scale;
hr_w :: Int64 := CROP_LR * model.scale;
in_n :: Int64 := h * w * model.in_c;
h1_n :: Int64 := h * w * model.c1;
h2_n :: Int64 := h * w * model.c2;
h3_n :: Int64 := h * w * model.c3;
out_c :: Int64 := model.c3 / (model.scale * model.scale);
out_n :: Int64 := hr_h * hr_w * out_c;

crop_x :: Array<Int64> := [0, CROP_LR, 0];
crop_y :: Array<Int64> := [0, 0, CROP_LR];
val_x0 :: Int64 := CROP_LR;
val_y0 :: Int64 := CROP_LR;

train_xf :: Array<Float32> := [];
train_yf :: Array<Float32> := [];
through ci :: 0..TRAIN_CROPS-1 -> loop {
    ox :: Int64 := crop_x[ci];
    oy :: Int64 := crop_y[ci];
    through yy :: 0..h-1 -> loop {
        through xx :: 0..w-1 -> loop {
            base :: Int64 := ((oy + yy) * input.width + (ox + xx)) * input.channels;
            through ch :: 0..input.channels-1 -> loop { train_xf.push(float32(input.pixels[base + ch])); };
        };
    };
    through hy :: 0..hr_h-1 -> loop {
        through hx :: 0..hr_w-1 -> loop {
            hbase :: Int64 := (((oy * model.scale) + hy) * reference.width + ((ox * model.scale) + hx)) * reference.channels;
            through ch2 :: 0..reference.channels-1 -> loop { train_yf.push(float32(reference.pixels[hbase + ch2])); };
        };
    };
};

val_xf :: Array<Float32> := [];
val_yf :: Array<Float32> := [];
val_x64 :: Array<Float64> := [];
val_y64 :: Array<Float64> := [];
through vyy :: 0..h-1 -> loop {
    through vxx :: 0..w-1 -> loop {
        vbase :: Int64 := ((val_y0 + vyy) * input.width + (val_x0 + vxx)) * input.channels;
        through vch :: 0..input.channels-1 -> loop {
            vv :: Float64 := input.pixels[vbase + vch];
            val_xf.push(float32(vv));
            val_x64.push(vv);
        };
    };
};
through vhy :: 0..hr_h-1 -> loop {
    through vhx :: 0..hr_w-1 -> loop {
        vhbase :: Int64 := (((val_y0 * model.scale) + vhy) * reference.width + ((val_x0 * model.scale) + vhx)) * reference.channels;
        through vch2 :: 0..reference.channels-1 -> loop {
            rv :: Float64 := reference.pixels[vhbase + vch2];
            val_yf.push(float32(rv));
            val_y64.push(rv);
        };
    };
};

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

loss_one :: Array<Float32> := [0.0f];
train_losses :: Array<Float64> := [];
val_losses :: Array<Float64> := [];
through li :: 0..EPOCHS-1 -> loop { train_losses.push(0.0); val_losses.push(0.0); };

first_loss :: Float64 := 0.0;
last_loss :: Float64 := 0.0;
first_val :: Float64 := 0.0;
last_val :: Float64 := 0.0;
t0 :: Int64 := vcore.now_ns();

@device<cuda> region train {
    out("device    : " + device_backend());

    d_train_x :: Int64 := device_alloc(train_xf.size());
    d_train_y :: Int64 := device_alloc(train_yf.size());
    d_val_x :: Int64 := device_alloc(in_n);
    d_val_y :: Int64 := device_alloc(out_n);
    d_x :: Int64 := device_alloc(in_n);
    d_target :: Int64 := device_alloc(out_n);
    d_w1 :: Int64 := device_alloc(w1f.size());
    d_b1 :: Int64 := device_alloc(model.c1);
    d_w2 :: Int64 := device_alloc(w2f.size());
    d_b2 :: Int64 := device_alloc(model.c2);
    d_w3 :: Int64 := device_alloc(w3f.size());
    d_b3 :: Int64 := device_alloc(model.c3);

    d_h1_pre :: Int64 := device_alloc(h1_n);
    d_a1 :: Int64 := device_alloc(h1_n);
    d_h2_pre :: Int64 := device_alloc(h2_n);
    d_a2 :: Int64 := device_alloc(h2_n);
    d_h3 :: Int64 := device_alloc(h3_n);
    d_pred :: Int64 := device_alloc(out_n);
    d_loss_grad :: Int64 := device_alloc(out_n);
    d_loss_part :: Int64 := device_alloc(out_n);
    d_loss :: Int64 := device_alloc(1);
    d_gh3 :: Int64 := device_alloc(h3_n);
    d_gw3 :: Int64 := device_alloc(w3f.size());
    d_gb3 :: Int64 := device_alloc(model.c3);
    d_ga2 :: Int64 := device_alloc(h2_n);
    d_gh2 :: Int64 := device_alloc(h2_n);
    d_gw2 :: Int64 := device_alloc(w2f.size());
    d_gb2 :: Int64 := device_alloc(model.c2);
    d_ga1 :: Int64 := device_alloc(h1_n);
    d_gh1 :: Int64 := device_alloc(h1_n);
    d_gw1 :: Int64 := device_alloc(w1f.size());
    d_gb1 :: Int64 := device_alloc(model.c1);

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

    device_upload(d_train_x, train_xf, train_xf.size());
    device_upload(d_train_y, train_yf, train_yf.size());
    device_upload(d_val_x, val_xf, in_n);
    device_upload(d_val_y, val_yf, out_n);
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
        epoch_loss :: Float64 := 0.0;
        through crop :: 0..TRAIN_CROPS-1 -> loop {
            x_off :: Int64 := crop * in_n;
            y_off :: Int64 := crop * out_n;
            device_copy_slice(d_x, d_train_x, x_off, in_n);
            device_copy_slice(d_target, d_train_y, y_off, out_n);

            device_conv2d(d_h1_pre, x_dev.handle, d_w1, d_b1, h, w, model.in_c, h, w, model.c1, model.k1, 1, model.k1 / 2, 1);
            device_relu(d_a1, d_h1_pre, h1_n);
            device_conv2d(d_h2_pre, d_a1, d_w2, d_b2, h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2, 1);
            device_relu(d_a2, d_h2_pre, h2_n);
            device_conv2d(d_h3, d_a2, d_w3, d_b3, h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2, 1);
            device_pixel_shuffle(d_pred, d_h3, h, w, model.c3, model.scale);

            device_mse_grad(d_loss_grad, d_loss_part, d_pred, d_target, out_n);
            device_sum(d_loss, d_loss_part, out_n);
            device_download(d_loss, loss_one, 1);
            epoch_loss = epoch_loss + float64(loss_one[0]);

            device_pixel_shuffle_backward(d_gh3, d_loss_grad, h, w, model.c3, model.scale);
            device_conv2d_backward_weight(d_gw3, d_a2, d_gh3, h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2);
            device_conv2d_backward_bias(d_gb3, d_gh3, h, w, model.c3);
            device_conv2d_backward_input(d_ga2, d_gh3, d_w3, h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2);
            device_relu_grad(d_gh2, d_ga2, d_h2_pre, h2_n);
            device_conv2d_backward_weight(d_gw2, d_a1, d_gh2, h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2);
            device_conv2d_backward_bias(d_gb2, d_gh2, h, w, model.c2);
            device_conv2d_backward_input(d_ga1, d_gh2, d_w2, h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2);
            device_relu_grad(d_gh1, d_ga1, d_h1_pre, h1_n);
            device_conv2d_backward_weight(d_gw1, x_dev.handle, d_gh1, h, w, model.in_c, h, w, model.c1, model.k1, 1, model.k1 / 2);
            device_conv2d_backward_bias(d_gb1, d_gh1, h, w, model.c1);

            step :: Int64 := epoch * TRAIN_CROPS + crop + 1;
            device_adamw_step(d_w1, d_m_w1, d_v_w1, d_gw1, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w1f.size());
            device_adamw_step(d_b1, d_m_b1, d_v_b1, d_gb1, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c1);
            device_adamw_step(d_w2, d_m_w2, d_v_w2, d_gw2, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w2f.size());
            device_adamw_step(d_b2, d_m_b2, d_v_b2, d_gb2, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c2);
            device_adamw_step(d_w3, d_m_w3, d_v_w3, d_gw3, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, w3f.size());
            device_adamw_step(d_b3, d_m_b3, d_v_b3, d_gb3, LR, BETA1, BETA2, EPS, WEIGHT_DECAY, step, model.c3);
        };

        train_loss :: Float64 := epoch_loss / float64(TRAIN_CROPS);
        device_copy_slice(d_x, d_val_x, 0, in_n);
        device_copy_slice(d_target, d_val_y, 0, out_n);
        device_conv2d(d_h1_pre, d_x, d_w1, d_b1, h, w, model.in_c, h, w, model.c1, model.k1, 1, model.k1 / 2, 1);
        device_relu(d_a1, d_h1_pre, h1_n);
        device_conv2d(d_h2_pre, d_a1, d_w2, d_b2, h, w, model.c1, h, w, model.c2, model.k2, 1, model.k2 / 2, 1);
        device_relu(d_a2, d_h2_pre, h2_n);
        device_conv2d(d_h3, d_a2, d_w3, d_b3, h, w, model.c2, h, w, model.c3, model.k3, 1, model.k3 / 2, 1);
        device_pixel_shuffle(d_pred, d_h3, h, w, model.c3, model.scale);
        device_mse_grad(d_loss_grad, d_loss_part, d_pred, d_target, out_n);
        device_sum(d_loss, d_loss_part, out_n);
        device_download(d_loss, loss_one, 1);
        val_loss :: Float64 := float64(loss_one[0]);

        train_losses[epoch] = train_loss;
        val_losses[epoch] = val_loss;
        if epoch == 0 { first_loss = train_loss; first_val = val_loss; }
        last_loss = train_loss;
        last_val = val_loss;
        out("epoch " + string(epoch) + " train " + string(train_loss) + " val " + string(val_loss));
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
out("first train: " + string(first_loss));
out("last train : " + string(last_loss));
out("first val  : " + string(first_val));
out("last val   : " + string(last_val));
if last_loss < first_loss { out("training : loss decreased"); } else { out("training : loss did not decrease"); exit(1); }

vfs.mkdir_p("build");
save_loss_csv(LOSS_CSV, train_losses, EPOCHS);
save_val_csv(VAL_CSV, val_losses, EPOCHS);
out("loss csv : " + LOSS_CSV);
out("val csv  : " + VAL_CSV);
save_trained_model(OUTPUT_MODEL, model, w1f, b1f, w2f, b2f, w3f, b3f);
save_trained_model(BEST_MODEL, model, w1f, b1f, w2f, b2f, w3f, b3f);
out("saved    : " + OUTPUT_MODEL);
out("best     : " + BEST_MODEL);

reloaded :: ESPCN := load_espcn(BEST_MODEL);
val_t :: vconv.Tensor := vconv.tensor(val_x64, h, w, input.channels);
pred_t :: vconv.Tensor := forward_cpu(reloaded, val_t);
reload_loss :: Float64 := tensor_mse(pred_t, val_y64, out_n);
out("reload val mse: " + string(reload_loss));
if reload_loss <= first_val { out("reload   : best checkpoint verified"); } else { out("reload   : best checkpoint did not improve validation"); exit(1); }
out("time     : " + string(float64(t1 - t0) / 1000000.0) + " ms");
out("=== done ===");
