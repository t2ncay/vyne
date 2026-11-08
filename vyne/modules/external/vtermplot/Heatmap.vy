# vtermplot/Heatmap.vy - matrix heatmaps on a Plot canvas.

use "Types.vy";
use "Canvas.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot heatmap(plot :: vtermplot.Plot, data :: Array,
                        rows :: Int64, cols :: Int64) -> Int64 {
    n :: Int64 := rows * cols;
    typed :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { typed.push(data[i]); };
    return vtermplot.k_heat(plot, typed, rows, cols);
}

fn :: vtermplot plot_heatmap(data :: Array, rows :: Int64, cols :: Int64,
                             width :: Int64, height :: Int64,
                             title :: String) -> String {
    p :: vtermplot.Plot := vtermplot.new_plot(width, height, title, "", "");
    vtermplot.heatmap(p, data, rows, cols);
    return vtermplot.render(p);
}
