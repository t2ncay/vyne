# voptim_test.vy - optimizer updates, schedules, and allocation profile.
#
#   ./vynec --compile examples/libs/voptim_test.vy
#   ./examples/libs/voptim_test

use external "voptim/voptim.vy";

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

fn near_zero(s :: voptim.Series, eps :: Float64) -> Bool {
    through i :: 0..s.n-1 -> loop {
        d :: Float64 := s.data[i];
        if d < 0.0 { d = 0.0 - d; }
        if d > eps { return false; }
    };
    return true;
}

out("=== voptim ===");

# ---- schedules ------------------------------------------------------
check("constant", close(voptim.constant(5, 0.1), 0.1, 1e-15));
check("step-decay-0", close(voptim.step_decay(0, 0.1, 10, 0.5), 0.1, 1e-15));
check("step-decay-10", close(voptim.step_decay(10, 0.1, 10, 0.5), 0.05, 1e-15));
check("step-decay-20", close(voptim.step_decay(20, 0.1, 10, 0.5), 0.025, 1e-15));
check("exponential", close(voptim.exponential(2, 0.1, 0.5), 0.025, 1e-15));
check("cosine-start", close(voptim.cosine(0, 0.1, 100), 0.1, 1e-15));
check("cosine-mid", close(voptim.cosine(50, 0.1, 100), 0.05, 1e-12));
check("cosine-end", close(voptim.cosine(100, 0.1, 100), 0.0, 1e-15));
check("warmup-start", close(voptim.warmup_cosine(0, 0.1, 10, 100), 0.0, 1e-15));
check("warmup-mid", close(voptim.warmup_cosine(5, 0.1, 10, 100), 0.05, 1e-15));
check("warmup-end", close(voptim.warmup_cosine(100, 0.1, 10, 100), 0.0, 1e-15));

# ---- SGD ------------------------------------------------------------
sgd :: voptim.SGDState := voptim.sgd_new([2.0, -3.0]);
grad_sgd :: Array<Float64> := [0.0, 0.0];
gs :: voptim.Series := voptim.Series(grad_sgd, 2);
through step :: 0..199 -> loop {
    grad_sgd[0] = sgd.params[0];
    grad_sgd[1] = sgd.params[1];
    voptim.sgd_step(sgd, gs, 0.1);
};
check("sgd-converges", near_zero(voptim.Series(sgd.params, 2), 1e-6));

# ---- Momentum and Nesterov -----------------------------------------
mom :: voptim.MomentumState := voptim.momentum_new([2.0, -3.0], 0.9);
grad_mom :: Array<Float64> := [0.0, 0.0];
gm := voptim.Series(grad_mom, 2);
through step2 :: 0..299 -> loop {
    grad_mom[0] = mom.params[0];
    grad_mom[1] = mom.params[1];
    voptim.momentum_step(mom, gm, 0.01);
};
check("momentum-converges", near_zero(voptim.Series(mom.params, 2), 1e-4));

nes :: voptim.MomentumState := voptim.momentum_new([2.0, -3.0], 0.9);
grad_nes :: Array<Float64> := [0.0, 0.0];
gn := voptim.Series(grad_nes, 2);
through step3 :: 0..299 -> loop {
    grad_nes[0] = nes.params[0];
    grad_nes[1] = nes.params[1];
    voptim.nesterov_step(nes, gn, 0.01);
};
check("nesterov-converges", near_zero(voptim.Series(nes.params, 2), 1e-4));

# ---- Adam / AdamW ---------------------------------------------------
adam :: voptim.AdamState := voptim.adam_new([2.0, -3.0], 0.9, 0.999, 1e-8);
grad_adam :: Array<Float64> := [0.0, 0.0];
ga := voptim.Series(grad_adam, 2);
through step4 :: 0..499 -> loop {
    grad_adam[0] = adam.params[0];
    grad_adam[1] = adam.params[1];
    voptim.adam_step(adam, ga, 0.05);
};
check("adam-converges", near_zero(voptim.Series(adam.params, 2), 1e-4));

adamw :: voptim.AdamState := voptim.adamw_new([1.0], 0.9, 0.999, 1e-8, 0.1);
grad_zero :: Array<Float64> := [0.0];
gz := voptim.Series(grad_zero, 1);
through step5 :: 0..99 -> loop {
    voptim.adamw_step(adamw, gz, 0.1);
};
check("adamw-decays", adamw.params[0] < 0.9);

# ---- RMSProp / Adagrad ---------------------------------------------
rms :: voptim.RMSPropState := voptim.rmsprop_new([2.0, -3.0], 0.9, 1e-8);
grad_rms :: Array<Float64> := [0.0, 0.0];
gr := voptim.Series(grad_rms, 2);
through step6 :: 0..999 -> loop {
    grad_rms[0] = rms.params[0];
    grad_rms[1] = rms.params[1];
    voptim.rmsprop_step(rms, gr, 0.01);
};
check("rmsprop-converges", near_zero(voptim.Series(rms.params, 2), 1e-2));

ada :: voptim.AdagradState := voptim.adagrad_new([2.0, -3.0], 1e-8);
grad_ada :: Array<Float64> := [0.0, 0.0];
gad := voptim.Series(grad_ada, 2);
through step7 :: 0..999 -> loop {
    grad_ada[0] = ada.params[0];
    grad_ada[1] = ada.params[1];
    voptim.adagrad_step(ada, gad, 0.1);
};
check("adagrad-converges", near_zero(voptim.Series(ada.params, 2), 5e-2));


if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
