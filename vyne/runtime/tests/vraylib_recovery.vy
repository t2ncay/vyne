use external "vraylib/vraylib.vy";
use native vmem;
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
fn nested_failure() -> Bool {
    region inner { region deeper { throw "nested region failure"; }; };
    return false;
}
app :: vraylib.App := vraylib.session("");
check(app.frame(), "headless frame");
app.transient_errors(true);
committed :: Int64 := vmem.commit_total_allocated();
region enclosing {
    depth :: Int64 := vmem.checkpoint_depth();
    through i :: 0..255 -> loop {
        try { nested_failure(); }
        catch (error) {
            check(error == "nested region failure", "exception payload");
            check(vmem.checkpoint_depth() == depth, "called nested regions abandoned checkpoints");
        }
    };
    try {
        try {
            region outer { throw "try failure"; };
        } catch (error) {
            region catch_scope { throw "catch failure"; };
        } finally {
            check(vmem.checkpoint_depth() == depth, "catch throw abandoned checkpoint before finally");
        }
    } catch (error) {
        check(error == "catch failure", "catch exception reaches enclosing handler");
    }
};
check(vmem.commit_total_allocated() == committed, "short recovery errors grow commit arena");
@persistent region persistent_enclosing {
    depth :: Int64 := vmem.checkpoint_depth();
    try {
        @persistent region nested_persistent { throw "persistent failure"; };
    } catch (error) {
        check(error == "persistent failure" && vmem.checkpoint_depth() == depth, "persistent nesting restored");
    }
};
try {
    @persistent region escaped_persistent { throw "escaped persistent failure"; };
} catch (error) {
    check(error == "escaped persistent failure" && vmem.checkpoint_depth() == 0, "persistent exit restores main arena");
}
app.transient_errors(false);
check(app.end_frame(), "explicit frame completion");
app.close();
check(vraylib.live_handles() == 0, "headless recovery cleanup");
region ordinary_enclosing {
    depth :: Int64 := vmem.checkpoint_depth();
    through i :: 0..127 -> loop {
        try { nested_failure(); } catch (error) {
            check(error == "nested region failure" && vmem.checkpoint_depth() == depth, "ordinary exception storage survives checkpoint restoration");
        }
    };
};
try {
    @persistent region ordinary_persistent { throw ["ordinary persistent payload"]; };
} catch (error) {
    check(error[0] == "ordinary persistent payload" && vmem.checkpoint_depth() == 0, "ordinary persistent exception storage survives allocator restoration");
}
out("VRAYLIB_RECOVERY_OK");
