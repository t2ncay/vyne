use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib open(width :: Int64, height :: Int64, title :: String, fps :: Int64) -> Bool {
    vraylib.init_window(width, height, title);
    vraylib.set_target_fps(fps);
    return vraylib.is_window_ready();
}

fn :: vraylib rgba(r :: Int64, g :: Int64, b :: Int64, a :: Int64) -> vraylib.Color {
    return vraylib.Color(r, g, b, a);
}

fn :: vraylib vec2(x :: Float64, y :: Float64) -> vraylib.Vector2 {
    return vraylib.Vector2(x, y);
}

fn :: vraylib vec3(x :: Float64, y :: Float64, z :: Float64) -> vraylib.Vector3 {
    return vraylib.Vector3(x, y, z);
}
