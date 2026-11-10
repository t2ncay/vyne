# gpu.vy - full SRCNN inference on the @device backend.
#
# All three convolution layers, both ReLUs, the residual add, and the
# final download run inside one device frame. The device backend now has a
# conv2d kernel (Float32 default, Float64 opt-in) alongside matmul and the
# elementwise kernels. If CUDA is unavailable, the same device calls fall
# back to the CPU backend.

use external "vconv/vconv.vy";
use "model.vy";

fn forward_gpu(model :: SRCNN, x :: vconv.Tensor) -> vconv.Tensor {
    h :: Int64 := x.height;
    w :: Int64 := x.width;
    hw :: Int64 := h * w;

    xf :: Array<Float32> := [];
    through cx :: 0..(hw*3)-1 -> loop { xf.push(float32(x.data[cx])); };

    w1f :: Array<Float32> := [];
    through cw1 :: 0..(8*3*5*5)-1 -> loop { w1f.push(float32(model.w1[cw1])); };
    b1f :: Array<Float32> := [];
    through cb1 :: 0..7 -> loop { b1f.push(float32(model.b1[cb1])); };

    w2f :: Array<Float32> := [];
    through cw2 :: 0..(4*8)-1 -> loop { w2f.push(float32(model.w2[cw2])); };
    b2f :: Array<Float32> := [];
    through cb2 :: 0..3 -> loop { b2f.push(float32(model.b2[cb2])); };

    w3f :: Array<Float32> := [];
    through cw3 :: 0..(3*4*5*5)-1 -> loop { w3f.push(float32(model.w3[cw3])); };
    b3f :: Array<Float32> := [];
    through cb3 :: 0..2 -> loop { b3f.push(float32(model.b3[cb3])); };

    outf :: Array<Float32> := [];
    through i :: 0..(hw*3)-1 -> loop { outf.push(0.0f); };

    @device<cuda> region infer {
        d_x :: Int64 := device_alloc(hw * 3);
        d_w1 :: Int64 := device_alloc(8 * 3 * 5 * 5);
        d_b1 :: Int64 := device_alloc(8);
        d_h1 :: Int64 := device_alloc(hw * 8);
        d_w2 :: Int64 := device_alloc(4 * 8);
        d_b2 :: Int64 := device_alloc(4);
        d_h2 :: Int64 := device_alloc(hw * 4);
        d_w3 :: Int64 := device_alloc(3 * 4 * 5 * 5);
        d_b3 :: Int64 := device_alloc(3);
        d_out :: Int64 := device_alloc(hw * 3);

        device_upload(d_x, xf, hw * 3);
        device_upload(d_w1, w1f, 8 * 3 * 5 * 5);
        device_upload(d_b1, b1f, 8);
        device_upload(d_w2, w2f, 4 * 8);
        device_upload(d_b2, b2f, 4);
        device_upload(d_w3, w3f, 3 * 4 * 5 * 5);
        device_upload(d_b3, b3f, 3);

        # Layer 1: 3 -> 8, 5x5, pad 2.
        device_conv2d(d_h1, d_x, d_w1, d_b1, h, w, 3, h, w, 8, 5, 1, 2, 1);
        device_relu(d_h1, d_h1, hw * 8);

        # Layer 2: 8 -> 4, 1x1.
        device_conv2d(d_h2, d_h1, d_w2, d_b2, h, w, 8, h, w, 4, 1, 1, 0, 1);
        device_relu(d_h2, d_h2, hw * 4);

        # Layer 3: 4 -> 3, 5x5, pad 2. Residual add to the input.
        device_conv2d(d_out, d_h2, d_w3, d_b3, h, w, 4, h, w, 3, 5, 1, 2, 1);
        device_add(d_out, d_out, d_x, hw * 3);
        device_download(d_out, outf, hw * 3);
        device_sync();
    };

    data :: Array<Float64> := [];
    through co :: 0..(hw*3)-1 -> loop { data.push(float64(outf[co])); };
    return vconv.Tensor(data, h, w, 3);
}
