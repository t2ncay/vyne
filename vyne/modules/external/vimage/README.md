# vimage - PNG/JPEG/BMP/TGA image I/O for Vyne

`vimage` is the image codec layer for the SR stack. Decoding and encoding
are handled by the vendored `stb_image` / `stb_image_write` single-header
libraries; the Vyne side is a typed `Image` interface plus shape-checked
operations. There is no new link dependency: stb is compiled into the
generated program through the `vimage` runtime header.

```vyne
use external "vimage/vimage.vy";

img :: vimage.Image := vimage.load("input.png");   # HWC, Float64 [0,1]
out("size: " + string(img.width) + "x" + string(img.height));

half :: vimage.Image := vimage.resize(img, img.width / 2, img.height / 2);
vimage.save("half.png", half);
```

## Pixel convention

- Layout is **HWC**: `index = (y * width + x) * channels + c`.
- Samples are `Float64` in `[0, 1]`. 8-bit source data is divided by 255
  on load and multiplied back on save.
- `channels` is whatever the file actually contains (1, 3, or 4 are the
  common cases). `vimage.grayscale` collapses RGB/RGBA to one channel.

The HWC layout matches `vconv.Tensor`, so an image can be wrapped as a
tensor without a transpose.

## API

### Decode / encode

| Function | Result |
| --- | --- |
| `vimage.load(path)` | decode PNG/JPEG/BMP/TGA/GIF/PSD/PNM into an `Image` |
| `vimage.save(path, img)` | encode by extension (`quality = 90` for JPEG) |
| `vimage.save_with_quality(path, img, q)` | JPEG quality 0..100 |
| `vimage.save_png(path, img)` | PNG shortcut |

Decode failures throw with the path in the message. Encode failures
return `false` (for example, a missing directory or an unsupported
extension falls back to PNG).

### Geometry

| Function | Result |
| --- | --- |
| `vimage.resize(img, w, h)` | centre-aligned bilinear resample |
| `vimage.crop(img, x, y, w, h)` | clamped rectangle crop |
| `vimage.grayscale(img)` | Rec.601 luma, one channel |
| `vimage.blank(w, h, c)` | zero-filled image |

### Sample access

| Function | Result |
| --- | --- |
| `vimage.pixel(img, x, y, c)` | one sample; out-of-range reads return `0.0` |
| `vimage.set_pixel(img, x, y, c, v)` | in-place write; out-of-range writes are ignored |

## Implementation notes

- The native surface is `load_raw`, `pixels`, `save_raw`,
  `resize_bilinear`, `crop_raw`, and `grayscale_raw` in
  `vyne/runtime/modules/vimage.h`. The `_raw` names avoid colliding
  with the public Vyne wrappers.
- `load_raw` returns a flat array with a three-element header
  `[width, height, channels]`. `pixels` returns an O(1) view with the
  header removed, so decoding does not copy the payload twice.
- Filters are computed in Float64 and quantised only at encode time.

## Tests

```bash
./vynec --compile examples/test/vimage_test.vy
./examples/test/vimage_test
```

The test exercises PNG/JPEG/BMP/TGA round-trips, resize, crop,
grayscale, and single-channel decode.

## Backend support

`vimage` is a native runtime module. Use the compiled backend
(`vynec --compile`). The AST interpreter does not bind the image codecs
yet; that is tracked with the other native-module interpreter gaps.
