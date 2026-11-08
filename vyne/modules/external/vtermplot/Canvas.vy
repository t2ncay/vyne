# vtermplot/Canvas.vy - plot construction and text rendering.

use "Types.vy";
use "Themes.vy";
use external "vcolors.vy";
use native vmath;
use native vfs;

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot new_plot(width :: Int64, height :: Int64,
                         title :: String, x_label :: String,
                         y_label :: String) -> vtermplot.Plot {
    if width <= 0 || height <= 0 {
        throw "vtermplot.new_plot: width and height must be positive";
    }
    grid :: Array<Int64> := [];
    through i :: 0..width*height-1 -> loop { grid.push(0); };
    return vtermplot.Plot(
        width, height, title, x_label, y_label, grid,
        0.0, 1.0, 0.0, 1.0, false, true, false, true
    );
}

fn :: vtermplot set_unicode(plot :: vtermplot.Plot, on :: Bool) -> Int64 {
    plot.unicode = on;
    return 0;
}

fn :: vtermplot set_color(plot :: vtermplot.Plot, on :: Bool) -> Int64 {
    plot.color = on;
    return 0;
}

fn :: vtermplot set_grid(plot :: vtermplot.Plot, on :: Bool) -> Int64 {
    plot.grid_on = on;
    return 0;
}

# Copy two boxed arrays into a SeriesData. If you already have typed
# arrays, construct SeriesData directly to skip the copy.
fn :: vtermplot series(xs :: Array, ys :: Array,
                       label :: String, marker :: Int64) -> vtermplot.SeriesData {
    n :: Int64 := xs.size();
    if ys.size() != n {
        throw "vtermplot.series: xs and ys differ in length";
    }
    xd :: Array<Float64> := [];
    yd :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        xd.push(xs[i]);
        yd.push(ys[i]);
    };
    return vtermplot.SeriesData(xd, yd, n, label, marker);
}

fn :: vtermplot repeat(text :: String, count :: Int64) -> String {
    if count <= 0 { return ""; }
    result :: String := "";
    through i :: 0..count-1 -> loop { result = result + text; };
    return result;
}

fn :: vtermplot fmt(x :: Float64) -> String {
    if x == 0.0 { return "0"; }
    scaled :: Float64 := vmath.round(x * 1000.0) / 1000.0;
    return string(scaled);
}

fn :: vtermplot cell_char(code :: Int64, unicode :: Bool) -> String {
    if code == 0 { return " "; }
    if code == 1 { return vtermplot.point_char(unicode); }
    if code == 2 { return vtermplot.line_char(unicode); }
    if code == 3 { return vtermplot.bar_char(unicode); }
    if code >= 5 && code <= 13 {
        return vtermplot.heat_char(code - 5, unicode);
    }
    return " ";
}

fn :: vtermplot render(plot :: vtermplot.Plot) -> String {
    result :: String := "";

    if plot.title != "" { result = result + plot.title + "\n"; }
    if plot.y_label != "" { result = result + plot.y_label + "\n"; }
    if plot.grid_on {
        result = result + "y " + vtermplot.fmt(plot.ymin) + " .. " +
                 vtermplot.fmt(plot.ymax) + "\n";
    }

    vchar :: String := vtermplot.vline_char(plot.unicode);
    through row :: 0..plot.height-1 -> loop {
        if plot.grid_on { result = result + vchar; }
        through col :: 0..plot.width-1 -> loop {
            result = result + vtermplot.cell_char(
                plot.grid[row * plot.width + col], plot.unicode
            );
        };
        if plot.grid_on { result = result + vchar; }
        result = result + "\n";
    };

    if plot.grid_on {
        hchar :: String := vtermplot.hline_char(plot.unicode);
        left :: String := "└";
        right :: String := "┘";
        if !plot.unicode {
            left = "+";
            right = "+";
        }
        result = result + left + vtermplot.repeat(hchar, plot.width) + right + "\n";

        lo :: String := vtermplot.fmt(plot.xmin);
        hi :: String := vtermplot.fmt(plot.xmax);
        pad :: Int64 := plot.width - sizeof(lo) - sizeof(hi);
        if pad < 1 { pad = 1; }
        result = result + " " + lo + vtermplot.repeat(" ", pad) + hi + "\n";
    }

    if plot.x_label != "" {
        pad2 :: Int64 := (plot.width - sizeof(plot.x_label)) / 2;
        if pad2 < 0 { pad2 = 0; }
        result = result + vtermplot.repeat(" ", pad2) + plot.x_label + "\n";
    }
    return result;
}

fn :: vtermplot show(plot :: vtermplot.Plot) {
    if plot.color {
        out(vcolors.cyan(vtermplot.render(plot)));
    } else {
        out(vtermplot.render(plot));
    }
}

fn :: vtermplot save(plot :: vtermplot.Plot, path :: String) -> Bool {
    return vfs.write(path, vtermplot.render(plot));
}
