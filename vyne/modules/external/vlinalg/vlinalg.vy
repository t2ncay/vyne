# vlinalg/vlinalg.vy — facade.
#
# `use "vlinalg.vy"` pulls in the full matrix/vector surface.
# Linker order: Types, then the leaf modules, then this file.

use "Types.vy";
use "Constructors.vy";
use "Ops.vy";
use "Activations.vy";
use "Reductions.vy";
use "Losses.vy";

module vlinalg;

deploy vlinalg;
deploy vmath;