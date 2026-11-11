# upscale.vy - Lanczos-3 and an RCAS-style contrast-adaptive sharpen.
#
# Used only as a non-neural baseline. It is an FSR-1-style approximation, not a
# bit-exact AMD FSR implementation.

use native vmath;
use external "vimage/vimage.vy";

fn sinc(x :: Float64) -> Float64 {
    if x == 0.0 { return 1.0; }
    pi :: Float64 := 3.141592653589793;
    px :: Float64 := pi * x;
    return vmath.sin(px) / px;
}

fn lanczos3_kernel(x :: Float64) -> Float64 {
    ax :: Float64 := x;
    if ax < 0.0 { ax = 0.0 - ax; }
    if ax >= 3.0 { return 0.0; }
    return sinc(x) * sinc(x / 3.0);
}

fn lanczos3(src :: vimage.Image, out_w :: Int64,
                out_h :: Int64) -> vimage.Image {
    if out_w <= 0 || out_h <= 0 {
        throw "upscale.lanczos3: dimensions must be positive";
    }
    c :: Int64 := src.channels;
    dst :: Array<Float64> := [];
    sx_scale :: Float64 := float64(src.width) / float64(out_w);
    sy_scale :: Float64 := float64(src.height) / float64(out_h);

    through oy :: 0..out_h-1 -> loop {
        cy :: Float64 := (float64(oy) + 0.5) * sy_scale - 0.5;
        if cy < 0.0 { cy = 0.0; }
        if cy > float64(src.height - 1) { cy = float64(src.height - 1); }
        y0 :: Int64 := int64(cy) - 2;

        through ox :: 0..out_w-1 -> loop {
            cx :: Float64 := (float64(ox) + 0.5) * sx_scale - 0.5;
            if cx < 0.0 { cx = 0.0; }
            if cx > float64(src.width - 1) { cx = float64(src.width - 1); }
            x0 :: Int64 := int64(cx) - 2;

            wsum :: Float64 := 0.0;
            acc :: Array<Float64> := [];
            through k :: 0..c-1 -> loop { acc.push(0.0); };

            through ky :: 0..5 -> loop {
                sy :: Int64 := y0 + ky;
                if sy < 0 { sy = 0; }
                if sy > src.height - 1 { sy = src.height - 1; }
                wy :: Float64 := lanczos3_kernel(cy - float64(sy));
                through kx :: 0..5 -> loop {
                    sx :: Int64 := x0 + kx;
                    if sx < 0 { sx = 0; }
                    if sx > src.width - 1 { sx = src.width - 1; }
                    wv :: Float64 := wy * lanczos3_kernel(cx - float64(sx));
                    wsum = wsum + wv;
                    base :: Int64 := (sy * src.width + sx) * c;
                    through ch :: 0..c-1 -> loop {
                        acc[ch] = acc[ch] + wv * src.pixels[base + ch];
                    };
                };
            };

            if wsum != 0.0 {
                through ch2 :: 0..c-1 -> loop { acc[ch2] = acc[ch2] / wsum; };
            }
            through ch3 :: 0..c-1 -> loop {
                v :: Float64 := acc[ch3];
                if v < 0.0 { v = 0.0; }
                if v > 1.0 { v = 1.0; }
                dst.push(v);
            };
        };
    };

    return vimage.Image(out_w, out_h, c, dst);
}

fn rcas(img :: vimage.Image, amount :: Float64) -> vimage.Image {
    c :: Int64 := img.channels;
    dst :: Array<Float64> := [];
    through y :: 0..img.height-1 -> loop {
        through x :: 0..img.width-1 -> loop {
            through ch :: 0..c-1 -> loop {
                lo :: Float64 := 1.0;
                hi :: Float64 := 0.0;
                sum :: Float64 := 0.0;
                cnt :: Int64 := 0;
                through dy :: -1..1 -> loop {
                    through dx :: -1..1 -> loop {
                        sx :: Int64 := x + dx;
                        sy :: Int64 := y + dy;
                        if sx < 0 { sx = 0; }
                        if sx > img.width - 1 { sx = img.width - 1; }
                        if sy < 0 { sy = 0; }
                        if sy > img.height - 1 { sy = img.height - 1; }
                        pv :: Float64 := img.pixels[(sy * img.width + sx) * c + ch];
                        if pv < lo { lo = pv; }
                        if pv > hi { hi = pv; }
                        if dx != 0 || dy != 0 { sum = sum + pv; cnt = cnt + 1; }
                    };
                };
                center :: Float64 := img.pixels[(y * img.width + x) * c + ch];
                avg :: Float64 := center;
                if cnt > 0 { avg = sum / float64(cnt); }
                outv :: Float64 := center + amount * (center - avg);
                if outv < lo { outv = lo; }
                if outv > hi { outv = hi; }
                if outv < 0.0 { outv = 0.0; }
                if outv > 1.0 { outv = 1.0; }
                dst.push(outv);
            };
        };
    };
    return vimage.Image(img.width, img.height, c, dst);
}

fn fsr1_like(src :: vimage.Image, out_w :: Int64,
                 out_h :: Int64) -> vimage.Image {
    up :: vimage.Image := lanczos3(src, out_w, out_h);
    return rcas(up, 0.45);
}
