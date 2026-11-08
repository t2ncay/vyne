# vimage/Types.vy - the public image container.
#
# Pixels are HWC (row-major, then channel) and Float64 in [0, 1].
# Keeping channels last means a tensor produced by the decoder can be
# handed to `vconv` without a transpose. `pixels.size()` is always
# width * height * channels.

ruleset { dynamic_casting };

module vimage;

interface Image {
    width    :: Int64,
    height   :: Int64,
    channels :: Int64,
    pixels   :: Array<Float64>,
}
