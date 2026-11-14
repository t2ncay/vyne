# Optional host image adapter. Import this only in image/export programs.
use "vplot.vy";
use external "vimage/vimage.vy";
ruleset { dynamic_casting };
module vplot;

fn :: vplot to_image(surface :: Int64) -> vimage.Image {
    if vplot.surface_width(surface) < 1 || vplot.surface_height(surface) < 1 {
        throw "vplot.to_image: invalid surface";
    }
    pixels :: Array<Float64> := vplot.image_pixels(surface);
    return vimage.Image(vplot.surface_width(surface), vplot.surface_height(surface), 3, pixels);
}

fn :: vplot from_image(image :: vimage.Image) -> Int64 {
    return vplot.image_bind(image.pixels, image.width, image.height, image.channels);
}

fn :: vplot save_png(surface :: Int64, path :: String) -> Bool {
    image :: vimage.Image := vplot.to_image(surface);
    return vimage.save(path, image);
}
