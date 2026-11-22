use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
expected_raw :: Int64 := 0;
cleanups :: Int64 := 0;
fn native_failure() -> Bool { vraylib.buffer_read(0, 0); return false; }
fn raw_cleanup() -> Bool {
    count :: Int64 := vraylib.buffer("int", 1);
    raw :: Int64 := 0;
    try {
        raw = vraylib.load_codepoints("Azərbaycan", count); expected_raw = raw;
        native_failure();
    } finally {
        check(raw == expected_raw && raw != 0 && vraylib.valid(raw), "finally lost allocated raw handle");
        vraylib.unload_codepoints(raw); vraylib.release(count); cleanups = cleanups + 1;
    }
    return false;
}
fn catch_cleanup() -> Bool {
    raw :: Int64 := 0;
    try { throw "enter catch"; } catch (error) {
        raw = vraylib.buffer("int", 4); expected_raw = raw; native_failure();
    } finally {
        check(raw == expected_raw && raw != 0 && vraylib.valid(raw), "finally lost catch allocation");
        vraylib.release(raw); cleanups = cleanups + 1;
    }
    return false;
}
fn partial_startup(stage :: Int64) -> Bool {
    app :: vraylib.App := vraylib.App(0);
    image :: vraylib.Image := vraylib.Image(0, 0);
    try {
        if stage == 0 { native_failure(); }
        app = vraylib.session("");
        if stage == 1 { native_failure(); }
        image = app.image(2, 2, vraylib.RED);
        native_failure();
    } finally {
        if image.valid() { image.close(); }
        if app.valid() { app.close(); }
        cleanups = cleanups + 1;
    }
    return false;
}
fn boxed_startup() -> Bool {
    owner := null;
    try { owner = vraylib.session(""); native_failure(); }
    finally { if owner != null { closed :: vraylib.App := owner; closed.close(); } cleanups = cleanups + 1; }
    return false;
}
through i :: 0..63 -> loop {
    before :: Int64 := cleanups;
    try { raw_cleanup(); } catch (error) { }
    check(cleanups == before + 1 && vraylib.live_handles() == 0, "raw allocation leaked after native fault");
    try { catch_cleanup(); } catch (error) { }
    check(cleanups == before + 2 && vraylib.live_handles() == 0, "catch allocation leaked after native fault");
};
through stage :: 0..2 -> loop {
    before :: Int64 := cleanups;
    try { partial_startup(stage); } catch (error) { }
    check(cleanups == before + 1 && vraylib.live_handles() == 0, "partial startup ownership leaked");
};
before :: Int64 := cleanups;
try { boxed_startup(); } catch (error) { }
check(cleanups == before + 1 && vraylib.live_handles() == 0, "boxed startup ownership leaked");
out("EXCEPTION_OWNERSHIP_OK");
