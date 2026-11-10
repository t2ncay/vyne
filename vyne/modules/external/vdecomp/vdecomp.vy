# vdecomp/vdecomp.vy - facade. `use external "vdecomp/vdecomp.vy"` pulls
# LU, QR, Cholesky, symmetric eigen, and singular values.
#
# Pure Vyne. The raw routines take flat row-major arrays plus dimensions;
# the Ops wrappers take vdecomp.Matrix / vdecomp.Series structs.

use "Types.vy";
use "Util.vy";
use "LU.vy";
use "QR.vy";
use "Cholesky.vy";
use "Eigen.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vdecomp;

deploy vdecomp;
