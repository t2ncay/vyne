# vgraph/BFS.vy — breadth-first search and unweighted shortest paths.

ruleset { dynamic_casting };

module vchem;

# Standard BFS from a single source. Returns a vector of hop counts:
# `dist[i]` is the number of edges on the shortest path from `src` to
# `i`, or -1 if `i` is unreachable.
#
# O(V + E). No priority queue; BFS's FIFO order is what makes the
# unit-weight case exact.
fn :: vchem bfs_distances(g :: vchem.Graph, src :: Int64) -> Array<Int64> {
    dist :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop { dist.push(-1); };

    # Queue as a growable Int64 array. Head pointer walks forward;
    # there is no dequeue, so we never shift elements. The tail is
    # just dist.size() of the queue array.
    queue :: Array<Int64> = [];
    queue.push(src);
    dist[src] = 0;

    head :: Int64 = 0;
    while head < queue.size() {
        u :: Int64 = queue[head];
        head = head + 1;

        lo :: Int64 = g.offset[u];
        hi :: Int64 = g.offset[u + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            if dist[v] < 0 {
                dist[v] = dist[u] + 1;
                queue.push(v);
            }
        };
    };

    return dist;
}

# Visit order from BFS. Returns nodes in the order they were first
# reached. `src` is at index 0.
fn :: vchem bfs_order(g :: vchem.Graph, src :: Int64) -> Array<Int64> {
    order :: Array<Int64> = [];
    seen :: Array<Int64> = [];
    through _i :: 0..g.n-1 -> loop { seen.push(0); };

    queue :: Array<Int64> = [];
    queue.push(src);
    seen[src] = 1;
    order.push(src);

    head :: Int64 = 0;
    while head < queue.size() {
        u :: Int64 = queue[head];
        head = head + 1;

        lo :: Int64 = g.offset[u];
        hi :: Int64 = g.offset[u + 1];
        through e :: lo..hi-1 -> loop {
            v :: Int64 = g.adj[e];
            if seen[v] == 0 {
                seen[v] = 1;
                order.push(v);
                queue.push(v);
            }
        };
    };

    return order;
}
