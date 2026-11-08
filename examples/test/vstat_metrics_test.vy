# vstat_metrics_test.vy - checks for mse / mae / psnr / ssim.
#
# Run compiled:
#   ./vynec --compile examples/test/vstat_metrics_test.vy
#   ./examples/test/vstat_metrics_test

use external "vstat/vstat.vy";

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vstat metrics ===");

a :: Array<Float64> := [1.0, 2.0, 3.0];
b :: Array<Float64> := [1.0, 1.0, 3.0];
sa :: vstat.Series := vstat.Series(a, 3);
sb :: vstat.Series := vstat.Series(b, 3);

check("mse", close(vstat.mse(sa, sb), 1.0 / 3.0));
check("mae", close(vstat.mae(sa, sb), 1.0 / 3.0));
check("psnr", close(vstat.psnr(sa, sb, 1.0), 4.771212547196624));
check("psnr-identical", close(vstat.psnr(sa, sa, 1.0), 100.0));

# ---- SSIM on a small grayscale image ------------------------------
W :: Int64 := 8;
H :: Int64 := 8;
C :: Int64 := 1;
img := [];
through y :: 0..H-1 -> loop {
    through x :: 0..W-1 -> loop {
        img.push(float64((x * 7 + y * 13) % 32) / 31.0);
    };
};
s_img :: vstat.Series := vstat.Series(img, W * H * C);
check("ssim-identical", close(vstat.ssim(s_img, s_img, W, H, C), 1.0));

# Shift the image by two pixels horizontally. SSIM must drop.
shifted := [];
through y :: 0..H-1 -> loop {
    through x :: 0..W-1 -> loop {
        sx :: Int64 := (x + 2) % W;
        shifted.push(float64((sx * 7 + y * 13) % 32) / 31.0);
    };
};
s_shifted :: vstat.Series := vstat.Series(shifted, W * H * C);
s_val :: Float64 := vstat.ssim(s_img, s_shifted, W, H, C);
check("ssim-shift-lt-one", s_val < 0.999);
# The covariance term can be negative, so SSIM ranges over [-1, 1].
check("ssim-shift-in-range", s_val >= -1.0);

# Constant images exercise the zero-variance branch.
flat := [];
through i :: 0..(W * H * C)-1 -> loop { flat.push(0.5); };
s_flat :: vstat.Series := vstat.Series(flat, W * H * C);
check("ssim-flat", close(vstat.ssim(s_flat, s_flat, W, H, C), 1.0));

# ---- Validation ---------------------------------------------------
bad := false;
try {
    vstat.mse(sa, vstat.Series(a, 2));
} catch {
    bad = true;
}
check("mse-length-throws", bad);

bad2 := false;
try {
    vstat.ssim(s_img, s_img, 7, 8, 1);
} catch {
    bad2 = true;
}
check("ssim-shape-throws", bad2);

if failures[0] > 0 {
    out("vstat metrics: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vstat metrics: all checks passed");
