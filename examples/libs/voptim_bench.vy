# voptim_bench.vy - optimizer update throughput on 100k parameters.
#
#   ./vynec --compile examples/libs/voptim_bench.vy
#   ./examples/libs/voptim_bench

use external "voptim/voptim.vy";
use native vcore;

out("voptim benchmark");

N :: Int64 := 100000;
STEPS :: Int64 := 100;

params :: Array<Float64> := [];
grads :: Array<Float64> := [];
through i :: 0..N-1 -> loop {
    params.push(1.0 + float64(i % 7) * 0.01);
    grads.push(0.001 * float64(i % 11));
};
g :: voptim.Series := voptim.Series(grads, N);

sgd :: voptim.SGDState := voptim.SGDState(params, N);
t0 :: Int64 := vcore.now_ns();
through s :: 0..STEPS-1 -> loop {
    voptim.sgd_step(sgd, g, 0.001);
};
t1 :: Int64 := vcore.now_ns();
out("sgd 100k x 100     : " + string((t1 - t0) / 1000000) + " ms");

adam_params :: Array<Float64> := [];
through i :: 0..N-1 -> loop { adam_params.push(params[i]); };
adam :: voptim.AdamState := voptim.adam_new(adam_params, 0.9, 0.999, 1e-8);
t2 :: Int64 := vcore.now_ns();
through s :: 0..STEPS-1 -> loop {
    voptim.adam_step(adam, g, 0.001);
};
t3 :: Int64 := vcore.now_ns();
out("adam 100k x 100    : " + string((t3 - t2) / 1000000) + " ms");

out("done.");
