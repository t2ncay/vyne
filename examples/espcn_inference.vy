# espcn_inference.vy - ESPCN-style super-resolution inference in Vyne.
#
# Run compiled (the image and convolution kernels are native):
#   ./vynec --compile examples/espcn_inference.vy
#   ./examples/espcn_inference
#
# What this demonstrates
# ----------------------
#   PNG/JPEG decode -> 3-layer convolutional network -> pixel shuffle ->
#   PNG encode, with every stage in the Vyne stack. If a trained weight
#   bundle is present at MODEL_PATH it is used. Otherwise the program
#   builds a deterministic 1x1 passthrough network, which is equivalent
#   to nearest-neighbour 2x upscaling; that keeps the demo runnable
#   without shipping weights, and exercises the same conv/pixel-shuffle
#   code path a trained model uses.
#
# Produce a trained bundle with:
#   python scripts/export_espcn_weights.py --checkpoint espcn_x2.pth \
#       --scale 2 --out models/espcn_x2.vwy
#
# Optional ground truth: if INPUT_HR_PATH exists, the program reports
# PSNR/SSIM against it and against a bilinear baseline.

ruleset { dynamic_casting };

use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use external "vstat/vstat.vy";
use native vcore;
use native vfs;

INPUT_PATH :: String := "examples/assets/sr/input.png";
MODEL_PATH :: String := "models/espcn_x2.vwy";
OUTPUT_PATH :: String := "build/espcn_output.png";
INPUT_HR_PATH :: String := "examples/assets/sr/input_hr.png";

# ---------------------------------------------------------------------
# Procedural fallback image: a colour grid with a soft diagonal. It has
# edges (so nearest-neighbour banding is visible) and flat regions.
# ---------------------------------------------------------------------
fn make_test_image(width :: Int64, height :: Int64) -> vimage.Image {
    pixels :: Array<Float64> := [];
    through y :: 0..height-1 -> loop {
        through x :: 0..width-1 -> loop {
            fx :: Float64 := float64(x) / float64(width - 1);
            fy :: Float64 := float64(y) / float64(height - 1);
            r :: Float64 := fx;
            g :: Float64 := fy;
            b :: Float64 := 0.5 + 0.5 * (fx - fy);
            if ((x / 8) + (y / 8)) % 2 == 0 {
                r = r * 0.75;
                g = g * 0.75;
                b = b * 0.75;
            }
            pixels.push(r);
            pixels.push(g);
            pixels.push(b);
        };
    };
    return vimage.Image(width, height, 3, pixels);
}

# ---------------------------------------------------------------------
# Passthrough weights: conv1/conv2 are channel-preserving 1x1
# identities, conv3 routes each input channel into its four 2x2
# sub-pixels. Pixel shuffle then performs nearest-neighbour 2x.
# ---------------------------------------------------------------------
fn identity_1x1(channels :: Int64) -> Array<Float64> {
    w :: Array<Float64> := [];
    through oc :: 0..channels-1 -> loop {
        through ic :: 0..channels-1 -> loop {
            if oc == ic { w.push(1.0); } else { w.push(0.0); }
        };
    };
    return w;
}

fn zeros_of(n :: Int64) -> Array<Float64> {
    z :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { z.push(0.0); };
    return z;
}

fn passthrough_upsample_weights(channels :: Int64, scale :: Int64)
    -> Array<Float64> {
    rr :: Int64 := scale * scale;
    out_channels :: Int64 := channels * rr;
    w :: Array<Float64> := [];
    through oc :: 0..out_channels-1 -> loop {
        through ic :: 0..channels-1 -> loop {
            # oc = ic*rr + i*scale + j
            if oc / rr == ic { w.push(1.0); } else { w.push(0.0); }
        };
    };
    return w;
}

# ---------------------------------------------------------------------
# Model description. `meta` is [scale, in_c, c1, c2, c3, k1, k2, k3,
# activation_code] where 0=relu, 1=tanh, 2=leaky_relu.
# ---------------------------------------------------------------------
fn activate(t :: vconv.Tensor, code :: Int64) -> vconv.Tensor {
    if code == 0 { return vconv.relu(t); }
    if code == 2 { return vconv.leaky_relu(t, 0.1); }
    return vconv.tanh(t);
}

fn run(input :: vimage.Image) -> vconv.Tensor {
    input_t :: vconv.Tensor := vconv.tensor(
        input.pixels,
        input.height,
        input.width,
        input.channels
    );

    if vfs.exists(MODEL_PATH) {
        model :: Map := vconv.load_model(MODEL_PATH);
        meta :: Array<Float64> := model["meta"];

        scale :: Int64 := 2;
        in_c :: Int64 := input.channels;
        c1 :: Int64 := 64;
        c2 :: Int64 := 32;
        c3 :: Int64 := input.channels * 4;
        k1 :: Int64 := 5;
        k2 :: Int64 := 3;
        k3 :: Int64 := 3;
        act :: Int64 := 1;

        if meta.size() >= 9 {
            scale = int64(meta[0]);
            in_c = int64(meta[1]);
            c1 = int64(meta[2]);
            c2 = int64(meta[3]);
            c3 = int64(meta[4]);
            k1 = int64(meta[5]);
            k2 = int64(meta[6]);
            k3 = int64(meta[7]);
            act = int64(meta[8]);
        }
        if in_c != input.channels {
            throw "espcn_inference: model expects " + string(in_c) +
                " input channels, image has " + string(input.channels);
        }

        w1 :: Array<Float64> := vconv.get_tensor(model, "conv1.weight");
        b1 :: Array<Float64> := vconv.get_tensor(model, "conv1.bias");
        w2 :: Array<Float64> := vconv.get_tensor(model, "conv2.weight");
        b2 :: Array<Float64> := vconv.get_tensor(model, "conv2.bias");
        w3 :: Array<Float64> := vconv.get_tensor(model, "conv3.weight");
        b3 :: Array<Float64> := vconv.get_tensor(model, "conv3.bias");

        out("model      : trained bundle " + MODEL_PATH);
        out("layers     : " + string(c1) + " -> " + string(c2) + " -> " +
            string(c3));
        out("kernels    : " + string(k1) + "/" + string(k2) + "/" +
            string(k3) + "  scale " + string(scale));

        t0 :: Int64 := vcore.now_ns();
        h1 :: vconv.Tensor := vconv.conv2d(
            input_t, w1, b1, c1, k1, 1, k1 / 2
        );
        a1 :: vconv.Tensor := activate(h1, act);
        h2 :: vconv.Tensor := vconv.conv2d(
            a1, w2, b2, c2, k2, 1, k2 / 2
        );
        a2 :: vconv.Tensor := activate(h2, act);
        h3 :: vconv.Tensor := vconv.conv2d(
            a2, w3, b3, c3, k3, 1, k3 / 2
        );
        out_t :: vconv.Tensor := vconv.pixel_shuffle(h3, scale);
        t1 :: Int64 := vcore.now_ns();

        out("inference  : " + string((t1 - t0) / 1000000) + " ms  (" +
            string(input.width) + "x" + string(input.height) + " -> " +
            string(out_t.width) + "x" + string(out_t.height) + ")");
        return out_t;
    }

    # Fallback: a deterministic passthrough network. Same ops, no
    # pretrained weights.
    in_c :: Int64 := input.channels;
    scale :: Int64 := 2;
    c1 :: Int64 := in_c;
    c2 :: Int64 := in_c;
    c3 :: Int64 := in_c * scale * scale;
    act :: Int64 := 0;

    w1 :: Array<Float64> := identity_1x1(in_c);
    b1 :: Array<Float64> := zeros_of(c1);
    w2 :: Array<Float64> := identity_1x1(c1);
    b2 :: Array<Float64> := zeros_of(c2);
    w3 :: Array<Float64> := passthrough_upsample_weights(in_c, scale);
    b3 :: Array<Float64> := zeros_of(c3);

    out("model      : built-in passthrough (nearest-neighbour 2x)");
    out("layers     : " + string(c1) + " -> " + string(c2) + " -> " +
        string(c3));
    out("kernels    : 1/1/1  scale 2");

    t0 :: Int64 := vcore.now_ns();
    h1 :: vconv.Tensor := vconv.conv2d(input_t, w1, b1, c1, 1, 1, 0);
    a1 :: vconv.Tensor := activate(h1, act);
    h2 :: vconv.Tensor := vconv.conv2d(a1, w2, b2, c2, 1, 1, 0);
    a2 :: vconv.Tensor := activate(h2, act);
    h3 :: vconv.Tensor := vconv.conv2d(a2, w3, b3, c3, 1, 1, 0);
    out_t :: vconv.Tensor := vconv.pixel_shuffle(h3, scale);
    t1 :: Int64 := vcore.now_ns();

    out("inference  : " + string((t1 - t0) / 1000000) + " ms  (" +
        string(input.width) + "x" + string(input.height) + " -> " +
        string(out_t.width) + "x" + string(out_t.height) + ")");
    return out_t;
}

# ---------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------
out("=== Vyne ESPCN inference ===");
out("");

input :: vimage.Image := vimage.blank(1, 1, 3);
if vfs.exists(INPUT_PATH) {
    input = vimage.load(INPUT_PATH);
    out("input      : " + INPUT_PATH + "  (" + string(input.width) + "x" +
        string(input.height) + ", " + string(input.channels) + " ch)");
} else {
    input = make_test_image(64, 64);
    out("input      : generated 64x64 test pattern (no " + INPUT_PATH + ")");
    vimage.save("build/espcn_input.png", input);
}

result :: vconv.Tensor := run(input);
result_img :: vimage.Image := vimage.Image(
    result.width,
    result.height,
    result.channels,
    result.data
);

if vfs.exists("build") || vfs.exists("build/") {
    ok :: Bool := vimage.save(OUTPUT_PATH, result_img);
    if ok {
        out("output     : " + OUTPUT_PATH);
    } else {
        out("output     : could not write " + OUTPUT_PATH);
    }
} else {
    ok2 :: Bool := vimage.save("espcn_output.png", result_img);
    if ok2 {
        out("output     : espcn_output.png");
    }
}

# Optional quality numbers when a ground-truth high-resolution image
# exists. The bilinear baseline is the same decoder + resize, so the
# two lines are directly comparable.
if vfs.exists(INPUT_HR_PATH) {
    hr :: vimage.Image := vimage.load(INPUT_HR_PATH);
    bilinear :: vimage.Image := vimage.resize(input, hr.width, hr.height);

    if hr.width == result.width && hr.height == result.height &&
        hr.channels == result.channels {
        out("");
        out("quality vs " + INPUT_HR_PATH + ":");

        gt :: vstat.Series := vstat.Series(hr.pixels, hr.pixels.size());
        sr :: vstat.Series := vstat.Series(
            result.data,
            result.data.size()
        );
        bl :: vstat.Series := vstat.Series(
            bilinear.pixels,
            bilinear.pixels.size()
        );

        out("  bilinear PSNR : " + string(vstat.psnr(bl, gt, 1.0)) + " dB");
        out("  ESPCN    PSNR : " + string(vstat.psnr(sr, gt, 1.0)) + " dB");
        out("  bilinear SSIM : " +
            string(vstat.ssim(bl, gt, hr.width, hr.height, hr.channels)));
        out("  ESPCN    SSIM : " +
            string(vstat.ssim(sr, gt, hr.width, hr.height, hr.channels)));
    } else {
        out("quality    : ground truth size does not match output; skipped");
    }
}

out("");
out("done.");
