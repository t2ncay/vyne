# vconv - convolution for Vyne

`vconv` is the numerical core of the image-super-resolution path. It is a
CPU implementation of square-kernel convolution with stride and zero
padding, the common activations, ESPCN pixel shuffle, a weight-bundle
loader, and explicit backward kernels for training. The loops live in
`vyne/runtime/modules/vconv.h`; this library is the typed wrapper.

```vyne
use external "vconv/vconv.vy";

x :: vconv.Tensor := vconv.tensor(pixels, height, width, channels);
y :: vconv.Tensor := vconv.conv2d(x, weights, bias, 64, 5, 1, 2);
y = vconv.tanh(y);
y = vconv.pixel_shuffle(y, 2);
```

## Layout

Tensors are **HWC**, the same layout as `vimage.Image`:

```
index = (y * width + x) * channels + c
```

Conv weights use PyTorch's **OIHW** order:

```
index = (((oc * in_c) + ic) * kernel + ky) * kernel + kx
```

That means a `torch.nn.Conv2d` state dict exports without a transpose.
Bias is a length-`out_channels` vector.

## API

### Construction

| Function | Result |
| --- | --- |
| `vconv.tensor(data, h, w, c)` | wrap an HWC buffer (no copy) |
| `vconv.zeros(h, w, c)` | zero-filled tensor |

### Layers

| Function | Result |
| --- | --- |
| `vconv.conv2d(input, weight, bias, out_c, kernel, stride, pad)` | conv2d, optional bias |
| `vconv.relu(input)` | `max(0, x)` |
| `vconv.leaky_relu(input, alpha)` | leaky ReLU |
| `vconv.tanh(input)` | tanh |
| `vconv.sigmoid(input)` | logistic |
| `vconv.pixel_shuffle(input, scale)` | ESPCN depth-to-space |

Output shape for convolution:

```
out_h = (in_h + 2*pad - kernel) / stride + 1
out_w = (in_w + 2*pad - kernel) / stride + 1
```

`pixel_shuffle` requires `channels % scale^2 == 0` and maps
`input[y][x][c*scale^2 + i*scale + j]` to
`output[y*scale + i][x*scale + j][c]`.

### CPU backward (training)
| Function | Result |
| :--- | :--- |
| `conv_backward_input(grad, weight, in_h, in_w, in_c, k, stride, pad)` | gradient w.r.t. the input |
| `conv_backward_weight(input, grad, k, stride, pad)` | gradient w.r.t. the OIHW weight |
| `conv_backward_bias(grad)` | gradient w.r.t. the bias |
| `relu_backward(grad, pre_activation)` | ReLU mask applied to an upstream gradient |
`weight` in `conv_backward_input` is passed as an `Array` so the wrapper
stays outside the native-array ABI.


`vconv.load_model(path)` reads a bundle into a `Map` of
`name -> Array<Float64>`. Two formats are accepted:

**Binary `.vwy`** (preferred):

```
"VYW1"
u32 tensor_count
repeat:
  u32 name_length, u8[] name
  u32 ndim, u32[] dims
  f32[] values
```

**Text `.vwt`** (fixtures and hand editing):

```
VYWT1
<tensor_count>
<name> <ndim> <d0> <d1> ... <values...>
```

`vconv.get_tensor(model, name)` throws when a tensor is absent or empty.
`scripts/export_espcn_weights.py` converts a PyTorch ESPCN checkpoint to
`.vwy` and also writes the deterministic passthrough model used by the
example.

## Shape validation

`conv2d`, `tensor`, `pixel_shuffle`, and the activation wrappers check
shapes before touching the kernel and throw a Vyne error on a mismatch.
The native functions themselves return an empty array on invalid input,
so the wrapper is the only place a caller needs to reason about.

## Explicit device API

`vconv.conv2d` and the tensor-returning wrappers are CPU operations. Inside an
active `@device` region, the following lower-level calls allocate their output
on the selected backend and return an **Int64 device handle**. They do not copy
host arrays or return `vconv.Tensor` values.

| Handle-returning function | Operation |
| :--- | :--- |
| `vconv_conv2d_fwd_device(x, w, b, in_h, in_w, in_c, out_c, kh, kw, stride, pad)` | Forward square-kernel convolution (`kh == kw`) |
| `vconv_conv2d_backward_input_device(grad, w, in_h, in_w, in_c, out_h, out_w, out_c, kh, kw, stride, pad)` | Input gradient (`kh == kw`) |
| `vconv_conv2d_backward_weight_device(x, grad, in_h, in_w, in_c, out_h, out_w, out_c, kh, kw, stride, pad)` | Weight gradient (`kh == kw`) |
| `vconv_conv2d_backward_bias_device(grad, out_h, out_w, out_c)` | Bias gradient |
| `vconv_relu_fwd_device(x, n)` | ReLU |
| `vconv_relu_backward_device(grad, pre, n)` | ReLU gradient |
| `vconv_pixel_shuffle_fwd_device(x, h, w, c, r)` | Depth-to-space |
| `vconv_pixel_shuffle_backward_device(grad, h, w, c, r)` | Pixel-shuffle gradient |

For reusable training buffers, use preallocated `device_conv2d_*`,
`device_relu_grad`, `device_pixel_shuffle*` intrinsics or the
[vnn](../vnn/README.md) helpers over [vtensor](../vtensor/README.md).
Device handles must stay within their owning region. Dtype defaults to Float32;
all handles participating in an operation must agree. See
[device regions](../../../../docs/src/language/device.md) and
[ESPCN](../../../../examples/projects/espcn/README.md).

## Limits

- `vconv.conv2d` is CPU. The `@device` backend now has `device_conv2d` and
  handle-returning vconv device wrappers for conv forward/backward, ReLU
  forward/backward, pixel shuffle forward, and pixel shuffle backward. The SR
  projects use those explicit wrappers for full-device inference/training.
  Optimizer/loss training primitives such as `device_mse_grad`,
  `device_sgd_step`, and `device_adamw_step` live in the `@device` intrinsic
  surface; `vconv` itself does not auto-dispatch.
- The tensor-returning backward wrappers above execute on CPU and cover the
  square-kernel stride/pad cases above. Explicit `device_conv2d_backward_*`
  intrinsics execute on the active device backend instead.
- No dilation or groups yet.
- Direct square-kernel convolution; no real-time 1080p->4K performance guarantee.

## Tests

```bash
./vynec --compile examples/test/vconv_test.vy
./examples/test/vconv_test
```

The test covers single- and multi-channel convolution, padding, stride,
pixel shuffle, activations, text weight loading, and shape validation.

## Backend support

`vconv` is a native runtime module. Use the compiled backend
(`vynec --compile`). The AST interpreter does not bind the convolution
kernels yet; that is tracked with the other native-module interpreter
gaps.
