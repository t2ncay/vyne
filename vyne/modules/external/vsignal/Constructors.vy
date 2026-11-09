# vsignal/Constructors.vy - Series construction and conversion.
#
# Use `vsignal.Series(data, n)` directly when you already have a typed
# `Array<Float64>` and its length; that is a zero-copy view. `from_array`
# copies a boxed `Array` once.

use "Types.vy";

ruleset { dynamic_casting };

module vsignal;

fn :: vsignal from_array(xs :: Array) -> vsignal.Series {
    n :: Int64 := xs.size();
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        data.push(float64(xs[i]));
    };
    return vsignal.Series(data, n);
}

fn :: vsignal to_array(s :: vsignal.Series) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop {
        data.push(s.data[i]);
    };
    return data;
}

fn :: vsignal zeros(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return data;
}
