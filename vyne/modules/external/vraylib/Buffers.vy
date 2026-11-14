use native vraylib;
ruleset { dynamic_casting };
module vraylib;

# Buffers are native allocations, independent of the caller's arena/region.
# C pointer parameters use these handles, never integer-encoded addresses.
fn :: vraylib float_buffer(values :: Array<Float64>) -> Int64 {
    handle :: Int64 := vraylib.buffer("float", values.size());
    if values.size() == 0 { return handle; }
    through i :: 0..values.size() - 1 -> loop {
        vraylib.buffer_write(handle, i, values[i]);
    };
    return handle;
}

fn :: vraylib int_buffer(values :: Array<Int64>) -> Int64 {
    handle :: Int64 := vraylib.buffer("int", values.size());
    if values.size() == 0 { return handle; }
    through i :: 0..values.size() - 1 -> loop {
        vraylib.buffer_write(handle, i, values[i]);
    };
    return handle;
}

fn :: vraylib bytes(values :: Array<Int64>) -> Int64 {
    handle :: Int64 := vraylib.buffer("unsigned char", values.size());
    if values.size() == 0 { return handle; }
    through i :: 0..values.size() - 1 -> loop {
        vraylib.buffer_write(handle, i, values[i]);
    };
    return handle;
}

fn :: vraylib resource_int(handle :: Int64, field :: String) -> Int64 {
    return vraylib.field(handle, field, 0);
}
