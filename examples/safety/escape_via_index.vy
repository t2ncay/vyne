# SAFETY: escape by storing a region-local array into an outer
# array's slot.
# Expected: compile error, VNE-070.

fn test() {
    outer_arr :: Array := [0.0, 0.0, 0.0];
    region r {
        inner :: Array := [1.0, 2.0, 3.0];
        outer_arr[0] = inner;
    }
}

test();
