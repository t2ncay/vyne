# vtensor_vnn_test.vy - reusable device tensor/NN helper smoke test.
#
#   ./vynec --compile examples/test/vtensor_vnn_test.vy
#   ./examples/test/vtensor_vnn_test

use external "vtensor/vtensor.vy";
use external "vnn/vnn.vy";

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else { out("FAIL " + name); failures[0] = failures[0] + 1; }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

out("=== vtensor/vnn ===");

packed :: Array<Float32> := [9.0f, 1.0f, 3.0f];
target :: Array<Float32> := [0.0f, 1.0f];
slice_out :: Array<Float32> := [0.0f, 0.0f];
grad_out :: Array<Float32> := [0.0f, 0.0f];
loss_out :: Array<Float32> := [0.0f];
param_out :: Array<Float32> := [1.0f, 2.0f];
m_out :: Array<Float32> := [0.0f, 0.0f];
v_out :: Array<Float32> := [0.0f, 0.0f];
hyper :: vnn.AdamWHyper := vnn.adamw_hyper(0.1, 0.9, 0.999, 1e-8, 0.0);

@device<cuda> region r {
    d_packed :: Int64 := device_alloc(3);
    d_x :: Int64 := device_alloc(2);
    d_target :: Int64 := device_alloc(2);
    d_grad :: Int64 := device_alloc(2);
    d_part :: Int64 := device_alloc(2);
    d_loss :: Int64 := device_alloc(1);
    d_param :: Int64 := device_alloc(2);
    d_m :: Int64 := device_alloc(2);
    d_v :: Int64 := device_alloc(2);

    x_t :: vtensor.DeviceTensor := vtensor.tensor(d_x, 1, 2, 1);
    y_t :: vtensor.DeviceTensor := vtensor.tensor(d_target, 1, 2, 1);

    device_upload(d_packed, packed, 3);
    device_upload(d_target, target, 2);
    device_upload(d_param, param_out, 2);
    device_fill(d_m, 0.0, 2);
    device_fill(d_v, 0.0, 2);

    vtensor.copy_slice(x_t, d_packed, 1);
    vnn.mse_grad_sum(d_grad, d_part, d_loss, x_t, y_t);
    vnn.adamw(d_param, d_m, d_v, d_grad, hyper, 1, 2);

    device_download(d_x, slice_out, 2);
    device_download(d_grad, grad_out, 2);
    device_download(d_loss, loss_out, 1);
    device_download(d_param, param_out, 2);
    device_download(d_m, m_out, 2);
    device_download(d_v, v_out, 2);
    device_sync();
};

check("slice-0", close(float64(slice_out[0]), 1.0, 1e-6));
check("slice-1", close(float64(slice_out[1]), 3.0, 1e-6));
check("grad-0", close(float64(grad_out[0]), 1.0, 1e-6));
check("grad-1", close(float64(grad_out[1]), 2.0, 1e-6));
check("loss", close(float64(loss_out[0]), 2.5, 1e-6));
check("adamw-param-0", close(float64(param_out[0]), 0.9, 1e-5));
check("adamw-param-1", close(float64(param_out[1]), 1.9, 1e-5));
check("adamw-m-0", close(float64(m_out[0]), 0.1, 1e-6));
check("adamw-v-1", close(float64(v_out[1]), 0.004, 1e-6));

if failures[0] != 0 {
    out("vtensor/vnn: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
