# gpu.vy - GPU-accelerated 1x1 mapping layer for the residual SRCNN.
#
# The device backend exposes matmul/add/relu but not convolution. The 5x5
# layers stay on the CPU (vconv), while the 1x1 layer is exactly a per-pixel
# matmul [H*W,8] * [8,4], so it runs through `@device<cuda>`. If CUDA is
# unavailable the device region falls back to the CPU backend.
#
# This hybrid keeps the inference path GPU-accelerated where the backend
# supports it and correctness-identical to the CPU model. A device conv
# kernel would move the 5x5 layers over too; that is the follow-up.

use external "vconv/vconv.vy";
use "model.vy";

# OIHW -> [in_c*k*k, out_c]
fn weight_matrix(w :: Array, out_c :: Int64, in_c :: Int64,
                 k :: Int64) -> Array<Float64> {
    m :: Array<Float64> := [];
    through i :: 0..(in_c*k*k*out_c)-1 -> loop { m.push(0.0); };
    through oc :: 0..out_c-1 -> loop {
        through ic :: 0..in_c-1 -> loop {
            through ky :: 0..k-1 -> loop {
                through kx :: 0..k-1 -> loop {
                    row :: Int64 := ((ic * k) + ky) * k + kx;
                    m[row * out_c + oc] = w[((oc * in_c + ic) * k + ky) * k + kx];
                };
            };
        };
    };
    return m;
}

fn bias_broadcast(b :: Array, hw :: Int64,
                  c :: Int64) -> Array<Float64> {
    outv :: Array<Float64> := [];
    through i :: 0..hw-1 -> loop {
        through ch :: 0..c-1 -> loop { outv.push(b[ch]); };
    };
    return outv;
}

fn forward_gpu(model :: SRCNN, x :: vconv.Tensor) -> vconv.Tensor {
    h :: Int64 := x.height;
    w :: Int64 := x.width;
    hw :: Int64 := h * w;

    # 5x5 feature extraction on the CPU.
    h1 :: vconv.Tensor := vconv.conv2d(x, model.w1, model.b1, 8, 5, 1, 2);
    a1 :: vconv.Tensor := vconv.relu(h1);

    # 1x1 mapping layer on the device: [hw,8] * [8,4].
    w2m :: Array<Float64> := weight_matrix(model.w2, 4, 8, 1);
    b2bc :: Array<Float64> := bias_broadcast(model.b2, hw, 4);

    a1f :: Array<Float32> := [];
    through i :: 0..(hw*8)-1 -> loop { a1f.push(float32(a1.data[i])); };
    w2mf :: Array<Float32> := [];
    through i2 :: 0..(8*4)-1 -> loop { w2mf.push(float32(w2m[i2])); };
    b2bcf :: Array<Float32> := [];
    through i3 :: 0..(hw*4)-1 -> loop { b2bcf.push(float32(b2bc[i3])); };
    a2f :: Array<Float32> := [];
    through i4 :: 0..(hw*4)-1 -> loop { a2f.push(0.0f); };

    @device<cuda> region layer2 {
        d_x :: Int64 := device_alloc(hw * 8);
        d_w :: Int64 := device_alloc(8 * 4);
        d_b :: Int64 := device_alloc(hw * 4);
        d_h :: Int64 := device_alloc(hw * 4);
        device_upload(d_x, a1f, hw * 8);
        device_upload(d_w, w2mf, 8 * 4);
        device_upload(d_b, b2bcf, hw * 4);
        device_matmul(d_h, d_x, d_w, hw, 4, 8);
        device_add(d_h, d_h, d_b, hw * 4);
        device_relu(d_h, d_h, hw * 4);
        device_download(d_h, a2f, hw * 4);
        device_sync();
    };

    a2 :: Array<Float64> := [];
    through i5 :: 0..(hw*4)-1 -> loop { a2.push(float64(a2f[i5])); };
    a2t :: vconv.Tensor := vconv.Tensor(a2, h, w, 4);

    # 5x5 reconstruction on the CPU, then the residual add.
    res :: vconv.Tensor := vconv.conv2d(a2t, model.w3, model.b3, 3, 5, 1, 2);
    return tensor_add(x, res);
}
