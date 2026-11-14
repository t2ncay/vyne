# rlgl operations use vraylib.gl_<snake_case>; constants retain RL_* names.
# Flush before mixing raw render state with raylib's batched draws.
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib flush() -> Bool {
    vraylib.gl_draw_render_batch_active();
    return true;
}
