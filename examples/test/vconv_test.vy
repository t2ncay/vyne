# vconv_test.vy - behavioural checks for the vconv library.
#
# Run compiled:
#   ./vynec --compile examples/test/vconv_test.vy
#   ./examples/test/vconv_test

use external "vconv/vconv.vy";
use native vcore;
use native vfs;

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vconv ===");

# ---- 1x1 single-channel convolution -------------------------------
x1 :: Array<Float64> := [1.0];
t1 :: vconv.Tensor := vconv.tensor(x1, 1, 1, 1);
w1 :: Array<Float64> := [2.0];
b1 :: Array<Float64> := [0.5];
y1 :: vconv.Tensor := vconv.conv2d(t1, w1, b1, 1, 1, 1, 0);
check("1x1-size", y1.data.size() == 1);
check("1x1-value", close(y1.data[0], 2.5));

# ---- 3x3 sum with a 3x3 all-ones kernel ---------------------------
x2 :: Array<Float64> := [];
through i :: 1..9 -> loop { x2.push(float64(i)); };
t2 :: vconv.Tensor := vconv.tensor(x2, 3, 3, 1);
w2 :: Array<Float64> := [];
through i :: 0..8 -> loop { w2.push(1.0); };
empty :: Array<Float64> := [];
y2 :: vconv.Tensor := vconv.conv2d(t2, w2, empty, 1, 3, 1, 0);
check("sum-shape", y2.height == 1 && y2.width == 1 && y2.channels == 1);
check("sum-value", close(y2.data[0], 45.0));

# ---- Same kernel with pad=1 ---------------------------------------
y3 :: vconv.Tensor := vconv.conv2d(t2, w2, empty, 1, 3, 1, 1);
check("pad-shape", y3.height == 3 && y3.width == 3);
check("pad-corner", close(y3.data[0], 12.0));
check("pad-center", close(y3.data[4], 45.0));
check("pad-corner2", close(y3.data[8], 28.0));

# ---- stride=2, pad=1 ----------------------------------------------
y4 :: vconv.Tensor := vconv.conv2d(t2, w2, empty, 1, 3, 2, 1);
check("stride-shape", y4.height == 2 && y4.width == 2);
check("stride-00", close(y4.data[0], 12.0));
check("stride-01", close(y4.data[1], 16.0));

# ---- Multi-channel 1x1 --------------------------------------------
x5 :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0];
t5 :: vconv.Tensor := vconv.tensor(x5, 2, 2, 2);
w5 :: Array<Float64> := [1.0, 1.0];
y5 :: vconv.Tensor := vconv.conv2d(t5, w5, empty, 1, 1, 1, 0);
check("multi-shape", y5.height == 2 && y5.width == 2 && y5.channels == 1);
check("multi-0", close(y5.data[0], 3.0));
check("multi-1", close(y5.data[1], 7.0));
check("multi-2", close(y5.data[2], 11.0));
check("multi-3", close(y5.data[3], 15.0));

# ---- Pixel shuffle -------------------------------------------------
ps_in :: Array<Float64> := [0.1, 0.2, 0.3, 0.4];
pt :: vconv.Tensor := vconv.tensor(ps_in, 1, 1, 4);
ps :: vconv.Tensor := vconv.pixel_shuffle(pt, 2);
check("shuffle-shape", ps.height == 2 && ps.width == 2 && ps.channels == 1);
check("shuffle-00", close(ps.data[0], 0.1));
check("shuffle-01", close(ps.data[1], 0.2));
check("shuffle-10", close(ps.data[2], 0.3));
check("shuffle-11", close(ps.data[3], 0.4));

# ---- Activations ---------------------------------------------------
a_in :: Array<Float64> := [-1.0, 0.0, 2.0];
at :: vconv.Tensor := vconv.tensor(a_in, 1, 3, 1);
r :: vconv.Tensor := vconv.relu(at);
check("relu-0", close(r.data[0], 0.0));
check("relu-2", close(r.data[2], 2.0));
lr :: vconv.Tensor := vconv.leaky_relu(at, 0.1);
check("leaky-neg", close(lr.data[0], -0.1));
th :: vconv.Tensor := vconv.tanh(at);
check("tanh-zero", close(th.data[1], 0.0));
sg :: vconv.Tensor := vconv.sigmoid(at);
check("sigmoid-zero", close(sg.data[1], 0.5));

# ---- Text weight bundle -------------------------------------------
model_path :: String := "vconv_test_tmp.vwt";
sb :: Int64 := vcore.sb_create();
vcore.sb_append(sb, "VYWT1\n2\n");
vcore.sb_append(sb, "w1 1 4\n1.0 2.0 3.0 4.0\n");
vcore.sb_append(sb, "b1 1 1\n0.5\n");
vfs.write(model_path, vcore.sb_build(sb));

model :: Map := vconv.load_model(model_path);
w_loaded :: Array<Float64> := vconv.get_tensor(model, "w1");
b_loaded :: Array<Float64> := vconv.get_tensor(model, "b1");
check("model-w-size", w_loaded.size() == 4);
check("model-w-value", close(w_loaded[2], 3.0));
check("model-b-value", close(b_loaded[0], 0.5));
vfs.remove(model_path);
check("model-cleanup", !vfs.exists(model_path));

# ---- Shape validation ---------------------------------------------
bad :: Bool := false;
try {
    vconv.conv2d(t1, w1, b1, 1, 3, 1, 0);
} catch {
    bad = true;
}
check("conv2d-too-small-throws", bad);

bad2 :: Bool := false;
try {
    vconv.tensor(x1, 2, 2, 1);
} catch {
    bad2 = true;
}
check("tensor-shape-throws", bad2);

if failures[0] > 0 {
    out("vconv: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vconv: all checks passed");
