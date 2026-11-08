# vtermplot/Types.vy - canvas and series types.
#
# A Plot owns a flat width*height grid of small integer codes:
#   0 empty, 1 point, 2 line, 3 bar, 5..13 heat levels.
# The renderer maps codes to Unicode or ASCII glyphs. SeriesData carries
# a copied typed array pair plus a label and a marker code.

ruleset { dynamic_casting };

module vtermplot;

interface SeriesData {
    xs     :: Array<Float64>,
    ys     :: Array<Float64>,
    n      :: Int64,
    label  :: String,
    marker :: Int64,
}

interface Plot {
    width    :: Int64,
    height   :: Int64,
    title    :: String,
    x_label  :: String,
    y_label  :: String,
    grid     :: Array<Int64>,
    xmin     :: Float64,
    xmax     :: Float64,
    ymin     :: Float64,
    ymax     :: Float64,
    has_data :: Bool,
    unicode  :: Bool,
    color    :: Bool,
    grid_on  :: Bool,
}
