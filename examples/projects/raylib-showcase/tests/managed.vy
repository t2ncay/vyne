use external "vraylib/vraylib.vy";
use native vmem;
ruleset { dynamic_casting };

fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.session("build/vraylib-tests");
image :: vraylib.Image := app.image(4, 4, vraylib.RED);
alias :: vraylib.Image := image;
check(image.width() == 4, "typed image method");
check(image.save("build/vraylib-tests/managed.png"), "generated image fixture");
app.limit(50);
baseline :: Int64 := vmem.total_allocated();
while app.frame() {
    cached :: vraylib.Image := app.load_image("managed.png");
    check(cached.width() == 4, "cached frame-time asset");
    temporary := "frame memory " + string(app.frames());
}
check(app.resources() == 2, "one cached image across frames");
check(vmem.total_allocated() < baseline + 4096, "frame arena is rewound");
check(app.frames() == 50, "bounded frame loop");
check(image.close(), "owned resource close");
check(!alias.valid(), "copied token shares invalidation");
check(!alias.close(), "idempotent copied close");
app.close();
check(!app.valid(), "session closed");
check(vraylib.live_handles() == 0, "session closes all native allocations");

fn fail_update(owner :: vraylib.App) -> Bool { throw "callback error " + string(owner.frames()); }
fn never_draw(owner :: vraylib.App) -> Bool { throw "draw should not be reached"; }
owner :: vraylib.App := vraylib.session("");
failed :: Bool := false;
try { owner.run(fail_update, never_draw); } catch (error) {
    check(error == "callback error 0", "exception survives frame rewind and session cleanup");
    failed = true;
}
check(failed && !owner.valid(), "callback failure closes session");
check(vraylib.live_handles() == 0, "failed callback cleans resources");

interface Failure { code :: Int64, message :: String, }
fn structured_error(owner :: vraylib.App) -> Bool {
    throw {"failure": Failure(42, "structured " + string(owner.frames()))};
}
structured :: vraylib.App := vraylib.session("");
try { structured.run(structured_error, never_draw); } catch (error) {
    check(error["failure"].code == 42, "structured exception scalar survives cleanup");
    check(error["failure"].message == "structured 0", "nested exception string survives cleanup");
}
check(!structured.valid() && vraylib.live_handles() == 0, "structured callback cleanup");
fn cyclic_error(owner :: vraylib.App) -> Bool {
    error := {"code": 7};
    error["self"] = error;
    throw error;
}
cyclic :: vraylib.App := vraylib.session("");
try { cyclic.run(cyclic_error, never_draw); } catch (error) {
    check(error["self"]["code"] == 7, "cyclic error graph preserves values after cleanup");
}
check(!cyclic.valid(), "cyclic exception closes session");

# A stale App.close must not rewind another application's active frame.
fresh :: vraylib.App := vraylib.session("");
fresh.limit(1);
check(fresh.frame(), "new frame starts");
text := "live frame " + string(123);
owner.close();
check(text == "live frame 123", "stale app close leaves current frame alive");
fresh.end_frame(); fresh.close();
updates :: Int64 := 0;
draws :: Int64 := 0;
fn cpu_update(owner :: vraylib.App) -> Bool { updates = updates + 1; return true; }
fn cpu_draw(owner :: vraylib.App) -> Bool { draws = draws + 1; return true; }
batch :: vraylib.App := vraylib.session("");
batch.limit(3);
check(batch.run(cpu_update, cpu_draw) == 3 && updates == 3 && draws == 3, "headless callbacks execute once per frame");
out("VRAYLIB_MANAGED_OK");
