# vconv/Ops.vy - forward convolutional primitives.
#
# This is the inference half of the SR stack. The native `vconv` module
# owns the numerical loops; this file validates shapes and keeps the
# Tensor wrapper honest.
#
# Weight layout is PyTorch OIHW:
#   index = (((oc * in_c) + ic) * kernel + ky) * kernel + kx
# so a `torch.nn.Conv2d` state dict exports without a transpose.

use "Types.vy";
use native vconv;

ruleset { dynamic_casting };

module vconv;

# Construct a tensor from a flat HWC buffer. No copy.
fn :: vconv tensor(
    data :: Array<Float64>,
    height :: Int64,
    width :: Int64,
    channels :: Int64
) -> vconv.Tensor {
    if height <= 0 || width <= 0 || channels <= 0 {
        throw "vconv.tensor: dimensions must be positive";
    }
    if data.size() != height * width * channels {
        throw "vconv.tensor: data length does not match dimensions";
    }
    return vconv.Tensor(data, height, width, channels);
}

# Zero-filled tensor.
fn :: vconv zeros(
    height :: Int64,
    width :: Int64,
    channels :: Int64
) -> vconv.Tensor {
    if height <= 0 || width <= 0 || channels <= 0 {
        throw "vconv.zeros: dimensions must be positive";
    }
    n :: Int64 := height * width * channels;
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return vconv.Tensor(data, height, width, channels);
}

# Square-kernel 2-D convolution over an HWC tensor.
#
#   out_h = (in_h + 2*pad - kernel) / stride + 1
#   out_w = (in_w + 2*pad - kernel) / stride + 1
#
# `weight` is OIHW and `bias` may be empty (no bias). Out-of-range
# kernel taps are treated as zero padding.
fn :: vconv conv2d(
    input :: vconv.Tensor,
    weight :: Array<Float64>,
    bias :: Array<Float64>,
    out_channels :: Int64,
    kernel :: Int64,
    stride :: Int64,
    pad :: Int64
) -> vconv.Tensor {
    if out_channels <= 0 || kernel <= 0 {
        throw "vconv.conv2d: out_channels and kernel must be positive";
    }
    if stride <= 0 {
        throw "vconv.conv2d: stride must be positive";
    }
    if pad < 0 {
        throw "vconv.conv2d: pad must be non-negative";
    }

    out_h :: Int64 := (input.height + 2 * pad - kernel) / stride + 1;
    out_w :: Int64 := (input.width + 2 * pad - kernel) / stride + 1;
    if out_h <= 0 || out_w <= 0 {
        throw "vconv.conv2d: kernel larger than padded input";
    }

    need :: Int64 := out_channels * input.channels * kernel * kernel;
    if weight.size() < need {
        throw "vconv.conv2d: weight has fewer than " + string(need) + " values";
    }

    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.conv2d_fwd(
        data,
        input.height,
        input.width,
        input.channels,
        weight,
        out_channels,
        kernel,
        kernel,
        stride,
        pad,
        bias
    );
    return vconv.Tensor(dst, out_h, out_w, out_channels);
}

# Gradient of conv2d w.r.t. the input, for a square kernel.
# `weight` is boxed so this wrapper is not itself a native-variant.
fn :: vconv conv_backward_input(
    grad :: vconv.Tensor,
    weight :: Array,
    in_h :: Int64,
    in_w :: Int64,
    in_c :: Int64,
    kernel :: Int64,
    stride :: Int64,
    pad :: Int64
) -> Array<Float64> {
    return vconv.conv2d_backward_input(
        grad.data,
        weight,
        grad.height,
        grad.width,
        grad.channels,
        in_h,
        in_w,
        in_c,
        kernel,
        kernel,
        stride,
        pad
    );
}

# Gradient of conv2d w.r.t. the OIHW weight.
fn :: vconv conv_backward_weight(
    input :: vconv.Tensor,
    grad :: vconv.Tensor,
    kernel :: Int64,
    stride :: Int64,
    pad :: Int64
) -> Array<Float64> {
    return vconv.conv2d_backward_weight(
        input.data,
        grad.data,
        input.height,
        input.width,
        input.channels,
        grad.height,
        grad.width,
        grad.channels,
        kernel,
        kernel,
        stride,
        pad
    );
}

# Gradient of conv2d w.r.t. the bias.
fn :: vconv conv_backward_bias(grad :: vconv.Tensor) -> Array<Float64> {
    return vconv.conv2d_backward_bias(
        grad.data,
        grad.height,
        grad.width,
        grad.channels
    );
}


fn :: vconv relu(input :: vconv.Tensor) -> vconv.Tensor {
    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.relu_fwd(data);
    return vconv.Tensor(dst, input.height, input.width, input.channels);
}

fn :: vconv leaky_relu(input :: vconv.Tensor, alpha :: Float64) -> vconv.Tensor {
    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.leaky_relu_fwd(data, alpha);
    return vconv.Tensor(dst, input.height, input.width, input.channels);
}

fn :: vconv tanh(input :: vconv.Tensor) -> vconv.Tensor {
    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.tanh_fwd(data);
    return vconv.Tensor(dst, input.height, input.width, input.channels);
}

fn :: vconv sigmoid(input :: vconv.Tensor) -> vconv.Tensor {
    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.sigmoid_fwd(data);
    return vconv.Tensor(dst, input.height, input.width, input.channels);
}

# ESPCN depth-to-space. Input channels must be divisible by scale^2.
fn :: vconv pixel_shuffle(input :: vconv.Tensor, scale :: Int64) -> vconv.Tensor {
    if scale <= 0 {
        throw "vconv.pixel_shuffle: scale must be positive";
    }
    rr :: Int64 := scale * scale;
    if input.channels % rr != 0 {
        throw "vconv.pixel_shuffle: channels not divisible by scale^2";
    }

    out_h :: Int64 := input.height * scale;
    out_w :: Int64 := input.width * scale;
    out_c :: Int64 := input.channels / rr;

    data :: Array<Float64> := input.data;
    dst :: Array<Float64> := vconv.pixel_shuffle_fwd(
        data,
        input.height,
        input.width,
        input.channels,
        scale
    );
    return vconv.Tensor(dst, out_h, out_w, out_c);
}

# `vconv.load_model(path)` is the native loader. It accepts the binary
# .vwy bundle emitted by scripts/export_espcn_weights.py and the text
# .vwt fixture format, and returns a Map of name -> Array<Float64>.
# The map is empty on a read/decode failure.

# Fetch one tensor and fail loudly when it is absent or empty.
fn :: vconv get_tensor(model :: Map, name :: String) -> Array<Float64> {
    t :: Array<Float64> := model[name];
    if t.size() == 0 {
        throw "vconv.get_tensor: missing tensor '" + name + "'";
    }
    return t;
}
