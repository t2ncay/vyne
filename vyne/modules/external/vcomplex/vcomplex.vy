# vcomplex/vcomplex.vy - facade. `use external "vcomplex/vcomplex.vy"`
# pulls the whole complex-number surface.

use "Types.vy";
use "Kernels.vy";
use "Ops.vy";
use "Functions.vy";
use "Polar.vy";
use "Arrays.vy";

ruleset { dynamic_casting };

module vcomplex;

deploy vcomplex;
