# vcontrol_bench.vy - controller/estimator step throughput.
#
#   ./vynec --compile examples/libs/vcontrol_bench.vy
#   ./examples/libs/vcontrol_bench

use external "vcontrol/vcontrol.vy";
use native vcore;

out("vcontrol benchmark");

N :: Int64 := 1000000;

pid :: vcontrol.Pid := vcontrol.pid_new(2.0, 5.0, 0.05, 0.01,
                                        -10.0, 10.0, 0.05);
acc_pid :: Float64 := 0.0;
t0 :: Int64 := vcore.now_ns();
through i :: 0..N-1 -> loop {
    meas :: Float64 := float64(i % 100) / 100.0;
    acc_pid = acc_pid + vcontrol.pid_step(pid, 1.0, meas);
};
t1 :: Int64 := vcore.now_ns();
out("pid_step 1M          : " + string((t1 - t0) / 1000000) + " ms (sum=" + string(acc_pid) + ")");

cf :: vcontrol.ComplementaryFilter := vcontrol.cf_new(0.98);
acc_cf :: Float64 := 0.0;
t2 :: Int64 := vcore.now_ns();
through i :: 0..N-1 -> loop {
    acc_cf = acc_cf + vcontrol.cf_step(cf, 10.0, 0.0, 0.01);
};
t3 :: Int64 := vcore.now_ns();
out("cf_step 1M           : " + string((t3 - t2) / 1000000) + " ms (sum=" + string(acc_cf) + ")");

kf1 :: vcontrol.Kalman1D := vcontrol.kf1_new(0.01, 1.0);
acc_kf1 :: Float64 := 0.0;
t4 :: Int64 := vcore.now_ns();
through i :: 0..N-1 -> loop {
    z :: Float64 := 5.0 + (float64(i % 5) - 2.0) * 0.1;
    acc_kf1 = acc_kf1 + vcontrol.kf1_step(kf1, z);
};
t5 :: Int64 := vcore.now_ns();
out("kf1_step 1M          : " + string((t5 - t4) / 1000000) + " ms (sum=" + string(acc_kf1) + ")");

kf2 :: vcontrol.Kalman2 := vcontrol.kf2_new(0.1, 1.0);
acc_kf2 :: Float64 := 0.0;
t6 :: Int64 := vcore.now_ns();
through i :: 0..199999 -> loop {
    z2 :: Float64 := 2.0 * float64(i) * 0.01;
    acc_kf2 = acc_kf2 + vcontrol.kf2_step(kf2, z2, 0.01);
};
t7 :: Int64 := vcore.now_ns();
out("kf2_step 200k        : " + string((t7 - t6) / 1000000) + " ms (sum=" + string(acc_kf2) + ")");

out("done.");
