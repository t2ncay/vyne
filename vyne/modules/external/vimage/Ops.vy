# vimage/Ops.vy - PNG/JPEG/BMP/TGA decode and encode.
#
# The heavy lifting lives in the native `vimage` runtime module, which is
# a thin wrapper over stb_image / stb_image_write. This file is the
# typed, validated surface: it turns the raw header-prefixed array into
# an `Image`, checks dimensions before resampling, and routes saves by
# file extension.

use "Types.vy";
use native vimage;

ruleset { dynamic_casting };

module vimage;

# Load any format stb_image understands (PNG, JPEG, BMP, TGA, GIF, PSD,
# PNM, HDR, PIC). The source bit depth is normalised to Float64 [0, 1].
# Throws when the file is missing or corrupt.
fn :: vimage load(path :: String) -> vimage.Image {
    raw :: Array<Float64> := vimage.load_raw(path);
    if raw.size() < 3 {
        throw "vimage.load: could not decode '" + path + "'";
    }

    w :: Int64 := int64(raw[0]);
    h :: Int64 := int64(raw[1]);
    c :: Int64 := int64(raw[2]);
    if w <= 0 || h <= 0 || c <= 0 {
        throw "vimage.load: decoded image has invalid dimensions";
    }

    pixels :: Array<Float64> := vimage.pixels(raw);
    expected :: Int64 := w * h * c;
    if pixels.size() != expected {
        throw "vimage.load: pixel buffer size does not match dimensions";
    }

    return vimage.Image(w, h, c, pixels);
}

# Encode by extension: .png (default), .jpg/.jpeg, .bmp, .tga.
# Quality is used only for JPEG; 90 is a reasonable default.
fn :: vimage save(path :: String, img :: vimage.Image) -> Bool {
    return vimage.save_with_quality(path, img, 90);
}

fn :: vimage save_with_quality(
    path :: String,
    img :: vimage.Image,
    quality :: Int64
) -> Bool {
    pixels :: Array<Float64> := img.pixels;
    return vimage.save_raw(
        path,
        pixels,
        img.width,
        img.height,
        img.channels,
        quality
    );
}

# PNG in, PNG out shortcut used by the examples.
fn :: vimage save_png(path :: String, img :: vimage.Image) -> Bool {
    return vimage.save_with_quality(path, img, 90);
}

# Bilinear resample with centre-aligned coordinate mapping.
fn :: vimage resize(
    img :: vimage.Image,
    new_width :: Int64,
    new_height :: Int64
) -> vimage.Image {
    if new_width <= 0 || new_height <= 0 {
        throw "vimage.resize: dimensions must be positive";
    }
    pixels :: Array<Float64> := img.pixels;
    dst :: Array<Float64> := vimage.resize_bilinear(
        pixels,
        img.width,
        img.height,
        img.channels,
        new_width,
        new_height
    );
    return vimage.Image(new_width, new_height, img.channels, dst);
}

# Crop a rectangle. The rectangle is clamped to the image bounds.
fn :: vimage crop(
    img :: vimage.Image,
    x :: Int64,
    y :: Int64,
    crop_width :: Int64,
    crop_height :: Int64
) -> vimage.Image {
    if crop_width <= 0 || crop_height <= 0 {
        throw "vimage.crop: dimensions must be positive";
    }

    x0 :: Int64 := x;
    y0 :: Int64 := y;
    cw :: Int64 := crop_width;
    ch :: Int64 := crop_height;
    if x0 < 0 { cw = cw + x0; x0 = 0; }
    if y0 < 0 { ch = ch + y0; y0 = 0; }
    if x0 >= img.width || y0 >= img.height || cw <= 0 || ch <= 0 {
        throw "vimage.crop: rectangle is outside the image";
    }
    if x0 + cw > img.width { cw = img.width - x0; }
    if y0 + ch > img.height { ch = img.height - y0; }
    if cw <= 0 || ch <= 0 {
        throw "vimage.crop: rectangle is outside the image";
    }

    pixels :: Array<Float64> := img.pixels;
    dst :: Array<Float64> := vimage.crop_raw(
        pixels,
        img.width,
        img.height,
        img.channels,
        x0,
        y0,
        cw,
        ch
    );
    if dst.size() != cw * ch * img.channels {
        throw "vimage.crop: native crop returned an unexpected size";
    }

    return vimage.Image(cw, ch, img.channels, dst);
}

# Rec.601 luma. RGB/RGBA input becomes a one-channel image.
fn :: vimage grayscale(img :: vimage.Image) -> vimage.Image {
    if img.channels < 1 {
        throw "vimage.grayscale: image has no channels";
    }
    if img.channels == 1 {
        return img;
    }
    pixels :: Array<Float64> := img.pixels;
    dst :: Array<Float64> := vimage.grayscale_raw(
        pixels,
        img.width,
        img.height,
        img.channels
    );
    return vimage.Image(img.width, img.height, 1, dst);
}

# Allocate a zero-filled image.
fn :: vimage blank(
    width :: Int64,
    height :: Int64,
    channels :: Int64
) -> vimage.Image {
    if width <= 0 || height <= 0 || channels <= 0 {
        throw "vimage.blank: dimensions must be positive";
    }
    n :: Int64 := width * height * channels;
    pixels :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { pixels.push(0.0); };
    return vimage.Image(width, height, channels, pixels);
}

# Read one sample. Bounds are checked; out-of-range reads return 0.0 so
# a sampling bug does not crash an inference run.
fn :: vimage pixel(
    img :: vimage.Image,
    x :: Int64,
    y :: Int64,
    channel :: Int64
) -> Float64 {
    if x < 0 || x >= img.width { return 0.0; }
    if y < 0 || y >= img.height { return 0.0; }
    if channel < 0 || channel >= img.channels { return 0.0; }
    idx :: Int64 := (y * img.width + x) * img.channels + channel;
    return img.pixels[idx];
}

# Write one sample. Silently ignores out-of-range coordinates.
fn :: vimage set_pixel(
    img :: vimage.Image,
    x :: Int64,
    y :: Int64,
    channel :: Int64,
    value :: Float64
) {
    if x < 0 || x >= img.width { return; }
    if y < 0 || y >= img.height { return; }
    if channel < 0 || channel >= img.channels { return; }
    idx :: Int64 := (y * img.width + x) * img.channels + channel;
    img.pixels[idx] = value;
}
