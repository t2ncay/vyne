# vchem/vchem.vy — top-level facade for the vchem namespace.
#
# Day-one surface: only the two leaf modules that have landed. The
# plan explicitly warns against re-exporting half-finished modules:
#
#     "The top-level vchem.vy facade ships with only the four
#     foundational sub-modules on day one: vchem_common, vmol,
#     vgraph, vsmiles. Everything else is added as it stabilizes.
#     A facade that re-exports a half-finished vdock produces
#     confusing errors."
#
# vmol and vsmiles are not yet written, so this file pulls in the two
# that are. When vmol lands, add `use "vmol/vmol.vy";` below. Same for
# vsmiles.
#
# Consumers that only need one sub-module should import it directly
# (`use "vchem/vgraph/vgraph.vy";`) to keep the compile unit small.

use "vchem_common/vchem_common.vy";
use "vgraph/vgraph.vy";
