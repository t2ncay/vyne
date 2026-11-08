# vinterp/vinterp.vy - facade. `use external "vinterp/vinterp.vy"`
# pulls the whole interpolation surface.

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";
use "OneD.vy";
use "TwoD.vy";

ruleset { dynamic_casting };

module vinterp;

deploy vinterp;
