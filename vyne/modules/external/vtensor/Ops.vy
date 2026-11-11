# vtensor/Ops.vy - small helpers over @device handles.
#
# DeviceTensor does not own memory. Its handle is valid only inside the active
# @device region that allocated it.

use "Types.vy";

ruleset { dynamic_casting };

module vtensor;

fn :: vtensor tensor(handle :: Int64, height :: Int64, width :: Int64,
                     channels :: Int64) -> vtensor.DeviceTensor {
    if height <= 0 || width <= 0 || channels <= 0 {
        throw "vtensor.tensor: dimensions must be positive";
    }
    return vtensor.DeviceTensor(handle, height, width, channels,
                                height * width * channels);
}

@device_fn
fn :: vtensor copy_slice(dst :: vtensor.DeviceTensor,
                         packed :: Int64,
                         offset :: Int64) {
    device_copy_slice(dst.handle, packed, offset, dst.n);
}
