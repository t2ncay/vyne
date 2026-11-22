use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(240, 120, "Font regression", 0, vraylib.FLAG_WINDOW_HIDDEN);
path :: String := vraylib.environment("VYNE_RUNTIME_FONT");
font :: vraylib.Font := app.load_font_size(path, 24);
alias :: vraylib.Font := app.load_font_size(path, 24);
larger :: vraylib.Font := app.load_font_size(path, 32);
check(font.token == alias.token && font.token != larger.token, "cache keys include font size");
through cp :: [199, 231, 286, 287, 304, 305, 350, 351, 399, 601] -> loop {
    glyph :: Int64 := vraylib.get_glyph_info(font.token, cp);
    check(vraylib.field(glyph, "value", 0) == cp, "Azerbaijani/Turkish glyph missing");
};
size :: vraylib.Vector2 := font.measure("Azərbaycan Türkçe İı Əə Şş Ğğ", 24.0);
check(size.x > 0.0 && size.y == 24.0, "Unicode text measurement");
multiline :: vraylib.Vector2 := font.measure("Hello\nWorld", 24.0);
check(multiline.y > size.y, "multiline measurement");
camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 2.0, 5.0), vraylib.vec3(0.0, 0.0, 0.0), 60.0);
camera.set_orthographic(10.0);
view :: vraylib.Camera3D := camera.view();
check(view.projection == vraylib.CAMERA_ORTHOGRAPHIC && view.fovy == 10.0 && view.position.z == 5.0, "orthographic setter preserves view");
failed :: Bool := false;
try { camera.set_orthographic(0.0); } catch (error) { failed = true; }
check(failed && camera.view().fovy == 10.0, "invalid orthographic size leaves camera intact");
failed = false;
try { app.load_font_size(path, 0); } catch (error) { failed = true; }
check(failed && app.resources() == 3, "invalid font size leaves resource registry intact");
failed = false;
try { app.load_font_size(path + ".missing", 24); } catch (error) { failed = true; }
check(failed && app.resources() == 3, "missing font leaves resource registry intact");
app.frame(); app.begin();
font.draw("Azərbaycan Türkçe", vraylib.vec2(5.0, 5.0), 24.0, vraylib.WHITE);
app.end(); app.end_frame();
font.close();
check(!alias.valid(), "font aliases invalidate");
failed = false;
try { alias.measure("closed", 24.0); } catch (error) { failed = true; }
check(failed, "closed font measurement rejects stale token");
replacement :: vraylib.Font := app.load_font_size(path, 24);
check(replacement.token != font.token, "closed cached font reloads");
app.close();
check(vraylib.live_handles() == 0, "font session cleanup");
out("VRAYLIB_FONT_OK");
