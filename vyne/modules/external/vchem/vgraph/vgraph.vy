# vgraph/vgraph.vy — sub-facade for graph algorithms.
#
# The plan calls for pulling in every algorithm file so a consumer
# that imports vgraph gets the whole API. Each leaf file declares
# `module vchem;` and shares the Graph interface; the facade is just
# a name-binding convenience.

use "Types.vy";
use "BFS.vy";
use "DFS.vy";
use "Dijkstra.vy";
