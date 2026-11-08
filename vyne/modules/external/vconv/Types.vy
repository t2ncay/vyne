# vconv/Types.vy - the public tensor container.
#
# Tensors are HWC (row-major, then channel), matching `vimage.Image` and
# PyTorch's memory order after a channel-last view. Keeping the same
# layout as the image decoder means the ESPCN path needs no transposes.

ruleset { dynamic_casting };

module vconv;

interface Tensor {
    data     :: Array<Float64>,
    height   :: Int64,
    width    :: Int64,
    channels :: Int64,
}
