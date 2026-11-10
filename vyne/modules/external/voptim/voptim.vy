# voptim/voptim.vy - facade. `use external "voptim/voptim.vy"` pulls the
# optimizer states, native update kernels, and learning-rate schedules.
#
# Pure Vyne over typed arrays. Each step is in-place and allocation-free
# once the state is built, so a training loop keeps a flat arena profile.

use "Types.vy";
use "Kernels.vy";
use "Schedules.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module voptim;

deploy voptim;
