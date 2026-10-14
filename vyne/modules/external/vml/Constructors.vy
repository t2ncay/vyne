# vml/Constructors.vy — factories for ML objects.

ruleset { dynamic_casting };

use "Types.vy";
use native vmath;
use external "vlin/vlin.vy";

module vml;

fn :: vml dense(in_features :: Int64, out_features :: Int64,
                activation :: String) -> vml.Types.Dense {
    W :: vlin.Types.Matrix := vlin.xavier_init(in_features, out_features);
    b :: Array<Float64> := [];
    through i :: 0..out_features-1 -> loop { b.push(0.0); };
    return vml.Types.Dense(W, b, activation);
}

fn :: vml dense_scaled(in_features :: Int64, out_features :: Int64,
                       activation :: String, scale :: Float64) -> vml.Types.Dense {
    W :: vlin.Types.Matrix := vlin.multiply_scalar(
        vlin.xavier_init(in_features, out_features), scale);
    b :: Array<Float64> := [];
    through i :: 0..out_features-1 -> loop { b.push(0.0); };
    return vml.Types.Dense(W, b, activation);
}

fn :: vml sequential(layers :: Array) -> vml.Types.Sequential {
    return vml.Types.Sequential(layers);
}

fn :: vml sgd(lr :: Float64) -> vml.Types.SGD {
    return vml.Types.SGD(lr);
}

# Adam with the standard defaults: beta1 = 0.9, beta2 = 0.999,
# eps = 1e-8.
fn :: vml adam(lr :: Float64) -> vml.Types.Adam {
    return vml.Types.Adam(lr, 0.9, 0.999, 0.00000001);
}

# Adam with explicit hyperparameters.
fn :: vml adam_custom(lr :: Float64, beta1 :: Float64,
                      beta2 :: Float64, eps :: Float64) -> vml.Types.Adam {
    return vml.Types.Adam(lr, beta1, beta2, eps);
}

# Fresh per-weight state. m and v both start at zero, which is what
# the bias-correction terms in k_adam_step expect.
fn :: vml adam_state(n :: Int64) -> vml.Types.AdamState {
    m :: Array<Float64> := vlin.zeros_f64(n);
    v :: Array<Float64> := vlin.zeros_f64(n);
    return vml.Types.AdamState(m, v);
}

# Gather rows [indices[0], indices[1], ...] from `source` into `output`.
# source is [total_rows x row_size] row-major; output is
# [batch_size x row_size]. All params native-ABI eligible.
fn :: vml k_gather_rows(output :: Array<Float64>, source :: Array<Float64>,
                        indices :: Array<Int64>,
                        row_size :: Int64, batch_size :: Int64) -> Int64 {
    through b :: 0..batch_size-1 -> loop {
        idx :: Int64 := indices[b];
        through j :: 0..row_size-1 -> loop {
            output[b * row_size + j] = source[idx * row_size + j];
        };
    };
    return 0;
}

# In-place Adam update.
#
#   m = beta1 * m + (1 - beta1) * g
#   v = beta2 * v + (1 - beta2) * g * g
#   m_hat = m / (1 - beta1^t)
#   v_hat = v / (1 - beta2^t)
#   W = W - lr * m_hat / (sqrt(v_hat) + eps)
#
# The bias-correction factors bc1, bc2 depend only on t, so they are
# computed once outside the loop.
fn :: vml k_adam_step(W :: Array<Float64>, grad :: Array<Float64>,
                      m :: Array<Float64>, v :: Array<Float64>,
                      lr :: Float64, beta1 :: Float64, beta2 :: Float64,
                      eps :: Float64, t :: Int64, n :: Int64) -> Int64 {
    tf       :: Float64 := float64(t);
    bc1      :: Float64 := 1.0 - vmath.pow(beta1, tf);
    bc2      :: Float64 := 1.0 - vmath.pow(beta2, tf);
    one_m_b1 :: Float64 := 1.0 - beta1;
    one_m_b2 :: Float64 := 1.0 - beta2;

    through i :: 0..n-1 -> loop {
        g :: Float64 := grad[i];
        m[i] = beta1 * m[i] + one_m_b1 * g;
        v[i] = beta2 * v[i] + one_m_b2 * g * g;
        m_hat :: Float64 := m[i] / bc1;
        v_hat :: Float64 := v[i] / bc2;
        W[i] = W[i] - lr * m_hat / (vmath.sqrt(v_hat) + eps);
    };
    return 0;
}
