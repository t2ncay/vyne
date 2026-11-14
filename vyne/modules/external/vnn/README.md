# vnn - device neural-network helpers for Vyne

`vnn` is the first reusable layer over `@device` intrinsics and `vtensor` shape
metadata. It is intentionally explicit: callers allocate device buffers in an
active `@device` region and pass handles/metadata into `@device_fn` helpers.

```vyne
use external "vtensor/vtensor.vy";
use external "vnn/vnn.vy";

input :: Array<Float32> := [1.0f, 3.0f];
target :: Array<Float32> := [0.0f, 1.0f];
loss_host :: Array<Float32> := [0.0f];
@device<cuda> region r {
    x :: Int64 := device_alloc(2);
    y :: Int64 := device_alloc(2);
    grad :: Int64 := device_alloc(2);
    partial :: Int64 := device_alloc(2);
    loss :: Int64 := device_alloc(1);
    device_upload(x, input, 2);
    device_upload(y, target, 2);
    pred :: vtensor.DeviceTensor := vtensor.tensor(x, 1, 2, 1);
    truth :: vtensor.DeviceTensor := vtensor.tensor(y, 1, 2, 1);
    vnn.mse_grad_sum(grad, partial, loss, pred, truth);
    device_download(loss, loss_host, 1);
};
out(loss_host[0]);  # 2.5
```

## Current Scope

- HWC image tensors.
- Square-kernel stride-1 same-padding conv2d.
- ReLU forward/backward.
- ESPCN pixel shuffle forward/backward.
- MSE gradient plus device scalar loss reduction.
- AdamW updates for device-resident parameter/moment buffers.

This is not automatic tensor AD. It is a small explicit device-training surface
used by `examples/projects/espcn` before a larger public ML API is frozen.

## API

| Function | Result |
| :--- | :--- |
| `adamw_hyper(lr, beta1, beta2, eps, weight_decay)` | host-side AdamW hyperparameter bundle |
| `conv2d(out, input, weight, bias, out_c, k)` | preallocated device conv2d |
| `conv2d_relu(pre, act, input, weight, bias, out_c, k)` | conv + ReLU |
| `pixel_shuffle(out, input, scale)` | ESPCN depth-to-space |
| `mse_grad_sum(grad, partial, loss, pred, target)` | device MSE grad and scalar loss |
| `pixel_shuffle_backward(grad_in, grad_out, scale)` | pixel-shuffle backward |
| `conv2d_backward(grad_input, grad_weight, grad_bias, input, grad_out, weight, out_c, k)` | conv input/weight/bias gradients |
| `conv2d_relu_backward(grad_input, grad_weight, grad_bias, grad_pre, input, grad_act, pre, weight, out_c, k)` | ReLU backward + conv backward |
| `adamw(param, m, v, grad, hyper, step, n)` | in-place device AdamW |

## Buffer and lifetime rules

Tensor arguments carry `vtensor.DeviceTensor` metadata; parameter, gradient,
moment, partial-loss, and scalar-loss buffers use Int64 handles. Callers allocate
all buffers inside the device region and supply matching shapes and dtypes.
`conv2d_relu` needs distinct preactivation and activation buffers so backward
can reuse the saved preactivation. Loss reduction needs `n` partial elements and
one scalar result. Initialize AdamW moments with `device_fill(..., 0.0, n)` and
use a one-based step count. Kernels write preallocated outputs; helpers do not
own or free buffers and do not infer a training graph.

## Examples and verification

The complete checkpoint/training workflow is in
[ESPCN](../../../../examples/projects/espcn/README.md).
The regression below checks crop staging, MSE gradients/reduction, and AdamW;
it uses CUDA when available and the CPU device backend otherwise.

```shell
vynec --run examples/test/vtensor_vnn_test.vy
```
