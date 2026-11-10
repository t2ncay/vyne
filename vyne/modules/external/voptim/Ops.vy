# voptim/Ops.vy - state constructors and step wrappers.
#
# Constructors copy a boxed Array once. After that, every step updates the
# parameter and moment buffers in place and allocates nothing. The step
# wrappers validate lengths, then call the native-ABI kernels with the
# state's typed fields.
#
# For zero-copy setup, construct the interface directly:
#   voptim.SGDState(params, params.size())
#   voptim.AdamState(params, m, v, b1, b2, eps, 0.0, 0, n)

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module voptim;
use native vmath;

fn :: voptim vector(xs :: Array) -> voptim.Series {
    n :: Int64 := xs.size();
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(xs[i]); };
    return voptim.Series(data, n);
}

fn :: voptim to_array(s :: voptim.Series) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop { data.push(s.data[i]); };
    return data;
}

fn :: voptim zeros(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return data;
}

# ---------------------------------------------------------------------------
# SGD
# ---------------------------------------------------------------------------

fn :: voptim sgd_new(xs :: Array) -> voptim.SGDState {
    s :: voptim.Series := voptim.vector(xs);
    return voptim.SGDState(s.data, s.n);
}

fn :: voptim sgd_step(state ^^ :: voptim.SGDState,
                      grads :: voptim.Series,
                      lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.sgd_step: gradient length does not match parameters";
    }
    voptim.k_sgd(state.params, grads.data, lr, state.n);
}

fn :: voptim sgd_params(state :: voptim.SGDState) -> Array<Float64> {
    return state.params;
}

# ---------------------------------------------------------------------------
# Momentum / Nesterov
# ---------------------------------------------------------------------------

fn :: voptim momentum_new(xs :: Array,
                          momentum :: Float64) -> voptim.MomentumState {
    s :: voptim.Series := voptim.vector(xs);
    velocity :: Array<Float64> := voptim.zeros(s.n);
    return voptim.MomentumState(s.data, velocity, momentum, s.n);
}

fn :: voptim momentum_step(state ^^ :: voptim.MomentumState,
                           grads :: voptim.Series,
                           lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.momentum_step: gradient length does not match parameters";
    }
    voptim.k_sgd_momentum(state.params, grads.data, state.velocity,
                          lr, state.momentum, state.n);
}

fn :: voptim nesterov_step(state ^^ :: voptim.MomentumState,
                           grads :: voptim.Series,
                           lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.nesterov_step: gradient length does not match parameters";
    }
    voptim.k_nesterov(state.params, grads.data, state.velocity,
                      lr, state.momentum, state.n);
}

# ---------------------------------------------------------------------------
# Adam / AdamW
# ---------------------------------------------------------------------------

fn :: voptim adam_new(xs :: Array, beta1 :: Float64, beta2 :: Float64,
                      eps :: Float64) -> voptim.AdamState {
    s :: voptim.Series := voptim.vector(xs);
    m :: Array<Float64> := voptim.zeros(s.n);
    v :: Array<Float64> := voptim.zeros(s.n);
    return voptim.AdamState(s.data, m, v, beta1, beta2, eps, 0.0, 0, s.n);
}

fn :: voptim adamw_new(xs :: Array, beta1 :: Float64, beta2 :: Float64,
                       eps :: Float64,
                       weight_decay :: Float64) -> voptim.AdamState {
    s :: voptim.Series := voptim.vector(xs);
    m :: Array<Float64> := voptim.zeros(s.n);
    v :: Array<Float64> := voptim.zeros(s.n);
    return voptim.AdamState(s.data, m, v, beta1, beta2, eps,
                            weight_decay, 0, s.n);
}

fn :: voptim adam_step(state ^^ :: voptim.AdamState,
                       grads :: voptim.Series,
                       lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.adam_step: gradient length does not match parameters";
    }
    t :: Int64 := state.t + 1;
    state.t = t;
    bc1 :: Float64 := 1.0 - vmath.pow(state.beta1, float64(t));
    bc2 :: Float64 := 1.0 - vmath.pow(state.beta2, float64(t));
    voptim.k_adam(state.params, grads.data, state.m, state.v,
                  lr, state.beta1, state.beta2, state.eps, bc1, bc2,
                  state.n);
}

fn :: voptim adamw_step(state ^^ :: voptim.AdamState,
                        grads :: voptim.Series,
                        lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.adamw_step: gradient length does not match parameters";
    }
    t :: Int64 := state.t + 1;
    state.t = t;
    bc1 :: Float64 := 1.0 - vmath.pow(state.beta1, float64(t));
    bc2 :: Float64 := 1.0 - vmath.pow(state.beta2, float64(t));
    voptim.k_adamw(state.params, grads.data, state.m, state.v,
                   lr, state.beta1, state.beta2, state.eps, bc1, bc2,
                   state.weight_decay, state.n);
}

fn :: voptim adam_params(state :: voptim.AdamState) -> Array<Float64> {
    return state.params;
}

# ---------------------------------------------------------------------------
# RMSProp / Adagrad
# ---------------------------------------------------------------------------

fn :: voptim rmsprop_new(xs :: Array, decay :: Float64,
                         eps :: Float64) -> voptim.RMSPropState {
    s :: voptim.Series := voptim.vector(xs);
    cache :: Array<Float64> := voptim.zeros(s.n);
    return voptim.RMSPropState(s.data, cache, decay, eps, s.n);
}

fn :: voptim rmsprop_step(state ^^ :: voptim.RMSPropState,
                          grads :: voptim.Series,
                          lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.rmsprop_step: gradient length does not match parameters";
    }
    voptim.k_rmsprop(state.params, grads.data, state.cache,
                     lr, state.decay, state.eps, state.n);
}

fn :: voptim adagrad_new(xs :: Array, eps :: Float64) -> voptim.AdagradState {
    s :: voptim.Series := voptim.vector(xs);
    accum :: Array<Float64> := voptim.zeros(s.n);
    return voptim.AdagradState(s.data, accum, eps, s.n);
}

fn :: voptim adagrad_step(state ^^ :: voptim.AdagradState,
                          grads :: voptim.Series,
                          lr :: Float64) {
    if grads.n != state.n {
        throw "voptim.adagrad_step: gradient length does not match parameters";
    }
    voptim.k_adagrad(state.params, grads.data, state.accum,
                     lr, state.eps, state.n);
}
