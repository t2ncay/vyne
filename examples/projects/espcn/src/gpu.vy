# gpu.vy - full ESPCN inference in one @device frame.
#
# The hot path keeps all intermediate tensors on the selected device:
# conv1 -> ReLU -> conv2 -> ReLU -> conv3 -> pixel shuffle -> one download.
# CUDA is requested, with the runtime's CPU device fallback when unavailable.

use external "vconv/vconv.vy";
use "model.vy";

fn forward_gpu(model :: ESPCN, x :: vconv.Tensor) -> vconv.Tensor {
    if !gpu_supported(model) {
        throw "espcn.forward_gpu: only ReLU activation is supported on device";
    }
    if x.channels != model.in_c {
        throw "espcn.forward_gpu: channel mismatch";
    }

    h :: Int64 := x.height;
    w :: Int64 := x.width;
    in_n :: Int64 := h * w * model.in_c;
    h1_n :: Int64 := h * w * model.c1;
    h2_n :: Int64 := h * w * model.c2;
    h3_n :: Int64 := h * w * model.c3;
    out_c :: Int64 := model.c3 / (model.scale * model.scale);
    out_n :: Int64 := (h * model.scale) * (w * model.scale) * out_c;

    xf :: Array<Float32> := [];
    through cx :: 0..in_n-1 -> loop { xf.push(float32(x.data[cx])); };

    w1f :: Array<Float32> := [];
    through cw1 :: 0..model.w1.size()-1 -> loop { w1f.push(float32(model.w1[cw1])); };
    b1f :: Array<Float32> := [];
    through cb1 :: 0..model.c1-1 -> loop { b1f.push(float32(model.b1[cb1])); };

    w2f :: Array<Float32> := [];
    through cw2 :: 0..model.w2.size()-1 -> loop { w2f.push(float32(model.w2[cw2])); };
    b2f :: Array<Float32> := [];
    through cb2 :: 0..model.c2-1 -> loop { b2f.push(float32(model.b2[cb2])); };

    w3f :: Array<Float32> := [];
    through cw3 :: 0..model.w3.size()-1 -> loop { w3f.push(float32(model.w3[cw3])); };
    b3f :: Array<Float32> := [];
    through cb3 :: 0..model.c3-1 -> loop { b3f.push(float32(model.b3[cb3])); };

    outf :: Array<Float32> := [];
    through oi :: 0..out_n-1 -> loop { outf.push(0.0f); };

    @device<cuda> region infer {
        d_x :: Int64 := device_alloc(in_n);
        d_w1 :: Int64 := device_alloc(w1f.size());
        d_b1 :: Int64 := device_alloc(model.c1);
        d_w2 :: Int64 := device_alloc(w2f.size());
        d_b2 :: Int64 := device_alloc(model.c2);
        d_w3 :: Int64 := device_alloc(w3f.size());
        d_b3 :: Int64 := device_alloc(model.c3);

        device_upload(d_x, xf, in_n);
        device_upload(d_w1, w1f, w1f.size());
        device_upload(d_b1, b1f, model.c1);
        device_upload(d_w2, w2f, w2f.size());
        device_upload(d_b2, b2f, model.c2);
        device_upload(d_w3, w3f, w3f.size());
        device_upload(d_b3, b3f, model.c3);

        d_h1 :: Int64 := vconv_conv2d_fwd_device(
            d_x, d_w1, d_b1,
            h, w, model.in_c,
            model.c1, model.k1, model.k1, 1, model.k1 / 2
        );
        device_relu(d_h1, d_h1, h1_n);

        d_h2 :: Int64 := vconv_conv2d_fwd_device(
            d_h1, d_w2, d_b2,
            h, w, model.c1,
            model.c2, model.k2, model.k2, 1, model.k2 / 2
        );
        device_relu(d_h2, d_h2, h2_n);

        d_h3 :: Int64 := vconv_conv2d_fwd_device(
            d_h2, d_w3, d_b3,
            h, w, model.c2,
            model.c3, model.k3, model.k3, 1, model.k3 / 2
        );
        d_out :: Int64 := vconv_pixel_shuffle_fwd_device(
            d_h3, h, w, model.c3, model.scale
        );

        device_download(d_out, outf, out_n);
        device_sync();

        out("device    : " + device_backend());
        out("dev peak  : " + string(device_peak_bytes()) + " bytes");
    };

    data :: Array<Float64> := [];
    through co :: 0..out_n-1 -> loop { data.push(float64(outf[co])); };
    return vconv.Tensor(data, h * model.scale, w * model.scale, out_c);
}
