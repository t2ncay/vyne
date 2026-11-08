# region_persistent.vy — @persistent commit-arena region policy.
#
# Run compiled (regions are a compiled-backend feature):
#   ./vynec --compile examples/test/region_persistent.vy
#   ./examples/test/region_persistent
#
# Verifies:
#   - allocations inside @persistent land in the commit arena
#   - assigning a persistent value to an outer variable is not an escape
#   - region.commit() inside @persistent is a harmless no-op
#   - nested @persistent regions both keep their values
#   - a later ordinary region rewind does not free persistent values
#   - the commit arena reports the surviving bytes

use native vmem;

out("=== @persistent region test ===");

commit_before :: Int64 := vmem.commit_total_allocated();

w :: Array<Float64> := [];
m :: Array<Float64> := [];

@persistent region weights {
    w = [1.0, 2.0, 3.0];
    w[0] = 9.0;
    out(w[0] == 9.0);
    out(vmem.commit_total_allocated() > commit_before);

    @persistent region opt_state {
        m = [4.0, 5.0];
        m[0] = 1.5;
        out(m[0] == 1.5);
    };

    # Redundant inside @persistent, but must not break anything.
    region.commit(w);
};

out(w[0] == 9.0);                             # survives the closing brace
out(m[0] == 1.5);
out(vmem.commit_total_allocated() > commit_before);

# A normal region rewind must not free commit-arena values.
region temp_scope {
    temp :: Array<Float64> := [7.0];
    out(temp[0] == 7.0);
};

out(w[0] == 9.0);
out(m[0] == 1.5);
out("=== done ===");
