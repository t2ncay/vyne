# vgraph/Types.vy — graph representation and construction.
#
# Compressed sparse row (CSR) representation. Every graph is stored as
# three parallel arrays:
#
#     adj     — flattened neighbour list
#     weight  — parallel to adj; the weight of each edge
#     offset  — length n+1; adj[offset[i] .. offset[i+1]-1] are the
#               neighbours of node i
#
# This is the standard representation used by every serious graph
# library (Boost, igraph, scipy.sparse). It is cache-friendly for
# iteration and takes O(V + E) memory. The alternative — an
# Array<Array<Int64>> — would need nested typed arrays, which the
# current C-eligibility pass does not accept (see the comment on
# memberIsCEligible in codegen/interfaces.cpp).

ruleset { dynamic_casting };

module vchem;

interface Graph {
    n      :: Int64,          # node count
    m      :: Int64,          # directed-edge count
    adj    :: Array<Int64>,   # neighbour list, length m
    weight :: Array<Float64>, # edge weights, length m
    offset :: Array<Int64>    # length n+1
};

# Build a graph from an edge list. `from[k] -> to[k]` with the given
# `w[k]` for k in 0..m-1.
#
# `directed` false adds both directions. This is the standard idiom
# for molecular graphs, which are almost always undirected (a bond
# between atoms i and j is the same bond regardless of which atom you
# started from).
#
# Two passes. First pass counts degrees so the offset array can be
# prefix-summed in one sweep. Second pass fills adj and weight.
# Total O(V + E), no map, no sort, no hash.
fn :: vchem build_graph(
    n        :: Int64,
    from     :: Array<Int64>,
    to       :: Array<Int64>,
    w        :: Array<Float64>,
    directed :: Bool
) -> vchem.Graph {
    # Effective edge count depends on directionality. A directed graph
    # stores one entry per input edge; an undirected graph stores two
    # (i->j and j->i).
    input_m :: Int64 = from.size();
    effective_m :: Int64 = input_m;
    if !directed { effective_m = input_m * 2; }

    # Pass 1: degree count.
    degree :: Array<Int64> = [];
    through i :: 0..n-1 -> loop { degree.push(0); };

    through k :: 0..input_m-1 -> loop {
        a :: Int64 = from[k];
        b :: Int64 = to[k];
        degree[a] = degree[a] + 1;
        if !directed { degree[b] = degree[b] + 1; }
    };

    # Pass 2: prefix-sum to get offsets.
    offset :: Array<Int64> = [];
    running :: Int64 = 0;
    through i :: 0..n-1 -> loop {
        offset.push(running);
        running = running + degree[i];
    };
    offset.push(running);   # sentinel slot at the end

    # Pass 3: fill adjacency using a per-node write cursor.
    adj :: Array<Int64> = [];
    weight :: Array<Float64> = [];
    cursor :: Array<Int64> = [];
    through i :: 0..n-1 -> loop {
        cursor.push(offset[i]);
        # Pre-fill so we can assign by index rather than push.
        through _j :: 0..degree[i]-1 -> loop {
            adj.push(0);
            weight.push(0.0);
        };
    };

    through k :: 0..input_m-1 -> loop {
        a :: Int64 = from[k];
        b :: Int64 = to[k];
        wv :: Float64 = 1.0;
        if k < w.size() { wv = w[k]; }

        ca :: Int64 = cursor[a];
        adj[ca] = b;
        weight[ca] = wv;
        cursor[a] = ca + 1;

        if !directed {
            cb :: Int64 = cursor[b];
            adj[cb] = a;
            weight[cb] = wv;
            cursor[b] = cb + 1;
        }
    };

    return vchem.Graph(n, effective_m, adj, weight, offset);
}

# Convenience constructor for an unweighted graph. Every edge has
# weight 1.0.
fn :: vchem build_unweighted(
    n        :: Int64,
    from     :: Array<Int64>,
    to       :: Array<Int64>,
    directed :: Bool
) -> vchem.Graph {
    w :: Array<Float64> = [];
    through _k :: 0..from.size()-1 -> loop { w.push(1.0); };
    return vchem.build_graph(n, from, to, w, directed);
}

# Out-degree of node i.
fn :: vchem degree(g :: vchem.Graph, i :: Int64) -> Int64 {
    return g.offset[i + 1] - g.offset[i];
}
