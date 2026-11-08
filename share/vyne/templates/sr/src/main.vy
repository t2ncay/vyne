# main.vy - ESPCN-style 2x upscaling demo with CUDA acceleration.
#
# Run from the project root:
#   vynec run src/main.vy

use native vfs;
use native vmath;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";

ruleset { dynamic_casting };

INPUT :: String := "data/sample.png";
REFERENCE :: String := "data/reference.png";
MODEL :: String := "data/espcn_x2.vwy";
OUTPUT :: String := "build/output_cuda.png";

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

out("=== gpu-trial - ESPCN 2x (CUDA) ===");
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

out("input      : " + string(input.width) + "x" + string(input.height) +
    "x" + string(input.channels));
out("model      : " + MODEL + "  (" + string(c1) + "->" + string(c2) +
    "->" + string(c3) + ", scale " + string(scale) + ")");

# Flatten the image to [H*W, 3] for the 1x1 convolution matmul
hw :: Int64 := input.height * input.width;
img_flat :: Array<Float32> := [];
through i :: 0..hw-1 -> loop {
    img_flat.push(float32(input.pixels[i * input.channels + 0]));
    img_flat.push(float32(input.pixels[i * input.channels + 1]));
    img_flat.push(float32(input.pixels[i * input.channels + 2]));
};

# Transpose weights to match matmul expectations [K, N]
# w1 is [c1, 3]. We need [3, c1].
w1_f32 :: Array<Float32> := [];
through r :: 0..3-1 -> loop {
    through c :: 0..c1-1 -> loop {
        w1_f32.push(float32(w1[c * 3 + r]));
    };
};

# w2 is [c2, c1]. We need [c1, c2].
w2_f32 :: Array<Float32> := [];
through r :: 0..c1-1 -> loop {
    through c :: 0..c2-1 -> loop {
        w2_f32.push(float32(w2[c * c1 + r]));
    };
};

# w3 is [c3, c2]. We need [c2, c3].
w3_f32 :: Array<Float32> := [];
through r :: 0..c2-1 -> loop {
    through c :: 0..c3-1 -> loop {
        w3_f32.push(float32(w3[c * c2 + r]));
    };
};

# Broadcast biases to [H*W, C]
b1_bc :: Array<Float32> := [];
through i :: 0..hw-1 -> loop {
    through c :: 0..c1-1 -> loop { b1_bc.push(float32(b1[c])); };
};

b2_bc :: Array<Float32> := [];
through i :: 0..hw-1 -> loop {
    through c :: 0..c2-1 -> loop { b2_bc.push(float32(b2[c])); };
};

b3_bc :: Array<Float32> := [];
through i :: 0..hw-1 -> loop {
    through c :: 0..c3-1 -> loop { b3_bc.push(float32(b3[c])); };
};

out_flat :: Array<Float32> := [];
through i :: 0..(hw*c3)-1 -> loop { out_flat.push(0.0f); };

@device<cuda> region gpu {
    out("device     : " + device_backend());

    # 1. Allocate and upload inputs & weights
    d_x  :: Int64 := device_alloc(hw * 3);
    d_w1 :: Int64 := device_alloc(3 * c1);
    d_w2 :: Int64 := device_alloc(c1 * c2);
    d_w3 :: Int64 := device_alloc(c2 * c3);
    
    device_upload(d_x,  img_flat, hw * 3);
    device_upload(d_w1, w1_f32, 3 * c1);
    device_upload(d_w2, w2_f32, c1 * c2);
    device_upload(d_w3, w3_f32, c2 * c3);

    d_b1 :: Int64 := device_alloc(hw * c1);
    d_b2 :: Int64 := device_alloc(hw * c2);
    d_b3 :: Int64 := device_alloc(hw * c3);
    
    device_upload(d_b1, b1_bc, hw * c1);
    device_upload(d_b2, b2_bc, hw * c2);
    device_upload(d_b3, b3_bc, hw * c3);

    # 2. Layer 1: Matmul -> Bias -> ReLU
    d_h1 :: Int64 := device_alloc(hw * c1);
    device_matmul(d_h1, d_x, d_w1, hw, c1, 3); # [hw,3] * [3,c1]
    device_add(d_h1, d_h1, d_b1, hw * c1);
    device_relu(d_h1, d_h1, hw * c1);

    # 3. Layer 2: Matmul -> Bias -> ReLU
    d_h2 :: Int64 := device_alloc(hw * c2);
    device_matmul(d_h2, d_h1, d_w2, hw, c2, c1); # [hw,c1] * [c1,c2]
    device_add(d_h2, d_h2, d_b2, hw * c2);
    device_relu(d_h2, d_h2, hw * c2);

    # 4. Layer 3: Matmul -> Bias (no activation before pixel shuffle)
    d_h3 :: Int64 := device_alloc(hw * c3);
    device_matmul(d_h3, d_h2, d_w3, hw, c3, c2); # [hw,c2] * [c2,c3]
    device_add(d_h3, d_h3, d_b3, hw * c3);

    # 5. Download results
    device_download(d_h3, out_flat, hw * c3);
    device_sync();
};

# Convert Float32 back to Float64 for vconv
out_f64 :: Array<Float64> := [];
through i :: 0..(hw*c3)-1 -> loop { out_f64.push(float64(out_flat[i])); };

# 6. Pixel Shuffle (CPU)
h3_tensor :: vconv.Tensor := vconv.tensor(out_f64, input.height, input.width, c3);
out_t :: vconv.Tensor := vconv.pixel_shuffle(h3_tensor, scale);

vfs.mkdir_p("build");
result :: vimage.Image := vimage.Image(
    out_t.width, out_t.height, out_t.channels, out_t.data
);
vimage.save(OUTPUT, result);

out("output     : " + OUTPUT + "  (" + string(out_t.width) + "x" +
    string(out_t.height) + "x" + string(out_t.channels) + ")");

# --- PSNR Evaluation ---
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
