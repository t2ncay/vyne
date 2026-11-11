# device_fn_helpers.vy - imported helpers for device_fn_test.vy.

@device_fn
fn imported_fill(d_tmp :: Int64, n :: Int64) {
    device_fill(d_tmp, 4.0, n);
}

@device_fn
fn imported_scale(d_out :: Int64, d_tmp :: Int64, n :: Int64) {
    imported_fill(d_tmp, n);
    device_scale(d_out, d_tmp, 2.0, n);
}
