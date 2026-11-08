# vstat/vstat.vy - facade. `use external "vstat/vstat.vy"` pulls the whole
# statistics surface.

use "Types.vy";
use "Kernels.vy";
use "Reductions.vy";
use "Correlation.vy";
use "Regression.vy";
use "Histogram.vy";
use "Streaming.vy";

ruleset { dynamic_casting };

module vstat;

deploy vstat;
