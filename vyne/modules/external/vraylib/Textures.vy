use "Types.vy";
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib present_target(target :: Int64, width :: Float64, height :: Float64) -> Bool {
    texture :: Int64 := vraylib.render_texture(target);
    tw :: Int64 := vraylib.field(texture, "width", 0);
    th :: Int64 := vraylib.field(texture, "height", 0);
    vraylib.draw_texture_pro(texture, vraylib.Rectangle(0.0, 0.0, float64(tw), -float64(th)), vraylib.Rectangle(0.0, 0.0, width, height), vraylib.Vector2(0.0, 0.0), 0.0, vraylib.WHITE);
    # This is a borrowed attachment; releasing its handle must not unload GPU storage.
    return true;
}
