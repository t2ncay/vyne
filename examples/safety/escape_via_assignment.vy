# SAFETY: direct escape via assignment of a region-local array
# into a shallower-declared variable.
# Expected: compile error, VNE-070.

outer :: Array := [];

region r {
    inner :: Array := [1.0, 2.0, 3.0];
    outer = inner;
};
