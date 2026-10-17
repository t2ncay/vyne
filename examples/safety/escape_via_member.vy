# SAFETY: escape by storing a region-local array into an outer
# struct's field.
# Expected: compile error, VNE-070.

interface Box {
    val :: Array,
}

fn test() {
    outer_box :: Box := Box([]);
    region r {
        inner :: Array := [1.0, 2.0, 3.0];
        outer_box.val = inner;
    }
}

test();
