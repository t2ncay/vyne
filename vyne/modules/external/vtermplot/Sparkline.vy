# vtermplot/Sparkline.vy - one-line sparklines.

use "Types.vy";
use "Themes.vy";
use native vcore;

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot sparkline_series(s :: vtermplot.SeriesData,
                                 unicode :: Bool) -> String {
    if s.n <= 0 { return ""; }

    minv :: Float64 := s.ys[0];
    maxv :: Float64 := s.ys[0];
    through i :: 1..s.n-1 -> loop {
        if s.ys[i] < minv { minv = s.ys[i]; }
        if s.ys[i] > maxv { maxv = s.ys[i]; }
    };
    span :: Float64 := maxv - minv;
    if span == 0.0 { span = 1.0; }

    sb :: Int64 := vcore.sb_create();
    through i :: 0..s.n-1 -> loop {
        level :: Int64 := int64((s.ys[i] - minv) / span * 7.0);
        vcore.sb_append(sb, vtermplot.spark_char(level, unicode));
    };
    return vcore.sb_build(sb);
}

fn :: vtermplot sparkline(xs :: Array) -> String {
    n :: Int64 := xs.size();
    yd :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { yd.push(xs[i]); };
    return vtermplot.sparkline_series(
        vtermplot.SeriesData(yd, yd, n, "", 1), true
    );
}

fn :: vtermplot sparkline_ascii(xs :: Array) -> String {
    n :: Int64 := xs.size();
    yd :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { yd.push(xs[i]); };
    return vtermplot.sparkline_series(
        vtermplot.SeriesData(yd, yd, n, "", 1), false
    );
}
