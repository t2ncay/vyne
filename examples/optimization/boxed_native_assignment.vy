# C17: boxed-to-native reassignment currently shadows the outer C variable.
# Pending reproducer; not passing V03 acceptance. The expected size is 3.
fn probe() {
    outer :: Array := [];
    region temporary {
        inner :: Array := [1.0, 2.0, 3.0];
        temporary.commit(inner);
        outer = inner;
    };
    out("C17 outer size: " + string(outer.size()));
    if outer.size() != 3 { exit(1); }
}
probe();
