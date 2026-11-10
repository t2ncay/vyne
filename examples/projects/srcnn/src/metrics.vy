# metrics.vy - PSNR, global SSIM, edge energy, and a ringing proxy.
#
# The ringing proxy is the mean absolute Laplacian of the error image.
# A method that produces halos around edges leaves high-frequency error
# there, so lower is better. It is a proxy, not a calibrated metric.

use native vmath;
use external "vimage/vimage.vy";

fn luma(img :: vimage.Image, x :: Int64, y :: Int64) -> Float64 {
    c :: Int64 := img.channels;
    base :: Int64 := (y * img.width + x) * c;
    if c == 1 { return img.pixels[base]; }
    r :: Float64 := img.pixels[base];
    g :: Float64 := img.pixels[base + 1];
    b :: Float64 := img.pixels[base + 2];
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

fn psnr(a :: vimage.Image, b :: vimage.Image) -> Float64 {
    if a.width != b.width || a.height != b.height || a.channels != b.channels {
        throw "metrics.psnr: image shapes differ";
    }
    n :: Int64 := a.pixels.size();
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := a.pixels[i] - b.pixels[i];
        s = s + d * d;
    };
    mse :: Float64 := s / float64(n);
    if mse <= 0.0 { return 100.0; }
    return 10.0 * vmath.log10(1.0 / mse);
}

fn ssim(a :: vimage.Image, b :: vimage.Image) -> Float64 {
    if a.width != b.width || a.height != b.height {
        throw "metrics.ssim: image shapes differ";
    }
    n :: Int64 := a.width * a.height;
    ma :: Float64 := 0.0;
    mb :: Float64 := 0.0;
    through y :: 0..a.height-1 -> loop {
        through x :: 0..a.width-1 -> loop {
            ma = ma + luma(a, x, y);
            mb = mb + luma(b, x, y);
        };
    };
    ma = ma / float64(n);
    mb = mb / float64(n);

    va :: Float64 := 0.0;
    vb :: Float64 := 0.0;
    cov :: Float64 := 0.0;
    through y2 :: 0..a.height-1 -> loop {
        through x2 :: 0..a.width-1 -> loop {
            da :: Float64 := luma(a, x2, y2) - ma;
            db :: Float64 := luma(b, x2, y2) - mb;
            va = va + da * da;
            vb = vb + db * db;
            cov = cov + da * db;
        };
    };
    va = va / float64(n);
    vb = vb / float64(n);
    cov = cov / float64(n);

    c1 :: Float64 := 0.01 * 0.01;
    c2 :: Float64 := 0.03 * 0.03;
    num :: Float64 := (2.0 * ma * mb + c1) * (2.0 * cov + c2);
    den :: Float64 := (ma * ma + mb * mb + c1) * (va + vb + c2);
    return num / den;
}

# Mean absolute first difference of luma. Higher means more high-frequency
# edge energy; useful for checking that a method is not just blurring.
fn edge_energy(img :: vimage.Image) -> Float64 {
    count :: Int64 := 0;
    s :: Float64 := 0.0;
    through y :: 0..img.height-2 -> loop {
        through x :: 0..img.width-2 -> loop {
            l0 :: Float64 := luma(img, x, y);
            dx :: Float64 := luma(img, x + 1, y) - l0;
            dy :: Float64 := luma(img, x, y + 1) - l0;
            if dx < 0.0 { dx = 0.0 - dx; }
            if dy < 0.0 { dy = 0.0 - dy; }
            s = s + dx + dy;
            count = count + 1;
        };
    };
    if count == 0 { return 0.0; }
    return s / float64(count);
}

# Mean absolute Laplacian of the error. Lower is better for ringing/halos.
fn ringing_proxy(pred :: vimage.Image, ref :: vimage.Image) -> Float64 {
    if pred.width != ref.width || pred.height != ref.height {
        throw "metrics.ringing_proxy: image shapes differ";
    }
    count :: Int64 := 0;
    s :: Float64 := 0.0;
    through y :: 1..pred.height-2 -> loop {
        through x :: 1..pred.width-2 -> loop {
            e0 :: Float64 := luma(pred, x, y) - luma(ref, x, y);
            el :: Float64 := luma(pred, x - 1, y) - luma(ref, x - 1, y);
            er :: Float64 := luma(pred, x + 1, y) - luma(ref, x + 1, y);
            eu :: Float64 := luma(pred, x, y - 1) - luma(ref, x, y - 1);
            ed :: Float64 := luma(pred, x, y + 1) - luma(ref, x, y + 1);
            lap :: Float64 := 4.0 * e0 - el - er - eu - ed;
            if lap < 0.0 { lap = 0.0 - lap; }
            s = s + lap;
            count = count + 1;
        };
    };
    if count == 0 { return 0.0; }
    return s / float64(count);
}
