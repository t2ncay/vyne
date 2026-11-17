# Data-only ABI stubs. scripts/bench.py replaces the generated function bodies
# with checked Float32 flash lookups. No inference arithmetic lives in the bridge.
# Fail closed: these stubs must never be deployed without staging the bridge.
fn bench_weight1(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_bias1(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_weight2(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_bias2(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_input(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_expected(index :: Int64) -> Float32 { exit(90); return float32(0.0); }
fn bench_begin() -> Int64 { exit(90); return 0; }
fn bench_end(batch :: Int64, elapsed :: Int64) -> Int64 { exit(90); return 0; }
fn bench_validate(index :: Int64, value :: Float32) -> Int64 { exit(90); return 0; }
fn bench_finish(peak :: Int64) -> Int64 { exit(90); return 0; }
fn bench_observe(value :: Float32) -> Int64 { exit(90); return 0; }
