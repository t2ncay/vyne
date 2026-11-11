# metrics.vy - image metrics and CPU/GPU difference helpers.

use native vmath;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use external "vstat/vstat.vy";

fn psnr_img(a :: vimage.Image, b :: vimage.Image) -> Float64 {
    if a.width != b.width || a.height != b.height || a.channels != b.channels {
        throw "metrics.psnr_img: image shapes differ";
    }
    sa :: vstat.Series := vstat.Series(a.pixels, a.pixels.size());
    sb :: vstat.Series := vstat.Series(b.pixels, b.pixels.size());
    return vstat.psnr(sa, sb, 1.0);
}

fn ssim_img(a :: vimage.Image, b :: vimage.Image) -> Float64 {
    if a.width != b.width || a.height != b.height || a.channels != b.channels {
        throw "metrics.ssim_img: image shapes differ";
    }
    sa :: vstat.Series := vstat.Series(a.pixels, a.pixels.size());
    sb :: vstat.Series := vstat.Series(b.pixels, b.pixels.size());
    return vstat.ssim(sa, sb, a.width, a.height, a.channels);
}

fn tensor_as_image(t :: vconv.Tensor) -> vimage.Image {
    return vimage.Image(t.width, t.height, t.channels, t.data);
}

fn max_abs_diff(a :: vconv.Tensor, b :: vconv.Tensor) -> Float64 {
    if a.height != b.height || a.width != b.width || a.channels != b.channels {
        throw "metrics.max_abs_diff: tensor shapes differ";
    }
    m :: Float64 := 0.0;
    through i :: 0..a.data.size()-1 -> loop {
        d :: Float64 := a.data[i] - b.data[i];
        if d < 0.0 { d = 0.0 - d; }
        if d > m { m = d; }
    };
    return m;
}

fn luma(img :: vimage.Image, x :: Int64, y :: Int64) -> Float64 {
    base :: Int64 := (y * img.width + x) * img.channels;
    if img.channels == 1 { return img.pixels[base]; }
    return 0.299 * img.pixels[base] +
        0.587 * img.pixels[base + 1] +
        0.114 * img.pixels[base + 2];
}

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
