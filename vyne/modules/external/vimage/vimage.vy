# vimage/vimage.vy - facade. `use external "vimage/vimage.vy"` pulls the
# whole image I/O surface.

use "Types.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vimage;

deploy vimage;
