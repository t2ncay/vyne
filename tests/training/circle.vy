ruleset {
    dynamic_casting,
};

use lib "vcolors.vy";
use lib "vlinalg.vy";
module vmath;

# ======================================================================
# CONFIG
# ======================================================================
N_POINTS = 300;
EPOCHS   = 1500;
LR       = 0.5;
HIDDEN   = 10;

# ======================================================================
# DATA  — points inside a circle of radius 0.5 = class 1, else class 0
# ======================================================================
X_data :: Array = [];
Y_data :: Array = [];

through i :: 0..N_POINTS-1 -> loop {
    x = vmath.random(-1.0, 1.0);
    y = vmath.random(-1.0, 1.0);
    X_data.push([x, y]);
    if x*x + y*y < 0.25 {
        Y_data.push([1.0]);
    } else {
        Y_data.push([0.0]);
    }
};

X = vlinalg.Types.Matrix(N_POINTS, 2, X_data);
Y = vlinalg.Types.Matrix(N_POINTS, 1, Y_data);

# ======================================================================
# WEIGHTS  — 2 -> 10 -> 10 -> 1, tanh hidden, sigmoid output
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
# RENDER HELPER  — draws the decision boundary in ASCII
# ======================================================================
fn render_boundary(W1 :: vlinalg.Types.Matrix, b1 :: Array,
                   W2 :: vlinalg.Types.Matrix, b2 :: Array,
                   W3 :: vlinalg.Types.Matrix, b3 :: Array) {
    COLS = 70;
    ROWS = 26;
    SPAN = 1.1;

    grid :: Array = [];
    through r :: 0..ROWS-1 -> loop {
        through c :: 0..COLS-1 -> loop {
            gx = -SPAN + (float64(c) / float64(COLS - 1)) * 2.0 * SPAN;
            gy =  SPAN - (float64(r) / float64(ROWS - 1)) * 2.0 * SPAN;
            grid.push([gx, gy]);
        };
    };
    G = vlinalg.Types.Matrix(ROWS * COLS, 2, grid);

    A1 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(G, W1), b1));
    A2 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(A1, W2), b2));
    A3 = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(A2, W3), b3));

    ramp :: Array = [" ", ".", ":", "-", "=", "+", "*", "#", "%", "@"];

    through r :: 0..ROWS-1 -> loop {
        line = "";
        through c :: 0..COLS-1 -> loop {
            p = A3.data[r * COLS + c][0];
            idx = int64(p * 9.0);
            if idx < 0 { idx = 0; }
            if idx > 9 { idx = 9; }
            line = line + ramp[idx];
        };
        out(line);
    };
    out("");
}

# ======================================================================
# TRAINING
# ======================================================================
through epoch :: 1..EPOCHS -> loop {
    # ---- forward ----
    A1 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(X, W1), b1));
    A2 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(A1, W2), b2));
    A3 = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(A2, W3), b3));

    # ---- loss / backprop ----
    diff   = vlinalg.subtract(Y, A3);
    delta3 = vlinalg.hadamard(diff, vlinalg.sigmoid_prime(A3));
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
    scale = LR / float64(N_POINTS);
    W1 = vlinalg.subtract(W1, vlinalg.multiply_scalar(dW1, scale));
    W2 = vlinalg.subtract(W2, vlinalg.multiply_scalar(dW2, scale));
    W3 = vlinalg.subtract(W3, vlinalg.multiply_scalar(dW3, scale));

    through c :: 0..HIDDEN-1 -> loop {
        b1[c] = b1[c] + scale * db1[c];
        b2[c] = b2[c] + scale * db2[c];
    };
    b3[0] = b3[0] + scale * db3;

    # ---- snapshot ----
    if epoch % 1000 == 0 {
        loss = vlinalg.mse(Y, A3);
        out(vcolors.bold("Epoch " + string(epoch) + "  loss=" + string(loss)));
        render_boundary(W1, b1, W2, b2, W3, b3);
    }
};

out(vcolors.success("Training complete."));
render_boundary(W1, b1, W2, b2, W3, b3);