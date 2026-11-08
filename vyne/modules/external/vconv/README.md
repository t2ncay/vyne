# vconv - forward convolution for Vyne

`vconv` is the numerical core of the image-super-resolution path. It is a
forward-only CPU implementation today: square-kernel convolution with
stride and zero padding, the common activations, ESPCN pixel shuffle, and
a weight-bundle loader. The loops live in
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

### Weight bundles

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

## Limits

- CPU only. `@device<backend: cuda>` integration is a follow-up; the
  kernel signatures are stable enough to swap the body later.
- Forward only. Training needs a backward pass (tracked separately).
- No dilation or groups yet.
- Naive direct convolution. It is fast enough for the ESPCN demo on
  modest images, not for real-time 1080p->4K.

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
