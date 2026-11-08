# vtermplot/vtermplot.vy - facade. `use external "vtermplot/vtermplot.vy"`
# pulls the whole terminal-plotting surface.

use "Types.vy";
use "Themes.vy";
use "Kernels.vy";
use "Canvas.vy";
use "Line.vy";
use "Bar.vy";
use "Heatmap.vy";
use "Sparkline.vy";

ruleset { dynamic_casting };

module vtermplot;

deploy vtermplot;
