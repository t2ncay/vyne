# Float32 arithmetic and bounded activation storage are compiled from Vyne.
# Build with scripts/bench.py: the data-only bridge embeds model.vwt in flash.
use native esp32;
use "data_bridge.vy";

bench_begin();
# Actual Float32 scratch: (64 + 32 + 10) * 4 = 424 bytes.
# Current compiler accounting uses 8 bytes per scratch element: 848 bytes.
@bounded<848> region inference {
    scratch input :: Float32[64];
    scratch hidden :: Float32[32];
    scratch output :: Float32[10];
    through batch :: -1..19 -> loop {
        repetitions :: Int64 := 100;
        if batch == -1 { repetitions = 10; }
        started :: Int64 := esp32.timer_read();
        through iteration :: 0..repetitions - 1 -> loop {
            vector :: Int64 := iteration % 8;
            through i :: 0..63 -> loop { input[i] = bench_input(vector * 64 + i); };
            through j :: 0..31 -> loop {
                acc :: Float32 := bench_bias1(j);
                through k :: 0..63 -> loop {
                    acc = float32(acc + float32(input[k] * bench_weight1(j * 64 + k)));
                };
                if acc < float32(0.0) { acc = float32(0.0); }
                hidden[j] = acc;
            };
            through j :: 0..9 -> loop {
                acc :: Float32 := bench_bias2(j);
                through k :: 0..31 -> loop {
                    acc = float32(acc + float32(hidden[k] * bench_weight2(j * 32 + k)));
                };
                output[j] = acc;
                bench_observe(output[j]);
            };
            # Validation is not in the timed loop; the bridge stores elapsed
            # after the whole batch and validates all 8 inputs in separate runs.
        };
        elapsed :: Int64 := esp32.timer_read() - started;
        if batch >= 0 { bench_end(batch, elapsed); }
    };
    through vector :: 0..7 -> loop {
        through i :: 0..63 -> loop { input[i] = bench_input(vector * 64 + i); };
        through j :: 0..31 -> loop {
            acc :: Float32 := bench_bias1(j);
            through k :: 0..63 -> loop { acc = float32(acc + float32(input[k] * bench_weight1(j * 64 + k))); };
            if acc < float32(0.0) { acc = float32(0.0); }
            hidden[j] = acc;
        };
        through j :: 0..9 -> loop {
            acc :: Float32 := bench_bias2(j);
            through k :: 0..31 -> loop { acc = float32(acc + float32(hidden[k] * bench_weight2(j * 32 + k))); };
            output[j] = acc;
            bench_validate(vector * 10 + j, output[j]);
        };
    };
};
arena_peak :: Int64 := esp32.arena_peak();
bench_finish(arena_peak);
