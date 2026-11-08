# vcsv/vcsv.vy - facade. `use external "vcsv/vcsv.vy"` pulls the whole
# CSV surface.

use "Types.vy";
use "Parse.vy";
use "Columns.vy";
use "Write.vy";

ruleset { dynamic_casting };

module vcsv;

deploy vcsv;
