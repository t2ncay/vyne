ruleset { dynamic_casting };

# Zero-copy view: no element is copied at the call site.
fn sum_slice(v :: Slice<Float64>) -> Float64 {
    n :: Int64 := v.size();
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        s = s + v[i];
    };
    return s;
}

fn sum_slice_i(v :: Slice<Int64>) -> Int64 {
    n :: Int64 := v.size();
    s :: Int64 := 0;
    through i :: 0..n-1 -> loop {
        s = s + v[i];
    };
    return s;
}

# reserve()/resize() are additive; push() growth is unchanged.
fn build_buffer() -> Array<Float64> {
    a :: Array<Float64> := [];
    a.reserve(16);
    a.resize(4, 1.5);
    a.resize(2, 0.0);
    a.push(9.0);
    return a;
}

data :: Array<Float64> := [10.0, 20.0, 30.0, 40.0, 50.0, 60.0];

out(sum_slice(data[1:4]));     # 20 + 30 + 40 = 90
out(sum_slice(data[3:6]));     # 40 + 50 + 60 = 150
out(sum_slice(data));          # whole-array view: 210

# A named view is still a borrow, not a copy.
view :: Slice<Float64> := data[1:4];
out(sum_slice(view));          # 90
view = data[4:6];
out(sum_slice(view));          # 50 + 60 = 110

buf :: Array<Float64> := build_buffer();
out(buf.size());               # 3
out(buf[0]);                   # 1.5
out(buf[2]);                   # 9.0

ints :: Array<Int64> := [];
ints.reserve(8);
ints.resize(3, 7);
out(ints.size());              # 3
out(ints[1]);                  # 7
out(sum_slice_i(ints[0:2]));   # 7 + 7 = 14
