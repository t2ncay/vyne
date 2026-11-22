use external "vraylib/vraylib.vy";
use native vmem;
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
fn zero_failure() -> Bool {
    @zeroed region zero_outer {
        @deterministic region deterministic_inner { throw "zero failure"; };
    };
    return false;
}
fn allocator_failure() -> Bool {
    @bounded<16384> region abandoned_bound {
        @pool<Float64, 4> region abandoned_pool {
            a :: Array<Float64> := pool_alloc();
            @slab<Float64, 16, 2> region abandoned_slab {
                b :: Array<Float64> := slab_alloc(8);
                @ring<Float64, 2, 4> region abandoned_ring {
                    c :: Array<Float64> := ring_alloc(4);
                    @zeroed region abandoned_zero { throw "allocator failure"; };
                };
            };
        };
    };
    return false;
}
fn exercise_state() -> Bool {
    out("EXPECT_ZERO_MODE_0");
    @zeroed region enclosing_zero {
        out("EXPECT_ZERO_MODE_1");
        try { zero_failure(); } catch (error) {
            check(error == "zero failure", "nested zero exception");
            out("EXPECT_ZERO_MODE_1");
        } finally { out("EXPECT_ZERO_MODE_1"); }
        @deterministic region enclosing_deterministic {
            out("EXPECT_ZERO_MODE_2");
            try { zero_failure(); } catch (error) { out("EXPECT_ZERO_MODE_2"); }
        };
        out("EXPECT_ZERO_MODE_1");
    };
    out("EXPECT_ZERO_MODE_0");
    through i :: 0..127 -> loop {
        try { zero_failure(); } catch (error) { out("EXPECT_ZERO_MODE_0"); }
        finally { out("EXPECT_ZERO_MODE_0"); }
    };
    try {
        try { throw "before finally"; } finally {
            @deterministic region throwing_finally { zero_failure(); };
        }
    } catch (error) {
        check(error == "zero failure", "finally throw reaches outer handler");
        out("EXPECT_ZERO_MODE_0");
    }
    @bounded<131072> region enclosing_bound {
        depth :: Int64 := vmem.checkpoint_depth();
        before :: Int64 := vmem.total_allocated();
        try { allocator_failure(); } catch (error) {
            check(error == "allocator failure", "special allocator exception");
            check(vmem.checkpoint_depth() == depth, "special allocator checkpoints restored");
            check(vmem.total_allocated() > before, "abandoned allocations remain charged to enclosing bound");
            out("EXPECT_ZERO_MODE_0");
        }
        @pool<Float64, 4> region fresh_pool {
            a :: Array<Float64> := pool_alloc(); a[0] = 7.0;
            check(a[0] == 7.0, "fresh pool after throw"); pool_free(a);
        };
        @slab<Float64, 16, 2> region fresh_slab {
            a :: Array<Float64> := slab_alloc(8); a[0] = 8.0;
            check(a[0] == 8.0, "fresh slab after throw"); slab_free(a);
        };
        @ring<Float64, 2, 4> region fresh_ring {
            a :: Array<Float64> := ring_alloc(4); a[0] = 9.0;
            check(a[0] == 9.0, "fresh ring after throw");
        };
    };
    check(vmem.checkpoint_depth() == 0, "all policy checkpoints restored");
    out("EXPECT_ZERO_MODE_0");
    return true;
}
exercise_state();
app :: vraylib.App := vraylib.session("");
app.frame(); app.transient_errors(true);
exercise_state();
app.transient_errors(false); app.end_frame(); app.close();
out("EXPECT_ZERO_MODE_0");
out("REGION_EXCEPTION_STATE_OK");
