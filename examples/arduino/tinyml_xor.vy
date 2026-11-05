# ═════════════════════════════════════════════════════════════════════
# Vyne TinyML — XOR learner on Arduino Uno R3
# ═════════════════════════════════════════════════════════════════════
#
# A 2 → 3 → 1 fully-connected network learns XOR from scratch, live on
# the board. Every weight is a Float64 scalar. All state fits in the C
# stack. One epoch = one sample.
#
#   x0 ──→ h0 ──┐
#          h1 ──┼──→ y_pred
#   x1 ──→ h2 ──┘
#
# Activation: ReLU hidden, linear output. This avoids every transcendental
# (exp / log / tanh) — only arithmetic and comparisons — so the demo runs
# on the vanilla MCU runtime without any of the extended math bindings.
#
# Loss: MSE on {0, 1} targets. Gradient w.r.t. output pre-activation is
# just (y_pred - y).

use native arduino;

# ═════════════════════════════════════════════════════════════════════
# §1 BOOT
# ═════════════════════════════════════════════════════════════════════

arduino.serial_begin(115200);
arduino.delay(300);

# Boot indicator: three quick blinks.
through i :: 0..2 -> loop {
    arduino.digital_write(13, 1);
    arduino.delay(100);
    arduino.digital_write(13, 0);
    arduino.delay(100);
};

arduino.serial_print("\n");
arduino.serial_print("=== Vyne TinyML ===\n");
arduino.serial_print("XOR learner on Uno R3\n");
arduino.serial_print("\n");

arduino.serial_print("arena_used at boot: ");
arduino.serial_print(arduino.arena_used());
arduino.serial_print(" B\n\n");

# ═════════════════════════════════════════════════════════════════════
# §2 FUNCTION SELF-TEST
# ═════════════════════════════════════════════════════════════════════
#
# Only the three functions that ship with the vanilla MCU runtime, plus
# the extensions from my earlier patch. Delete the section marked
# "extended" if you have not installed mcu_math.h.

arduino.serial_print("Basic math:\n");

arduino.serial_print("  sin(pi/2) = ");
arduino.serial_print(arduino.sin(1.5707963));
arduino.serial_print("\n");

arduino.serial_print("  cos(0)    = ");
arduino.serial_print(arduino.cos(0.0));
arduino.serial_print("\n");

arduino.serial_print("  sqrt(16)  = ");
arduino.serial_print(arduino.sqrt(16.0));
arduino.serial_print("\n\n");

# --- extended (requires mcu_math.h from the earlier patch) ---
arduino.serial_print("Extended math:\n");

arduino.serial_print("  exp(0)    = ");
arduino.serial_print(arduino.exp(0.0));
arduino.serial_print("\n");

arduino.serial_print("  log(e)    = ");
arduino.serial_print(arduino.log(2.7182818));
arduino.serial_print("\n");

arduino.serial_print("  pow(2,10) = ");
arduino.serial_print(arduino.pow(2.0, 10.0));
arduino.serial_print("\n\n");

# ═════════════════════════════════════════════════════════════════════
# §3 MEMORY MODEL PROOF
# ═════════════════════════════════════════════════════════════════════
#
# A region block checkpoints the arena on entry and rewinds on exit.
# The `scratch` array is a raw C `double buf[16]` on the stack — no
# arena traffic. But the checkpoint/rewind pair still runs, and the
# drift check confirms nothing leaked. This is the exact proof from
# reactive.vy, kept compact.

arduino.serial_print("Memory model:\n");

before :: Int64 := arduino.arena_used();

region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop {
        buf[i] = arduino.sin(0.1 * i);
    };
    m :: Float64 := buf.mean();
    arduino.serial_print("  buf.mean() = ");
    arduino.serial_print(m);
    arduino.serial_print("\n");
};

after :: Int64 := arduino.arena_used();
drift :: Int64 := after - before;

arduino.serial_print("  drift      = ");
arduino.serial_print(drift);
arduino.serial_print(" B\n\n");

# ═════════════════════════════════════════════════════════════════════
# §4 PRNG
# ═════════════════════════════════════════════════════════════════════
#
# A small LCG implemented in Vyne rather than arduino.random(), so this
# demo depends on nothing beyond the vanilla core. arduino.random would
# work too, but the arithmetic path keeps the demo provably self-contained
# and reproducible across boots.

seed :: Int64 := 12345;

fn lcg_next() -> Int64 {
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return seed;
}

# Uniform Float64 in [-1, 1]. The (v + 0.0) is a native int64_t →
# double conversion: the emitter emits `(double)(v)` and keeps the
# whole expression unboxed.
fn rand_unit() -> Float64 {
    v :: Int64 := lcg_next() % 2001;
    vf :: Float64 := v + 0.0;
    return (vf - 1000.0) / 1000.0;
}

# ═════════════════════════════════════════════════════════════════════
# §5 NETWORK INIT
# ═════════════════════════════════════════════════════════════════════

arduino.serial_print("Initializing weights...\n");

# Weight layout:
#   w[input][hidden]  — 2 × 3
#   b[hidden]         — 3
#   v[hidden]         — 3
#   c0                — 1
#
# Deterministic small init so every boot produces the same trajectory.
# Change any literal to get a different run.

w00 :: Float64 :=  0.30;
w01 :: Float64 := -0.20;
w02 :: Float64 :=  0.15;
w10 :: Float64 :=  0.40;
w11 :: Float64 :=  0.10;
w12 :: Float64 := -0.25;
b0  :: Float64 :=  0.00;
b1  :: Float64 :=  0.00;
b2  :: Float64 :=  0.00;
v0  :: Float64 :=  0.20;
v1  :: Float64 := -0.30;
v2  :: Float64 :=  0.25;
c0  :: Float64 :=  0.00;

arduino.serial_print("  done\n\n");

# ═════════════════════════════════════════════════════════════════════
# §6 ONLINE TRAINING
# ═════════════════════════════════════════════════════════════════════
#
# XOR: label is 1 if the two inputs have different signs. Not linearly
# separable. Requires a hidden layer.
#
# Every iteration:
#   1. Draw a random (x0, x1) from the LCG.
#   2. Forward: z = x·W + b; h = ReLU(z); y = h·V + c.
#   3. Gradient: d_out = y − y_true; d_hidden = d_out·V·[z > 0].
#   4. SGD update: every weight -= lr * its gradient.
#
# The training body lives inside `region step { ... }` per iteration.
# Nothing allocates in the loop today — the region is a syntax
# demonstration and a placeholder for later code that will allocate.

lr :: Float64 := 0.05;
report_every :: Int64 := 1000;
total_iters :: Int64 := 30000;

running_loss :: Float64 := 0.0;
running_correct :: Float64 := 0.0;
running_n :: Float64 := 0.0;

arduino.serial_print("Training (SGD, lr=0.05):\n");
arduino.serial_print("  iter  |  loss  |  acc  |  arena\n");
arduino.serial_print("  ------+--------+-------+-------\n");

through iter :: 1..total_iters -> loop {
    # ── Sample ──
    x0 :: Float64 := rand_unit();
    x1 :: Float64 := rand_unit();

    y_true :: Int64 := 0;
    if x0 * x1 < 0.0 { y_true = 1; }
    y_f :: Float64 := 0.0;
    if y_true == 1 { y_f = 1.0; }

    region step {
        # ── Forward ──
        z0 :: Float64 := w00 * x0 + w10 * x1 + b0;
        h0 :: Float64 := z0;
        if h0 < 0.0 { h0 = 0.0; }

        z1 :: Float64 := w01 * x0 + w11 * x1 + b1;
        h1 :: Float64 := z1;
        if h1 < 0.0 { h1 = 0.0; }

        z2 :: Float64 := w02 * x0 + w12 * x1 + b2;
        h2 :: Float64 := z2;
        if h2 < 0.0 { h2 = 0.0; }

        y_pred :: Float64 := v0 * h0 + v1 * h1 + v2 * h2 + c0;

        # ── MSE loss ──
        diff :: Float64 := y_pred - y_f;
        loss :: Float64 := diff * diff;

        # ── Accuracy tracking ──
        pred_class :: Int64 := 0;
        if y_pred > 0.5 { pred_class = 1; }
        if pred_class == y_true { running_correct = running_correct + 1.0; }
        running_loss = running_loss + loss;
        running_n = running_n + 1.0;

        # ── Backward ──
        # Output layer gradients (linear activation → derivative is 1).
        dv0 :: Float64 := diff * h0;
        dv1 :: Float64 := diff * h1;
        dv2 :: Float64 := diff * h2;
        dc0 :: Float64 := diff;

        # Hidden layer gradients (ReLU derivative: 1 if pre-activation
        # positive, else 0). z > 0 uses the pre-activation, not h.
        d1_0 :: Float64 := 0.0;
        if z0 > 0.0 { d1_0 = diff * v0; }
        d1_1 :: Float64 := 0.0;
        if z1 > 0.0 { d1_1 = diff * v1; }
        d1_2 :: Float64 := 0.0;
        if z2 > 0.0 { d1_2 = diff * v2; }

        # Input-to-hidden gradients.
        dw00 :: Float64 := d1_0 * x0;
        dw10 :: Float64 := d1_0 * x1;
        db0  :: Float64 := d1_0;

        dw01 :: Float64 := d1_1 * x0;
        dw11 :: Float64 := d1_1 * x1;
        db1  :: Float64 := d1_1;

        dw02 :: Float64 := d1_2 * x0;
        dw12 :: Float64 := d1_2 * x1;
        db2  :: Float64 := d1_2;

        # ── SGD update ──
        w00 = w00 - lr * dw00;
        w10 = w10 - lr * dw10;
        b0  = b0  - lr * db0;

        w01 = w01 - lr * dw01;
        w11 = w11 - lr * dw11;
        b1  = b1  - lr * db1;

        w02 = w02 - lr * dw02;
        w12 = w12 - lr * dw12;
        b2  = b2  - lr * db2;

        v0 = v0 - lr * dv0;
        v1 = v1 - lr * dv1;
        v2 = v2 - lr * dv2;
        c0 = c0 - lr * dc0;
    };

    # ── Report ── (outside the region; no escape check applies)
    if iter % report_every == 0 {
        avg_loss :: Float64 := running_loss / running_n;
        acc :: Float64 := running_correct / running_n;

        arduino.serial_print("  ");
        arduino.serial_print(iter);
        arduino.serial_print(" | ");
        arduino.serial_print(avg_loss);
        arduino.serial_print(" | ");
        arduino.serial_print(acc);
        arduino.serial_print(" | ");
        arduino.serial_print(arduino.arena_used());
        arduino.serial_print(" B\n");

        running_loss = 0.0;
        running_correct = 0.0;
        running_n = 0.0;

        # Progress blip
        arduino.digital_write(13, 1);
        arduino.delay(10);
        arduino.digital_write(13, 0);
    }
};

# ═════════════════════════════════════════════════════════════════════
# §7 TEST ON FRESH SAMPLES
# ═════════════════════════════════════════════════════════════════════
#
# 10 samples the network has never seen. Same forward pass as training.
# Prints predicted probability, decided class, and ground truth.

arduino.serial_print("\nTesting on 10 fresh samples:\n");

through test :: 1..10 -> loop {
    x0 :: Float64 := rand_unit();
    x1 :: Float64 := rand_unit();

    y_true :: Int64 := 0;
    if x0 * x1 < 0.0 { y_true = 1; }

    region infer {
        z0 :: Float64 := w00 * x0 + w10 * x1 + b0;
        h0 :: Float64 := z0;
        if h0 < 0.0 { h0 = 0.0; }

        z1 :: Float64 := w01 * x0 + w11 * x1 + b1;
        h1 :: Float64 := z1;
        if h1 < 0.0 { h1 = 0.0; }

        z2 :: Float64 := w02 * x0 + w12 * x1 + b2;
        h2 :: Float64 := z2;
        if h2 < 0.0 { h2 = 0.0; }

        y_pred :: Float64 := v0 * h0 + v1 * h1 + v2 * h2 + c0;

        pred_class :: Int64 := 0;
        if y_pred > 0.5 { pred_class = 1; }

        arduino.serial_print("  (");
        arduino.serial_print(x0);
        arduino.serial_print(", ");
        arduino.serial_print(x1);
        arduino.serial_print(") -> ");
        arduino.serial_print(y_pred);
        arduino.serial_print(" [");
        arduino.serial_print(pred_class);
        arduino.serial_print("/");
        arduino.serial_print(y_true);
        if pred_class == y_true {
            arduino.serial_print("] OK\n");
        } else {
            arduino.serial_print("] MISS\n");
        }
    };
};

# ═════════════════════════════════════════════════════════════════════
# §8 DONE
# ═════════════════════════════════════════════════════════════════════

arduino.serial_print("\nTraining complete.\n");

through i :: 0..4 -> loop {
    arduino.digital_write(13, 1);
    arduino.delay(100);
    arduino.digital_write(13, 0);
    arduino.delay(100);
};
