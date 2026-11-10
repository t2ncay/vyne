# dataset.vy - build a larger synthetic SR pair set from the bundled reference.
#
#   vynec --compile src/dataset.vy && ./src/dataset
#
# The training image is a 256x256 montage: the reference tiled 2x2 with
# per-quadrant brightness/contrast/checkerboard variation. The LR image is
# a 2x downscale; the network input is that LR image bilinearly upscaled
# back to 256x256. The original reference is the target.

use native vfs;
use external "vimage/vimage.vy";

out("=== srcnn dataset ===");

ref :: vimage.Image := vimage.load("data/reference.png");
if ref.channels != 3 {
    throw "dataset: reference must be RGB";
}

W :: Int64 := ref.width * 2;
H :: Int64 := ref.height * 2;
C :: Int64 := 3;

pixels :: Array<Float64> := [];

through y :: 0..H-1 -> loop {
    through x :: 0..W-1 -> loop {
        sx :: Int64 := x % ref.width;
        sy :: Int64 := y % ref.height;
        qx :: Int64 := x / ref.width;
        qy :: Int64 := y / ref.height;
        q :: Int64 := qy * 2 + qx;
        base :: Int64 := (sy * ref.width + sx) * C;

        through c :: 0..C-1 -> loop {
            v :: Float64 := ref.pixels[base + c];

            if q == 1 {
                v = v * 0.90;
            } else if q == 2 {
                v = (v - 0.5) * 1.20 + 0.5;
            } else if q == 3 {
                checker :: Float64 := 0.2;
                if ((x / 8 + y / 8) % 2) == 0 {
                    checker = 0.8;
                }
                v = 0.5 * v + 0.5 * checker;
            }

            if v < 0.0 { v = 0.0; }
            if v > 1.0 { v = 1.0; }
            pixels.push(v);
        };
    };
};

hr :: vimage.Image := vimage.Image(W, H, C, pixels);
vfs.mkdir_p("data");
vimage.save("data/train_hr.png", hr);
out("wrote data/train_hr.png  (" + string(W) + "x" + string(H) + ")");

lr :: vimage.Image := vimage.resize(hr, W / 2, H / 2);
vimage.save("data/train_lr.png", lr);
out("wrote data/train_lr.png  (" + string(lr.width) + "x" + string(lr.height) + ")");

input_img :: vimage.Image := vimage.resize(lr, W, H);
vimage.save("data/train_input.png", input_img);
out("wrote data/train_input.png");

# A held-out pair from the raw reference (same dimensions as the template
# sample/reference pair) for quick sanity checks.
ref_lr :: vimage.Image := vimage.resize(ref, ref.width / 2, ref.height / 2);
vimage.save("data/val_lr.png", ref_lr);
ref_input :: vimage.Image := vimage.resize(ref_lr, ref.width, ref.height);
vimage.save("data/val_input.png", ref_input);
out("wrote data/val_lr.png + data/val_input.png");

out("=== done ===");
