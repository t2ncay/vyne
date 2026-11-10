# main.vy - run the residual SRCNN, the FSR-1-style baseline, and bilinear.
#
#   vynec --compile src/main.vy && ./src/main
#
# Uses the generated synthetic HR/LR pair because the template's bundled`r`n# reference.png is itself a bilinear upscale, which makes bilinear look`r`n# perfect. Writes build/output_bilinear.png, build/output_fsr1.png, and
# build/output_vyne_sr.png, then prints PSNR, global SSIM, edge energy,
# and a ringing proxy against data/reference.png.

use native vfs;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use "metrics.vy";
use "upscale.vy";
use "model.vy";
use "gpu.vy";

out("=== Vyne neural upscaler ===");

INPUT :: String := "data/train_lr.png";
REFERENCE :: String := "data/train_hr.png";
MODEL :: String := "data/srcnn_x2.vwt";

input :: vimage.Image := vimage.load(INPUT);
reference :: vimage.Image := vimage.load(REFERENCE);
model :: SRCNN := model_from_map(vconv.load_model(MODEL));

out("input     : " + string(input.width) + "x" + string(input.height) + "x" + string(input.channels));
out("reference : " + string(reference.width) + "x" + string(reference.height) + "x" + string(reference.channels));

# The network sees the bilinear-upscaled low-resolution image.
bilinear :: vimage.Image := vimage.resize(input, reference.width, reference.height);
x :: vconv.Tensor := vconv.tensor(bilinear.pixels, bilinear.height, bilinear.width, bilinear.channels);
out("");
@device<cuda> region probe {
    out("device    : " + device_backend());
};
sr_t :: vconv.Tensor := forward_gpu(model, x);
sr :: vimage.Image := vimage.Image(sr_t.width, sr_t.height, sr_t.channels, sr_t.data);

fsr :: vimage.Image := fsr1_like(input, reference.width, reference.height);

vfs.mkdir_p("build");
vimage.save("build/output_bilinear.png", bilinear);
vimage.save("build/output_fsr1.png", fsr);
vimage.save("build/output_vyne_sr.png", sr);

out("");
out("method      PSNR(dB)   SSIM      edge      ringing");
out("bilinear    " + string(psnr(bilinear, reference)) + "   " + string(ssim(bilinear, reference)) + "   " + string(edge_energy(bilinear)) + "   " + string(ringing_proxy(bilinear, reference)));
out("fsr1-like   " + string(psnr(fsr, reference)) + "   " + string(ssim(fsr, reference)) + "   " + string(edge_energy(fsr)) + "   " + string(ringing_proxy(fsr, reference)));
out("vyne-sr     " + string(psnr(sr, reference)) + "   " + string(ssim(sr, reference)) + "   " + string(edge_energy(sr)) + "   " + string(ringing_proxy(sr, reference)));

out("");
out("outputs: build/output_bilinear.png");
out("         build/output_fsr1.png");
out("         build/output_vyne_sr.png");
out("=== done ===");
