# model.vy - ESPCN model loading and exact CPU inference.
#
# The weight bundle is the .vwy/.vwt format emitted by
# scripts/export_espcn_weights.py. We keep this CPU path as the correctness
# reference for the Float32 device implementation in gpu.vy.

use external "vconv/vconv.vy";

ruleset { dynamic_casting };

interface ESPCN {
    scale :: Int64,
    in_c  :: Int64,
    c1    :: Int64,
    c2    :: Int64,
    c3    :: Int64,
    k1    :: Int64,
    k2    :: Int64,
    k3    :: Int64,
    act   :: Int64,
    w1    :: Array<Float64>,
    b1    :: Array<Float64>,
    w2    :: Array<Float64>,
    b2    :: Array<Float64>,
    w3    :: Array<Float64>,
    b3    :: Array<Float64>,
}

fn load_espcn(path :: String) -> ESPCN {
    raw :: Map := vconv.load_model(path);
    meta :: Array<Float64> := raw["meta"];

    scale :: Int64 := 2;
    in_c :: Int64 := 3;
    c1 :: Int64 := 64;
    c2 :: Int64 := 32;
    c3 :: Int64 := 12;
    k1 :: Int64 := 5;
    k2 :: Int64 := 3;
    k3 :: Int64 := 3;
    act :: Int64 := 1;

    if meta.size() >= 9 {
        scale = int64(meta[0]);
        in_c = int64(meta[1]);
        c1 = int64(meta[2]);
        c2 = int64(meta[3]);
        c3 = int64(meta[4]);
        k1 = int64(meta[5]);
        k2 = int64(meta[6]);
        k3 = int64(meta[7]);
        act = int64(meta[8]);
    }

    return ESPCN(
        scale, in_c, c1, c2, c3, k1, k2, k3, act,
        vconv.get_tensor(raw, "conv1.weight"),
        vconv.get_tensor(raw, "conv1.bias"),
        vconv.get_tensor(raw, "conv2.weight"),
        vconv.get_tensor(raw, "conv2.bias"),
        vconv.get_tensor(raw, "conv3.weight"),
        vconv.get_tensor(raw, "conv3.bias")
    );
}

fn activation_name(act :: Int64) -> String {
    if act == 0 { return "relu"; }
    if act == 2 { return "leaky_relu"; }
    return "tanh";
}

fn activate_cpu(x :: vconv.Tensor, act :: Int64) -> vconv.Tensor {
    if act == 0 { return vconv.relu(x); }
    if act == 2 { return vconv.leaky_relu(x, 0.1); }
    return vconv.tanh(x);
}

fn forward_cpu(model :: ESPCN, x :: vconv.Tensor) -> vconv.Tensor {
    if x.channels != model.in_c {
        throw "espcn.forward_cpu: model expects " + string(model.in_c) +
            " channels, input has " + string(x.channels);
    }

    h1 :: vconv.Tensor := vconv.conv2d(
        x, model.w1, model.b1, model.c1, model.k1, 1, model.k1 / 2
    );
    a1 :: vconv.Tensor := activate_cpu(h1, model.act);
    h2 :: vconv.Tensor := vconv.conv2d(
        a1, model.w2, model.b2, model.c2, model.k2, 1, model.k2 / 2
    );
    a2 :: vconv.Tensor := activate_cpu(h2, model.act);
    h3 :: vconv.Tensor := vconv.conv2d(
        a2, model.w3, model.b3, model.c3, model.k3, 1, model.k3 / 2
    );
    return vconv.pixel_shuffle(h3, model.scale);
}

fn gpu_supported(model :: ESPCN) -> Bool {
    # The device runtime currently has ReLU but not tanh/leaky-ReLU intrinsics.
    return model.act == 0;
}
