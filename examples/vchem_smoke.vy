use external "vchem/vchem.vy";

ruleset { dynamic_casting };

# vchem_common: table lookup returns a bare Float64, not a boxed
# VyneValue. If the parallel-array design landed correctly, this
# prints "carbon  12.011".
z :: Int64 = vchem.atomic_number("C");
out(vchem.symbol(z) + "  " + string(vchem.atomic_mass(z)));

# vgraph: 5 nodes, undirected cycle plus one tail. BFS from 0
# should reach everything in at most 3 hops.
from :: Array<Int64> = [0, 1, 2, 3, 4];
to   :: Array<Int64> = [1, 2, 3, 0, 0];
g :: vchem.Graph = vchem.build_unweighted(5, from, to, false);

dist :: Array<Int64> = vchem.bfs_distances(g, 0);
out("components = " + string(vchem.component_count(g)));
out("dist to 4  = " + string(dist[4]));
out("order      = " + string(vchem.dfs_order(g, 0).size()));
