# vgraph/DFS.vy — depth-first search, connected components, and
# topological sort.
#
# Iterative DFS throughout. The recursive form is easier to read but
# hits C's stack limit on deep molecular graphs (a 5000-atom protein
# can have a 2000-deep path), and the interpreter has no tail-call
# elimination.

ruleset { dynamic_casting };

module vchem;

# DFS visit order. `order[k]` is the k-th node visited. Each node
# appears exactly once, but the ordering depends on adjacency order —
# for a graph with cycles, there is no canonical DFS order.
fn :: vchem dfs_order(g :: vchem.Graph, src :: Int64) -> Array<Int64> {
    order :: Array<Int64> = [];
    seen :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop { seen.push(0); };

    # Stack of (node, next-edge-index) pairs, encoded as two parallel
    # arrays. Pushing node alone loses the resume position, which
    # makes the traversal recursive in disguise.
    stack_node :: Array<Int64> = [];
    stack_edge :: Array<Int64> = [];

    stack_node.push(src);
    stack_edge.push(g.offset[src]);
    seen[src] = 1;
    order.push(src);

    while stack_node.size() > 0 {
        top :: Int64 = stack_node.size() - 1;
        u :: Int64 = stack_node[top];
        ei :: Int64 = stack_edge[top];

        if ei < g.offset[u + 1] {
            stack_edge[top] = ei + 1;
            v :: Int64 = g.adj[ei];
            if seen[v] == 0 {
                seen[v] = 1;
                order.push(v);
                stack_node.push(v);
                stack_edge.push(g.offset[v]);
            }
        } else {
            # Exhausted this node's adjacency; pop it.
            stack_node.pop();
            stack_edge.pop();
        }
    };

    return order;
}

# Label each node with a component id. Component ids are 0-based and
# assigned in node order. Isolated nodes each get their own component.
fn :: vchem connected_components(g :: vchem.Graph) -> Array<Int64> {
    comp :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop { comp.push(-1); };

    next_id :: Int64 = 0;

    through s :: 0..g.n-1 -> loop {
        if comp[s] >= 0 { continue; }

        stack_node :: Array<Int64> = [];
        stack_edge :: Array<Int64> = [];
        stack_node.push(s);
        stack_edge.push(g.offset[s]);
        comp[s] = next_id;

        while stack_node.size() > 0 {
            top :: Int64 = stack_node.size() - 1;
            u :: Int64 = stack_node[top];
            ei :: Int64 = stack_edge[top];

            if ei < g.offset[u + 1] {
                stack_edge[top] = ei + 1;
                v :: Int64 = g.adj[ei];
                if comp[v] < 0 {
                    comp[v] = next_id;
                    stack_node.push(v);
                    stack_edge.push(g.offset[v]);
                }
            } else {
                stack_node.pop();
                stack_edge.pop();
            }
        };

        next_id = next_id + 1;
    };

    return comp;
}

# Number of connected components.
fn :: vchem component_count(g :: vchem.Graph) -> Int64 {
    comp :: Array<Int64> = vchem.connected_components(g);
    max_id :: Int64 = -1;
    through i :: 0..comp.size()-1 -> loop {
        if comp[i] > max_id { max_id = comp[i]; }
    };
    return max_id + 1;
}

# Topological sort via Kahn's algorithm. Returns an ordering where
# every edge u -> v has u appearing before v. Empty array if the
# graph contains a cycle. Undirected graphs are always cyclic (every
# edge is a 2-cycle), so this is only meaningful for directed input.
fn :: vchem topological_sort(g :: vchem.Graph) -> Array<Int64> {
    indeg :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop { indeg.push(0); };

    through u :: 0..g.n-1 -> loop {
        lo :: Int64 = g.offset[u];
        hi :: Int64 = g.offset[u + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            indeg[v] = indeg[v] + 1;
        };
    };

    order :: Array<Int64> = [];
    queue :: Array<Int64> = [];
    through i :: 0..g.n-1 -> loop {
        if indeg[i] == 0 { queue.push(i); }
    };

    head :: Int64 = 0;
    while head < queue.size() {
        u :: Int64 = queue[head];
        head = head + 1;
        order.push(u);

        lo :: Int64 = g.offset[u];
        hi :: Int64 = g.offset[u + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            indeg[v] = indeg[v] - 1;
            if indeg[v] == 0 { queue.push(v); }
        };
    };

    # Any node not emitted is part of a cycle.
    if order.size() != g.n {
        return [];
    }
    return order;
}
