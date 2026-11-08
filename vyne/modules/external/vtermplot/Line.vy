# vtermplot/Line.vy - line and scatter plots.

use "Types.vy";
use "Canvas.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot line(plot :: vtermplot.Plot, xs :: Array, ys :: Array,
                     marker :: Int64) -> Int64 {
    s :: vtermplot.SeriesData := vtermplot.series(xs, ys, "", marker);
    return vtermplot.k_draw_line(plot, s.xs, s.ys, s.n, marker);
}

fn :: vtermplot line_series(plot :: vtermplot.Plot,
                            s :: vtermplot.SeriesData) -> Int64 {
    return vtermplot.k_draw_line(plot, s.xs, s.ys, s.n, s.marker);
}

fn :: vtermplot scatter(plot :: vtermplot.Plot, xs :: Array, ys :: Array,
                        marker :: Int64) -> Int64 {
    s :: vtermplot.SeriesData := vtermplot.series(xs, ys, "", marker);
    return vtermplot.k_draw_scatter(plot, s.xs, s.ys, s.n, marker);
}

fn :: vtermplot scatter_series(plot :: vtermplot.Plot,
                               s :: vtermplot.SeriesData) -> Int64 {
    return vtermplot.k_draw_scatter(plot, s.xs, s.ys, s.n, s.marker);
}

fn :: vtermplot plot_line(xs :: Array, ys :: Array,
                          width :: Int64, height :: Int64,
                          title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.line(p, xs, ys, 2);
    return vtermplot.render(p);
}

fn :: vtermplot plot_scatter(xs :: Array, ys :: Array,
                             width :: Int64, height :: Int64,
                             title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.scatter(p, xs, ys, 1);
    return vtermplot.render(p);
}

fn :: vtermplot plot_line_series(s :: vtermplot.SeriesData,
                                 width :: Int64, height :: Int64,
                                 title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.line_series(p, s);
    return vtermplot.render(p);
}
