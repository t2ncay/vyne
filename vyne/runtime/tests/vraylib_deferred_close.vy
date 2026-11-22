use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.session("");
check(!app.close_requested(), "no initial close event");
check(app.defer_close(true), "enable deferred close");
check(app.frame() && !app.close_requested(), "headless deferred frame");
app.stop();
check(!app.frame(), "stop remains definitive");
app.close();
bounded :: vraylib.App := vraylib.session("");
bounded.defer_close(true); bounded.limit(2);
check(bounded.frame() && !bounded.close_requested(), "first bounded frame");
check(bounded.frame() && !bounded.close_requested(), "second bounded frame");
check(!bounded.frame() && bounded.frames() == 2, "limit remains definitive");
bounded.defer_close(false); bounded.close();
check(vraylib.live_handles() == 0, "deferred close session cleanup");
out("VRAYLIB_DEFERRED_CLOSE_VY_OK");
