# vcontrol/vcontrol.vy - facade. `use external "vcontrol/vcontrol.vy"`
# pulls the control and estimation surface: PID, complementary filter,
# scalar and constant-velocity Kalman filters, and a two-state
# state-space model.
#
# Pure Vyne; no compiler features beyond ordinary interfaces and arrays.
# Composes with `vsignal` for sensor pre-filtering.

use "Types.vy";
use "Pid.vy";
use "Filters.vy";
use "Kalman.vy";
use "StateSpace.vy";

ruleset { dynamic_casting };

module vcontrol;

deploy vcontrol;
