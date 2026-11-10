# vsparse/vsparse.vy - facade. `use external "vsparse/vsparse.vy"` pulls
# CSR construction, SpMV, residuals, and the conjugate-gradient solver.
#
# Pure Vyne over typed arrays. `spmv` and `cg` lower to native-ABI kernels;
# CG keeps peak memory at O(n) regardless of iteration count.

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vsparse;

deploy vsparse;
