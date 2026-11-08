# vstat/Metrics.vy - error and similarity metrics for image work.
#
# The image libraries keep pixels as HWC Float64 in [0, 1], which is the
# input convention here. `mse` and `mae` are exact reductions over the
# native kernel; `psnr` wraps `mse`; `ssim` uses a uniform 8x8 window
# (not a Gaussian) so it stays a compact, dependency-free implementation.
# For published comparisons, state the window shape alongside the number.

use "Types.vy";
use "Kernels.vy";
use native vmath;

ruleset { dynamic_casting };

module vstat;

# Mean squared error between two equally sized series.
fn :: vstat mse(a :: vstat.Series, b :: vstat.Series) -> Float64 {
    if a.n != b.n {
        throw "vstat.mse: series differ in length";
    }
    if a.n <= 0 {
        throw "vstat.mse: empty series";
    }
    return vstat.k_mse_sum(a.data, b.data, a.n) / float64(a.n);
}

# Mean absolute error between two equally sized series.
fn :: vstat mae(a :: vstat.Series, b :: vstat.Series) -> Float64 {
    if a.n != b.n {
        throw "vstat.mae: series differ in length";
    }
    if a.n <= 0 {
        throw "vstat.mae: empty series";
    }
    return vstat.k_mae_sum(a.data, b.data, a.n) / float64(a.n);
}

# Peak signal-to-noise ratio in decibels.
#
# `max_value` is the data range (1.0 for normalised images, 255.0 for
# 8-bit buffers). Identical inputs return a finite sentinel of 100 dB
# instead of +inf so downstream arithmetic stays well-defined.
fn :: vstat psnr(
    a :: vstat.Series,
    b :: vstat.Series,
    max_value :: Float64
) -> Float64 {
    if max_value <= 0.0 {
        throw "vstat.psnr: max_value must be positive";
    }
    err :: Float64 := vstat.mse(a, b);
    if err <= 0.0 {
        return 100.0;
    }
    return 10.0 * vmath.log10((max_value * max_value) / err);
}

# Structural similarity over an HWC image buffer.
#
# `width`, `height`, and `channels` describe the buffer layout; `a.n`
# and `b.n` must equal width*height*channels. The result is the mean
# SSIM over all valid 8x8 windows and all channels. Identical inputs
# return 1.0.
fn :: vstat ssim(
    a :: vstat.Series,
    b :: vstat.Series,
    width :: Int64,
    height :: Int64,
    channels :: Int64
) -> Float64 {
    if width <= 0 || height <= 0 || channels <= 0 {
        throw "vstat.ssim: dimensions must be positive";
    }
    expected :: Int64 := width * height * channels;
    if a.n != expected || b.n != expected {
        throw "vstat.ssim: series length does not match dimensions";
    }

    win :: Int64 := 8;
    if width < win || height < win {
        throw "vstat.ssim: image is smaller than the 8x8 window";
    }

    # Constants for normalised [0, 1] data: C1 = (0.01 L)^2,
    # C2 = (0.03 L)^2.
    c1 :: Float64 := 0.0001;
    c2 :: Float64 := 0.0009;
    win_f :: Float64 := float64(win * win);

    total :: Float64 := 0.0;
    count :: Int64 := 0;

    through ch :: 0..channels-1 -> loop {
        through y :: 0..height-win -> loop {
            through x :: 0..width-win -> loop {
                sx :: Float64 := 0.0;
                sy :: Float64 := 0.0;
                sxx :: Float64 := 0.0;
                syy :: Float64 := 0.0;
                sxy :: Float64 := 0.0;

                through dy :: 0..win-1 -> loop {
                    through dx :: 0..win-1 -> loop {
                        idx :: Int64 :=
                            ((y + dy) * width + (x + dx)) * channels + ch;
                        xv :: Float64 := a.data[idx];
                        yv :: Float64 := b.data[idx];
                        sx = sx + xv;
                        sy = sy + yv;
                        sxx = sxx + xv * xv;
                        syy = syy + yv * yv;
                        sxy = sxy + xv * yv;
                    };
                };

                mx :: Float64 := sx / win_f;
                my :: Float64 := sy / win_f;
                vx :: Float64 := sxx / win_f - mx * mx;
                vy :: Float64 := syy / win_f - my * my;
                cxy :: Float64 := sxy / win_f - mx * my;
                if vx < 0.0 { vx = 0.0; }
                if vy < 0.0 { vy = 0.0; }

                num :: Float64 := (2.0 * mx * my + c1) * (2.0 * cxy + c2);
                den :: Float64 := (mx * mx + my * my + c1) * (vx + vy + c2);
                if den != 0.0 {
                    total = total + num / den;
                    count = count + 1;
                }
            };
        };
    };

    if count == 0 {
        return 0.0;
    }
    return total / float64(count);
}
