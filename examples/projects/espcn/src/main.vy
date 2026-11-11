# main.vy - ESPCN inference benchmark: CPU reference, CUDA path, baselines.
#
#   ../../../vynec --compile src/main.vy && ./src/main

use native vcore;
use native vfs;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use "model.vy";
use "gpu.vy";
use "metrics.vy";
use "upscale.vy";

ruleset { dynamic_casting };

INPUT :: String := "data/sample.png";
REFERENCE :: String := "data/reference.png";
MODEL :: String := "data/espcn_x2.vwy";
TRAINED_MODEL :: String := "build/espcn_gpu_trained.vwt";
BEST_MODEL :: String := "build/espcn_gpu_best.vwt";

fn print_quality(name :: String, img :: vimage.Image, ref :: vimage.Image) {
    out(name + " PSNR : " + string(psnr_img(img, ref)) + " dB");
    out(name + " SSIM : " + string(ssim_img(img, ref)));
    out(name + " edge : " + string(edge_energy(img)));
    out(name + " ring : " + string(ringing_proxy(img, ref)));
}

out("=== Vyne ESPCN project ===");

input :: vimage.Image := vimage.load(INPUT);
reference :: vimage.Image := vimage.load(REFERENCE);
model_path :: String := MODEL;
if vfs.exists(TRAINED_MODEL) { model_path = TRAINED_MODEL; }
if vfs.exists(BEST_MODEL) { model_path = BEST_MODEL; }
model :: ESPCN := load_espcn(model_path);

out("input     : " + string(input.width) + "x" + string(input.height) +
    "x" + string(input.channels));
out("reference : " + string(reference.width) + "x" + string(reference.height) +
    "x" + string(reference.channels));
out("model path: " + model_path);
out("model     : " + string(model.c1) + " -> " + string(model.c2) +
    " -> " + string(model.c3) + "  kernels " + string(model.k1) + "/" +
    string(model.k2) + "/" + string(model.k3) + "  scale " +
    string(model.scale) + "  act " + activation_name(model.act));
out("");

input_t :: vconv.Tensor := vconv.tensor(
    input.pixels, input.height, input.width, input.channels
);

baseline0 :: vimage.Image := vimage.resize(input, reference.width, reference.height);
baseline1 :: vimage.Image := fsr1_like(input, reference.width, reference.height);

t0 :: Int64 := vcore.now_ns();
cpu_t :: vconv.Tensor := forward_cpu(model, input_t);
t1 :: Int64 := vcore.now_ns();
cpu_img :: vimage.Image := tensor_as_image(cpu_t);

gpu_ran :: Bool := false;
gpu_t :: vconv.Tensor := cpu_t;
gpu_ms :: Float64 := 0.0;
if gpu_supported(model) {
    tg0 :: Int64 := vcore.now_ns();
    gpu_t = forward_gpu(model, input_t);
    tg1 :: Int64 := vcore.now_ns();
    gpu_ms = float64(tg1 - tg0) / 1000000.0;
    gpu_ran = true;
} else {
    out("device    : skipped; activation " + activation_name(model.act) +
        " needs CPU exact path");
}

cpu_ms :: Float64 := float64(t1 - t0) / 1000000.0;
out("cpu time  : " + string(cpu_ms) + " ms");
if gpu_ran {
    out("gpu time  : " + string(gpu_ms) + " ms");
    out("max diff  : " + string(max_abs_diff(cpu_t, gpu_t)));
}

vfs.mkdir_p("build");
vimage.save("build/output_bilinear.png", baseline0);
vimage.save("build/output_fsr1_like.png", baseline1);
vimage.save("build/output_espcn_cpu.png", cpu_img);
if gpu_ran {
    gpu_img :: vimage.Image := tensor_as_image(gpu_t);
    vimage.save("build/output_espcn_gpu.png", gpu_img);
}

out("");
out("quality vs " + REFERENCE + ":");
print_quality("bilinear", baseline0, reference);
out("");
print_quality("fsr-like", baseline1, reference);
out("");
print_quality("espcn-cpu", cpu_img, reference);
if gpu_ran {
    out("");
    print_quality("espcn-gpu", tensor_as_image(gpu_t), reference);
}

out("");
out("outputs   : build/output_bilinear.png");
out("            build/output_fsr1_like.png");
out("            build/output_espcn_cpu.png");
if gpu_ran { out("            build/output_espcn_gpu.png"); }
out("=== done ===");
