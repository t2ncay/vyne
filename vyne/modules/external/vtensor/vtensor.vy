# vtensor/vtensor.vy - facade. `use external "vtensor/vtensor.vy"` pulls the
# minimal device tensor metadata surface.

use "Types.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vtensor;

deploy vtensor;
