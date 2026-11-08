# main.vy - ESPCN-style 2x upscaling demo.
#
# Run from the project root:
#   vynec run src/main.vy
#
# The bundled data/espcn_x2.vwy is a deterministic passthrough model:
# 1x1 identity convolutions plus a sub-pixel fan-out. It exercises the
# same decode -> conv -> pixel-shuffle -> encode path a trained ESPCN
# uses. Replace it with a real checkpoint exported by
# scripts/export_espcn_weights.py; the `meta` tensor in the bundle
# selects the channel widths, kernel sizes, scale, and activation.

use native vfs;
use native vmath;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";

ruleset { dynamic_casting };

INPUT :: String := "data/sample.png";
REFERENCE :: String := "data/reference.png";
MODEL :: String := "data/espcn_x2.vwy";
OUTPUT :: String := "build/output.png";

fn psnr(a :: Array<Float64>, b :: Array<Float64>, n :: Int64) -> Float64 {
    mse :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := a[i] - b[i];
        mse = mse + d * d;
    };
    mse = mse / float64(n);
    if mse <= 0.0 { return 100.0; }
    return 10.0 * vmath.log10(1.0 / mse);
}

fn activate(t :: vconv.Tensor, code :: Int64) -> vconv.Tensor {
    if code == 0 { return vconv.relu(t); }
    if code == 2 { return vconv.leaky_relu(t, 0.1); }
    return vconv.tanh(t);
}

out("=== {{name}} - ESPCN 2x ===");
out("");

input :: vimage.Image := vimage.load(INPUT);
reference :: vimage.Image := vimage.load(REFERENCE);
model :: Map := vconv.load_model(MODEL);

meta :: Array<Float64> := model["meta"];
scale :: Int64 := 2;
c1 :: Int64 := 3;
c2 :: Int64 := 3;
c3 :: Int64 := 12;
k1 :: Int64 := 1;
k2 :: Int64 := 1;
k3 :: Int64 := 1;
act :: Int64 := 0;

if meta.size() >= 9 {
    scale = int64(meta[0]);
    c1 = int64(meta[2]);
    c2 = int64(meta[3]);
    c3 = int64(meta[4]);
    k1 = int64(meta[5]);
    k2 = int64(meta[6]);
    k3 = int64(meta[7]);
    act = int64(meta[8]);
}

w1 :: Array<Float64> := vconv.get_tensor(model, "conv1.weight");
b1 :: Array<Float64> := vconv.get_tensor(model, "conv1.bias");
w2 :: Array<Float64> := vconv.get_tensor(model, "conv2.weight");
b2 :: Array<Float64> := vconv.get_tensor(model, "conv2.bias");
w3 :: Array<Float64> := vconv.get_tensor(model, "conv3.weight");
b3 :: Array<Float64> := vconv.get_tensor(model, "conv3.bias");

x :: vconv.Tensor := vconv.tensor(
    input.pixels, input.height, input.width, input.channels
);

out("input      : " + string(input.width) + "x" + string(input.height) +
    "x" + string(input.channels));
out("model      : " + MODEL + "  (" + string(c1) + "->" + string(c2) +
    "->" + string(c3) + ", scale " + string(scale) + ")");

h1 :: vconv.Tensor := vconv.conv2d(x, w1, b1, c1, k1, 1, k1 / 2);
a1 :: vconv.Tensor := activate(h1, act);
h2 :: vconv.Tensor := vconv.conv2d(a1, w2, b2, c2, k2, 1, k2 / 2);
a2 :: vconv.Tensor := activate(h2, act);
h3 :: vconv.Tensor := vconv.conv2d(a2, w3, b3, c3, k3, 1, k3 / 2);
out_t :: vconv.Tensor := vconv.pixel_shuffle(h3, scale);

vfs.mkdir_p("build");
result :: vimage.Image := vimage.Image(
    out_t.width, out_t.height, out_t.channels, out_t.data
);
vimage.save(OUTPUT, result);

out("output     : " + OUTPUT + "  (" + string(out_t.width) + "x" +
    string(out_t.height) + "x" + string(out_t.channels) + ")");

if reference.width == out_t.width && reference.height == out_t.height &&
    reference.channels == out_t.channels {
    baseline :: vimage.Image := vimage.resize(
        input, reference.width, reference.height
    );
    n :: Int64 := out_t.data.size();
    out("");
    out("PSNR vs reference:");
    out("  bilinear : " + string(psnr(
        baseline.pixels, reference.pixels, n
    )) + " dB");
    out("  ESPCN    : " + string(psnr(
        out_t.data, reference.pixels, n
    )) + " dB");
}
