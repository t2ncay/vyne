ruleset {
    dynamic_casting,
};

use lib "vcolors.vy";
use lib "vlinalg/vlinalg.vy";
module vmath;

# ======================================================================
# CONFIG
# ======================================================================
N_POINTS    = 300;
EPOCHS      = 3000;
LR          = 0.3;
HIDDEN      = 10;
PRINT_EVERY = 100;
COLS        = 70;
ROWS        = 26;
SPAN        = 1.1;

# ======================================================================
# HELPERS
# ======================================================================

# vmath.random is integer-only (see runtime/modules/vmath.h). Convert
# its output to a uniform float in [lo, hi).
fn rand_uniform(lo :: Float64, hi :: Float64) -> Float64 {
    r = float64(vmath.random(0, 1000000)) / 1000000.0;
    return lo + r * (hi - lo);
}

fn forward(X_in, W1, b1, W2, b2, W3, b3) -> Array {
    A1 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(X_in, W1), b1));
    A2 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(A1,   W2), b2));
    A3 = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(A2, W3), b3));
    return [A1, A2, A3];
}

fn accuracy(Y :: vlinalg.Types.Matrix,
            A3 :: vlinalg.Types.Matrix,
            n :: Int64) -> Float64 {
    correct = 0;
    through i :: 0..n-1 -> loop {
        p  = A3.data[i][0];
        a  = Y.data[i][0];
        pi = 0;
        ai = 0;
        if p > 0.5 { pi = 1; }
        if a > 0.5 { ai = 1; }
        if pi == ai { correct = correct + 1; }
    };
    return float64(correct) / float64(n);
}

fn bar(pct :: Float64, width :: Int64) -> String {
    filled = int64(pct * float64(width));
    if filled < 0     { filled = 0; }
    if filled > width { filled = width; }
    s = "[";
    through i :: 0..width-1 -> loop {
        if i < filled { s = s + "#"; }
        else          { s = s + "."; }
    };
    return s + "]";
}

fn pad_left(s :: String, width :: Int64) -> String {
    n      = int64(s.size());
    result = "";
    through i :: 0..width-n-1 -> loop { result = result + " "; };
    return result + s;
}
# ======================================================================
# HEADER
# ======================================================================
out("");
out(vcolors.bold("=== Circle Classifier ==="));
out("");
out("  architecture   2 -> " + string(HIDDEN) + " -> " + string(HIDDEN) + " -> 1");
out("  samples        " + string(N_POINTS));
out("  learning rate  " + string(LR));
out("  epochs         " + string(EPOCHS));
out("");

# ======================================================================
# DATA — uniform points in [-1, 1]^2, label 1 if inside radius 0.5
# ======================================================================
out("Generating data...");

X_data :: Array = [];
Y_data :: Array = [];
n_pos = 0;
n_neg = 0;

through i :: 0..N_POINTS-1 -> loop {
    x = rand_uniform(-1.0, 1.0);
    y = rand_uniform(-1.0, 1.0);
    X_data.push([x, y]);
    if x*x + y*y < 0.25 {
        Y_data.push([1.0]);
        n_pos = n_pos + 1;
    } else {
        Y_data.push([0.0]);
        n_neg = n_neg + 1;
    }
};

out("  class 1 (inside)  : " + string(n_pos));
out("  class 0 (outside) : " + string(n_neg));
out("");

X = vlinalg.Types.Matrix(N_POINTS, 2, X_data);
Y = vlinalg.Types.Matrix(N_POINTS, 1, Y_data);

# ======================================================================
# WEIGHTS — 2 -> 10 -> 10 -> 1, tanh hidden, sigmoid output
# ======================================================================
W1 = vlinalg.xavier_init(2, HIDDEN);
b1 :: Array = [];
through j :: 0..HIDDEN-1 -> loop { b1.push(0.0); };

W2 = vlinalg.xavier_init(HIDDEN, HIDDEN);
b2 :: Array = [];
through j :: 0..HIDDEN-1 -> loop { b2.push(0.0); };

W3 = vlinalg.xavier_init(HIDDEN, 1);
b3 :: Array = [0.0];

# ======================================================================
# TRAINING
# ======================================================================
scale = LR / float64(N_POINTS);

h0    = forward(X, W1, b1, W2, b2, W3, b3);
loss0 = vlinalg.cross_entropy(h0[2], Y);

out(vcolors.bold("Training:"));
out("  initial loss  " + string(loss0));
out("");

lossN = loss0;
A3    = h0[2];

through epoch :: 1..EPOCHS -> loop {
    h  = forward(X, W1, b1, W2, b2, W3, b3);
    A1 = h[0];
    A2 = h[1];
    A3 = h[2];

    # ---- backprop ----
    delta3 = vlinalg.subtract(A3, Y);
    dW3    = vlinalg.multiply(vlinalg.transpose(A2), delta3);

    db3 = 0.0;
    through r :: 0..N_POINTS-1 -> loop { db3 = db3 + delta3.data[r][0]; };

    delta2 = vlinalg.hadamard(
        vlinalg.multiply(delta3, vlinalg.transpose(W3)),
        vlinalg.tanh_prime(A2)
    );
    dW2 = vlinalg.multiply(vlinalg.transpose(A1), delta2);

    db2 :: Array = [];
    through c :: 0..HIDDEN-1 -> loop {
        s = 0.0;
        through r :: 0..N_POINTS-1 -> loop { s = s + delta2.data[r][c]; };
        db2.push(s);
    };

    delta1 = vlinalg.hadamard(
        vlinalg.multiply(delta2, vlinalg.transpose(W2)),
        vlinalg.tanh_prime(A1)
    );
    dW1 = vlinalg.multiply(vlinalg.transpose(X), delta1);

    db1 :: Array = [];
    through c :: 0..HIDDEN-1 -> loop {
        s = 0.0;
        through r :: 0..N_POINTS-1 -> loop { s = s + delta1.data[r][c]; };
        db1.push(s);
    };

    # ---- update ----
    W1 = vlinalg.subtract(W1, vlinalg.multiply_scalar(dW1, scale));
    W2 = vlinalg.subtract(W2, vlinalg.multiply_scalar(dW2, scale));
    W3 = vlinalg.subtract(W3, vlinalg.multiply_scalar(dW3, scale));

    through c :: 0..HIDDEN-1 -> loop {
        b1[c] = b1[c] - scale * db1[c];
        b2[c] = b2[c] - scale * db2[c];
    };
    b3[0] = b3[0] - scale * db3;

    # ---- progress ----
    if epoch % PRINT_EVERY == 0 {
        lossN = vlinalg.cross_entropy(A3, Y);
        acc   = accuracy(Y, A3, N_POINTS);
        out("  " + pad_left(string(epoch), 5) + "/" + string(EPOCHS)
            + "  loss " + string(lossN)
            + "  acc  " + string(acc)
            + "  " + bar(acc, 18));
    }
};

out("");
out(vcolors.bold("Training complete."));
out("  initial loss   " + string(loss0));
out("  final loss     " + string(lossN));
out("  final accuracy " + string(accuracy(Y, A3, N_POINTS)));
out("");

# ======================================================================
# RENDER — decision boundary with training data overlaid
# ======================================================================
out(vcolors.bold("Decision boundary:"));
out("  legend:  x = class 1 (inside)    o = class 0 (outside)    .:@ = model");
out("");

grid :: Array = [];
through r :: 0..ROWS-1 -> loop {
    through c :: 0..COLS-1 -> loop {
        gx = -SPAN + (float64(c) / float64(COLS - 1)) * 2.0 * SPAN;
        gy =  SPAN - (float64(r) / float64(ROWS - 1)) * 2.0 * SPAN;
        grid.push([gx, gy]);
    };
};
G = vlinalg.Types.Matrix(ROWS * COLS, 2, grid);

hG  = forward(G, W1, b1, W2, b2, W3, b3);
A3G = hG[2];

# Overlay map:  0 = no sample, 1 = class-0 sample, 2 = class-1 sample
overlay :: Array = [];
through i :: 0..ROWS*COLS-1 -> loop { overlay.push(0); };

through i :: 0..N_POINTS-1 -> loop {
    x = X_data[i][0];
    y = X_data[i][1];
    c = int64((x + SPAN)     / (2.0 * SPAN) * float64(COLS - 1));
    r = int64((SPAN - y)     / (2.0 * SPAN) * float64(ROWS - 1));
    if c >= 0 && c < COLS && r >= 0 && r < ROWS {
        idx = r * COLS + c;
        if Y_data[i][0] > 0.5 { overlay[idx] = 2; }
        else                  { overlay[idx] = 1; }
    }
};

ramp :: Array = [" ", ".", ":", "-", "=", "+", "*", "#", "%", "@"];

through r :: 0..ROWS-1 -> loop {
    line = "";
    through c :: 0..COLS-1 -> loop {
        idx = r * COLS + c;
        ov  = overlay[idx];
        if ov == 1 {
            line = line + "o";
        } else if ov == 2 {
            line = line + "x";
        } else {
            p  = A3G.data[idx][0];
            pi = int64(p * 9.0);
            if pi < 0 { pi = 0; }
            if pi > 9 { pi = 9; }
            line = line + ramp[pi];
        }
    };
    out(line);
};

out("");
out(vcolors.success("Done."));