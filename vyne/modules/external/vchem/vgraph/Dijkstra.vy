# vgraph/Dijkstra.vy — single-source shortest paths on weighted graphs.
#
# The classic O(V^2) dense implementation. Every iteration scans the
# frontier for the minimum-distance unsettled node, settles it, and
# relaxes its outgoing edges. For the graph sizes typical of molecular
# work (a few hundred atoms, or a few thousand for a protein) this is
# competitive with a heap. When a profile shows otherwise, swap in a
# binary heap keyed on distance — the signature of dijkstra() does not
# change.
#
# Requires: all edge weights >= 0. Negative weights need Bellman-Ford,
# which is not in this file.

ruleset { dynamic_casting };

module vchem;

# Distance vector. INF is represented by a very large positive
# Float64; the returned array has INF for unreachable nodes. The
# specific value matters only in that INF + anything must still be
# INF in floating point — which 1e300 satisfies.
INF :: Float64 = 1.0e300;

fn :: vchem dijkstra(g :: vchem.Graph, src :: Int64) -> Array<Float64> {
    dist :: Array<Float64> = [];
    settled :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop {
        dist.push(vchem.INF);
        settled.push(0);
    };
    dist[src] = 0.0;

    iteration :: Int64 = 0;
    while iteration < g.n {
        # Find the minimum-distance unsettled node.
        best :: Int64 = -1;
        best_d :: Float64 = vchem.INF;
        through i :: 0..g.n-1 -> loop {
            if settled[i] == 0 && dist[i] < best_d {
                best = i;
                best_d = dist[i];
            }
        };

        # No reachable unsettled node remains.
        if best < 0 { break; }
        settled[best] = 1;

        # Relax outgoing edges.
        lo :: Int64 = g.offset[best];
        hi :: Int64 = g.offset[best + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            wv :: Float64 = g.weight[e];
            candidate :: Float64 = best_d + wv;
            if candidate < dist[v] {
                dist[v] = candidate;
            }
        };

        iteration = iteration + 1;
    };

    return dist;
}

# Same as dijkstra(), plus the predecessor vector needed to
# reconstruct paths. `pred[i]` is the node that precedes i on the
# shortest path from src, or -1 if i is the source or unreachable.
fn :: vchem dijkstra_with_pred(
    g   :: vchem.Graph,
    src :: Int64
) -> Array<Int64> {
    dist :: Array<Float64> = [];
    pred :: Array<Int64> = [];
    settled :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop {
        dist.push(vchem.INF);
        pred.push(-1);
        settled.push(0);
    };
    dist[src] = 0.0;

    iteration :: Int64 = 0;
    while iteration < g.n {
        best :: Int64 = -1;
        best_d :: Float64 = vchem.INF;
        through i :: 0..g.n-1 -> loop {
            if settled[i] == 0 && dist[i] < best_d {
                best = i;
                best_d = dist[i];
            }
        };
        if best < 0 { break; }
        settled[best] = 1;

        lo :: Int64 = g.offset[best];
        hi :: Int64 = g.offset[best + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            wv :: Float64 = g.weight[e];
            candidate :: Float64 = best_d + wv;
            if candidate < dist[v] {
                dist[v] = candidate;
                pred[v] = best;
            }
        };

        iteration = iteration + 1;
    };

    return pred;
}

# Reconstruct a path from src to dst using the predecessor vector
# returned by dijkstra_with_pred(). Returns the node sequence from
# src to dst inclusive. Empty array if dst is unreachable.
fn :: vchem reconstruct_path(
    pred :: Array<Int64>,
    src  :: Int64,
    dst  :: Int64
) -> Array<Int64> {
    if src == dst { return [src]; }

    path :: Array<Int64> = [];
    cur :: Int64 = dst;
    while cur != -1 {
        path.push(cur);
        if cur == src { break; }
        cur = pred[cur];
    };

    if path[path.size() - 1] != src {
        return [];
    }

    # Reverse in place.
    lo :: Int64 = 0;
    hi :: Int64 = path.size() - 1;
    while lo < hi {
        tmp :: Int64 = path[lo];
        path[lo] = path[hi];
        path[hi] = tmp;
        lo = lo + 1;
        hi = hi - 1;
    };

    return path;
}

# Single-pair convenience: shortest distance from src to dst.
# Returns INF if dst is unreachable. Runs the full Dijkstra; if you
# need many single-pair queries on the same graph, call dijkstra()
# once and index the result.
fn :: vchem shortest_distance(
    g   :: vchem.Graph,
    src :: Int64,
    dst :: Int64
) -> Float64 {
    dist :: Array<Float64> = vchem.dijkstra(g, src);
    return dist[dst];
}

# Single-pair path convenience: node sequence from src to dst.
# Empty array if unreachable.
fn :: vchem shortest_path(
    g   :: vchem.Graph,
    src :: Int64,
    dst :: Int64
) -> Array<Int64> {
    pred :: Array<Int64> = vchem.dijkstra_with_pred(g, src);
    return vchem.reconstruct_path(pred, src, dst);
}
