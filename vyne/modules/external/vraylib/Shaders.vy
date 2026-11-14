use native vraylib;
ruleset { dynamic_casting };
module vraylib;

# Reuse one native float buffer for uniforms: no buffer allocation per frame.
fn :: vraylib uniform_float(shader :: Int64, location :: Int64, storage :: Int64, value :: Float64) -> Bool {
    if location < 0 { return false; }
    vraylib.buffer_write(storage, 0, value);
    vraylib.set_shader_value(shader, location, storage, vraylib.SHADER_UNIFORM_FLOAT);
    return true;
}

fn :: vraylib uniform_vec2(shader :: Int64, location :: Int64, storage :: Int64, x :: Float64, y :: Float64) -> Bool {
    if location < 0 { return false; }
    vraylib.buffer_write(storage, 0, x);
    vraylib.buffer_write(storage, 1, y);
    vraylib.set_shader_value(shader, location, storage, vraylib.SHADER_UNIFORM_VEC2);
    return true;
}

fn :: vraylib uniform_vec3(shader :: Int64, location :: Int64, storage :: Int64, x :: Float64, y :: Float64, z :: Float64) -> Bool {
    if location < 0 { return false; }
    vraylib.buffer_write(storage, 0, x);
    vraylib.buffer_write(storage, 1, y);
    vraylib.buffer_write(storage, 2, z);
    vraylib.set_shader_value(shader, location, storage, vraylib.SHADER_UNIFORM_VEC3);
    return true;
}
