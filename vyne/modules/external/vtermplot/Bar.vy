# vtermplot/Bar.vy - bar charts and histograms.

use "Types.vy";
use "Canvas.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot bars(plot :: vtermplot.Plot, ys :: Array,
                     marker :: Int64) -> Int64 {
    n :: Int64 := ys.size();
    typed :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { typed.push(ys[i]); };
    return vtermplot.k_draw_bars(plot, typed, n, marker);
}

fn :: vtermplot bars_series(plot :: vtermplot.Plot,
                            s :: vtermplot.SeriesData) -> Int64 {
    return vtermplot.k_draw_bars(plot, s.ys, s.n, s.marker);
}

fn :: vtermplot hist(plot :: vtermplot.Plot, xs :: Array, bins :: Int64,
                     marker :: Int64) -> Int64 {
    n :: Int64 := xs.size();
    typed :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { typed.push(xs[i]); };
    return vtermplot.k_draw_hist(plot, typed, n, bins, marker);
}

fn :: vtermplot plot_bars(ys :: Array, width :: Int64, height :: Int64,
                          title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.bars(p, ys, 3);
    return vtermplot.render(p);
}

fn :: vtermplot plot_hist(xs :: Array, bins :: Int64,
                          width :: Int64, height :: Int64,
                          title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.hist(p, xs, bins, 3);
    return vtermplot.render(p);
}

fn :: vtermplot bars_series_plot(s :: vtermplot.SeriesData,
                                 width :: Int64, height :: Int64,
                                 title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.bars_series(p, s);
    return vtermplot.render(p);
}
