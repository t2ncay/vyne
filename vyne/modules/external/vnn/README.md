# vnn - device neural-network helpers for Vyne

`vnn` is the first reusable layer over `@device` intrinsics and `vtensor` shape
metadata. It is intentionally explicit: callers allocate device buffers in an
active `@device` region and pass handles/metadata into `@device_fn` helpers.

```vyne
use external "vtensor/vtensor.vy";
use external "vnn/vnn.vy";

@device<cuda> region r {
    # allocate handles first
    vnn.conv2d_relu(pre, act, input, weight, bias, 64, 5);
};
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
| `adamw_hyper(...)` | host-side AdamW hyperparameter bundle |
| `conv2d(out, input, weight, bias, out_c, k)` | preallocated device conv2d |
| `conv2d_relu(pre, act, input, weight, bias, out_c, k)` | conv + ReLU |
| `pixel_shuffle(out, input, scale)` | ESPCN depth-to-space |
| `mse_grad_sum(grad, partial, loss, pred, target)` | device MSE grad and scalar loss |
| `pixel_shuffle_backward(grad_in, grad_out, scale)` | pixel-shuffle backward |
| `conv2d_backward(...)` | conv input/weight/bias gradients |
| `conv2d_relu_backward(...)` | ReLU backward + conv backward |
| `adamw(param, m, v, grad, hyper, step, n)` | in-place device AdamW |
