---
id: vchem
title: vchem
group: Domain Libraries
icon: fa-flask
order: 20
description: Periodic-table data, physical constants, CSR graphs, and BFS/DFS/Dijkstra.
---

# vchem

`vchem` is the chemistry and graph layer. The facade re-exports two landed sub-modules:

```vyne
use external "vchem/vchem.vy";
```

| Sub-module | Surface |
| :--- | :--- |
| `vchem_common` | Physical constants and periodic-table accessors for elements 1–54 (H through Xe). |
| `vgraph` | CSR graph construction, BFS, DFS, topological sort, connected components, Dijkstra. |

Import a sub-module directly to keep the compile unit small:

```vyne
use external "vchem/vgraph/vgraph.vy";
```

## Constants

`vchem.Constants` holds SI values from the 2019 redefinition / CODATA 2018:

| Constant | Meaning |
| :--- | :--- |
| `AVOGADRO` | Avogadro constant, mol⁻¹ |
| `BOLTZMANN` | Boltzmann constant, J/K |
| `R_GAS` | Universal gas constant, J/(mol·K) |
| `STD_PRESSURE` | Standard pressure, Pa |
| `STD_TEMPERATURE` | 273.15 K |
| `WATER_BOILING_K` | 373.15 K |
| `BODY_TEMPERATURE_K` | 310.15 K |
| `KT_BODY` | kT at body temperature, J |
| `KCAL_TO_KJ` | 4.184 |
| `KCAL_PER_MOL_TO_KT_300` | 1.6782 |
| `ANGSTROM_TO_NM` / `ANGSTROM_TO_M` | 0.1 / 1e-10 |
| `DEG_TO_RAD` / `RAD_TO_DEG` | conversion factors |
| `TWO_PI`, `PHI` | 2π and the golden ratio |

```vyne
energy := 5.0 * vchem.Constants.KCAL_TO_KJ;
out(vchem.Constants.R_GAS);
```

## Elements

Elements 1–54 are tabulated. Accessors are bounds-checked; out-of-range atomic numbers return a sentinel (`0.0`, `"X"`, or `"unknown"`).

| Function | Result |
| :--- | :--- |
| `vchem.atomic_mass(z)` | Standard atomic weight |
| `vchem.covalent_radius(z)` | Single-bond covalent radius, Å |
| `vchem.vdw_radius(z)` | van der Waals radius, Å |
| `vchem.electronegativity(z)` | Pauling electronegativity |
| `vchem.symbol(z)` | Element symbol |
| `vchem.name(z)` | Element name |
| `vchem.atomic_number(symbol)` | Atomic number, or `0` if unknown |
| `vchem.MAX_Z` | Highest supported atomic number (54) |

```vyne
z := vchem.atomic_number("C");        # 6
out(vchem.atomic_mass(z));            # 12.011
out(vchem.symbol(26));                # Fe
```

## Graphs

A graph is a CSR structure:

```vyne
interface Graph {
    n      :: Int64,          # node count
    m      :: Int64,          # directed-edge count
    adj    :: Array<Int64>,   # neighbour list
    weight :: Array<Float64>, # parallel weights
    offset :: Array<Int64>,   # length n+1
}
```

Construct from an edge list:

```vyne
from    :: Array<Int64>   := [0, 1, 1, 2];
to      :: Array<Int64>   := [1, 2, 3, 3];
weights :: Array<Float64> := [1.0, 2.0, 1.5, 0.5];

g :: vchem.Types.Graph := vchem.build_graph(4, from, to, weights, false);
```

`directed = false` adds both directions. `build_unweighted(n, from, to, directed)` uses weight `1.0` for every edge. `vchem.degree(g, i)` returns the degree of a node.

## Algorithms

| Function | Result |
| :--- | :--- |
| `vchem.bfs_distances(g, src)` | Hop counts, `-1` for unreachable |
| `vchem.bfs_order(g, src)` | Visit order |
| `vchem.dfs_order(g, src)` | DFS preorder |
| `vchem.connected_components(g)` | Component id per node |
| `vchem.component_count(g)` | Number of components |
| `vchem.topological_sort(g)` | Topological order |
| `vchem.dijkstra(g, src)` | Distances, `INF` for unreachable |
| `vchem.dijkstra_with_pred(g, src)` | Predecessor vector |
| `vchem.reconstruct_path(pred, src, dst)` | Node sequence |
| `vchem.shortest_distance(g, src, dst)` | Single-pair distance |
| `vchem.shortest_path(g, src, dst)` | Single-pair path |

```vyne
dist := vchem.bfs_distances(g, 0);
out(dist[3]);

hop := vchem.shortest_path(g, 0, 3);
out(hop);
```

Dijkstra is the O(V²) dense implementation and requires non-negative weights. `vchem.INF` is `1.0e300`; test against it for unreachable nodes. For many single-pair queries, run `dijkstra` once and index the result.

## Status

`vchem_common` and `vgraph` are the stable day-one surface. `vmol` and `vsmiles` are planned but not yet written; the facade deliberately does not re-export half-finished modules.

## See also

- [Library Overview](page:libs)
- [vsparse](page:vsparse)
- [vbio](page:vbio)
