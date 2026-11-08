# vimage_test.vy - behavioural checks for the vimage library.
#
# Run compiled:
#   ./vynec --compile examples/test/vimage_test.vy
#   ./examples/test/vimage_test

use external "vimage/vimage.vy";
use native vfs;

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn close8(a :: Float64, b :: Float64) -> Bool {
    # Round-trip through 8-bit storage allows ~1/255 of quantisation.
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= 0.002;
}

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vimage ===");

w :: Int64 := 5;
h :: Int64 := 3;
c :: Int64 := 3;

pixels :: Array<Float64> := [];
through i :: 0..(w * h * c)-1 -> loop {
    pixels.push(float64((i * 37) % 256) / 255.0);
};

img :: vimage.Image := vimage.Image(w, h, c, pixels);
check("ctor-size", img.pixels.size() == w * h * c);

path :: String := "vimage_test_tmp.png";
ok :: Bool := vimage.save(path, img);
check("save-png", ok);
check("file-exists", vfs.exists(path));

loaded :: vimage.Image := vimage.load(path);
check("load-width", loaded.width == w);
check("load-height", loaded.height == h);
check("load-channels", loaded.channels == c);
check("load-size", loaded.pixels.size() == w * h * c);

# Every sample should survive the 8-bit round-trip within 1/255.
max_err :: Float64 := 0.0;
through i :: 0..(w * h * c)-1 -> loop {
    d :: Float64 := loaded.pixels[i] - img.pixels[i];
    if d < 0.0 { d = 0.0 - d; }
    if d > max_err { max_err = d; }
};
check("roundtrip-error", max_err <= 0.002);

check("pixel-access", close8(vimage.pixel(loaded, 0, 0, 0), pixels[0]));
check("pixel-oob", close(vimage.pixel(loaded, 99, 99, 0), 0.0));

# Resize doubles the canvas and keeps channels.
resized :: vimage.Image := vimage.resize(img, 10, 6);
check("resize-width", resized.width == 10);
check("resize-height", resized.height == 6);
check("resize-channels", resized.channels == c);
check("resize-size", resized.pixels.size() == 10 * 6 * c);

# Crop a 2x2 rectangle.
cropped :: vimage.Image := vimage.crop(img, 1, 1, 2, 2);
check("crop-width", cropped.width == 2);
check("crop-height", cropped.height == 2);
check("crop-size", cropped.pixels.size() == 2 * 2 * c);

# Grayscale collapses RGB to one channel and preserves luminance.
gray :: vimage.Image := vimage.grayscale(img);
check("gray-channels", gray.channels == 1);
check("gray-size", gray.pixels.size() == w * h);
g0 :: Float64 := 0.299 * pixels[0] + 0.587 * pixels[1] + 0.114 * pixels[2];
check("gray-luma", close(gray.pixels[0], g0));

# Blank allocates a zero-filled image.
blank :: vimage.Image := vimage.blank(4, 4, 3);
check("blank-size", blank.pixels.size() == 48);
check("blank-zero", close(blank.pixels[20], 0.0));

# JP/JPEG/BMP/TGA encode paths all succeed.
ok_jpg :: Bool := vimage.save("vimage_test_tmp.jpg", img);
check("save-jpg", ok_jpg);
ok_bmp :: Bool := vimage.save("vimage_test_tmp.bmp", img);
check("save-bmp", ok_bmp);
ok_tga :: Bool := vimage.save("vimage_test_tmp.tga", img);
check("save-tga", ok_tga);

# Decode an image with one channel as well.
mono :: vimage.Image := vimage.blank(3, 2, 1);
mono.pixels[0] = 0.5;
mono.pixels[1] = 0.25;
ok_mono :: Bool := vimage.save("vimage_test_tmp_mono.png", mono);
check("save-mono", ok_mono);
mono_in :: vimage.Image := vimage.load("vimage_test_tmp_mono.png");
check("load-mono-channels", mono_in.channels == 1);
check("load-mono-value", close8(mono_in.pixels[0], 0.5));

vfs.remove(path);
vfs.remove("vimage_test_tmp.jpg");
vfs.remove("vimage_test_tmp.bmp");
vfs.remove("vimage_test_tmp.tga");
vfs.remove("vimage_test_tmp_mono.png");
check("cleanup", !vfs.exists(path));

if failures[0] > 0 {
    out("vimage: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vimage: all checks passed");
