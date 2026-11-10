# voptim/Kernels.vy - native-ABI optimizer updates.
#
# Each function takes typed arrays plus explicit lengths and writes the
# parameter buffer in place. They are leaves: no other array-parameter
# function is called from here, which keeps them eligible for the native
# array ABI and lets the emitted C be a bare loop.

ruleset { dynamic_casting };

module voptim;
use native vmath;

fn :: voptim k_sgd(params :: Array<Float64>, grads :: Array<Float64>,
                   lr :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        params[i] = params[i] - lr * grads[i];
    };
    return 0;
}

fn :: voptim k_sgd_momentum(params :: Array<Float64>,
                            grads :: Array<Float64>,
                            velocity :: Array<Float64>,
                            lr :: Float64, momentum :: Float64,
                            n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        v :: Float64 := momentum * velocity[i] + grads[i];
        velocity[i] = v;
        params[i] = params[i] - lr * v;
    };
    return 0;
}

fn :: voptim k_nesterov(params :: Array<Float64>,
                        grads :: Array<Float64>,
                        velocity :: Array<Float64>,
                        lr :: Float64, momentum :: Float64,
                        n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        v :: Float64 := momentum * velocity[i] + grads[i];
        velocity[i] = v;
        params[i] = params[i] - lr * (grads[i] + momentum * v);
    };
    return 0;
}

fn :: voptim k_adam(params :: Array<Float64>, grads :: Array<Float64>,
                    m :: Array<Float64>, v :: Array<Float64>,
                    lr :: Float64, beta1 :: Float64, beta2 :: Float64,
                    eps :: Float64, bc1 :: Float64, bc2 :: Float64,
                    n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        g :: Float64 := grads[i];
        mi :: Float64 := beta1 * m[i] + (1.0 - beta1) * g;
        vi :: Float64 := beta2 * v[i] + (1.0 - beta2) * g * g;
        m[i] = mi;
        v[i] = vi;
        mh :: Float64 := mi / bc1;
        vh :: Float64 := vi / bc2;
        params[i] = params[i] - lr * mh / (vmath.sqrt(vh) + eps);
    };
    return 0;
}

fn :: voptim k_adamw(params :: Array<Float64>, grads :: Array<Float64>,
                     m :: Array<Float64>, v :: Array<Float64>,
                     lr :: Float64, beta1 :: Float64, beta2 :: Float64,
                     eps :: Float64, bc1 :: Float64, bc2 :: Float64,
                     weight_decay :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        g :: Float64 := grads[i];
        mi :: Float64 := beta1 * m[i] + (1.0 - beta1) * g;
        vi :: Float64 := beta2 * v[i] + (1.0 - beta2) * g * g;
        m[i] = mi;
        v[i] = vi;
        mh :: Float64 := mi / bc1;
        vh :: Float64 := vi / bc2;
        decayed :: Float64 := params[i] * (1.0 - lr * weight_decay);
        params[i] = decayed - lr * mh / (vmath.sqrt(vh) + eps);
    };
    return 0;
}

fn :: voptim k_rmsprop(params :: Array<Float64>, grads :: Array<Float64>,
                       cache :: Array<Float64>, lr :: Float64,
                       decay :: Float64, eps :: Float64,
                       n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        c :: Float64 := decay * cache[i] + (1.0 - decay) * grads[i] * grads[i];
        cache[i] = c;
        params[i] = params[i] - lr * grads[i] / (vmath.sqrt(c) + eps);
    };
    return 0;
}

fn :: voptim k_adagrad(params :: Array<Float64>, grads :: Array<Float64>,
                       accum :: Array<Float64>, lr :: Float64,
                       eps :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        a :: Float64 := accum[i] + grads[i] * grads[i];
        accum[i] = a;
        params[i] = params[i] - lr * grads[i] / (vmath.sqrt(a) + eps);
    };
    return 0;
}
