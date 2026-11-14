# Pure, deterministic story state. All fields are primitive values so state can
# safely cross managed frame boundaries without retaining frame-arena storage.
use native vmath;
ruleset { dynamic_casting };
module SectorShift;

interface State {
    time :: Float64,
    event_active :: Bool,
    event_start :: Float64,
    freeze :: Bool,
    event_count :: Int64,
    stalker_z :: Float64,
    glitch :: Float64,
    game_event :: Int64,
    message_id :: Int64,
    radio_played :: Bool,
    radio_now :: Bool,
    fog_density :: Float64,
    final_triggered :: Bool,
    fade :: Float64,
}

fn :: SectorShift initial() -> SectorShift.State {
    return SectorShift.State(0.0, false, 0.0, false, 0, 120.0, 0.0, 0, 0, false, false, 0.05, false, 0.0);
}

fn :: SectorShift step(state :: SectorShift.State, z :: Float64, dt :: Float64) -> SectorShift.State {
    state.time = state.time + vmath.max(0.0, dt);
    state.radio_now = false;
    if state.event_active && state.time - state.event_start >= 3.0 { state.event_active = false; }
    distance :: Float64 := vmath.abs(z - state.stalker_z);
    state.glitch = vmath.clamp(1.0 - distance / 70.0, 0.0, 1.0) * 0.25;
    if distance < 5.0 && !state.event_active && !state.final_triggered {
        state.event_active = true;
        state.event_start = state.time;
        state.event_count = state.event_count + 1;
        state.stalker_z = state.stalker_z + 180.0;
    }
    state.freeze = state.event_active;
    if state.event_active {
        state.glitch = vmath.max(state.glitch, 0.8 * vmath.clamp(1.0 - (state.time - state.event_start) / 3.0, 0.0, 1.0));
    }
    if z > 50.0 && state.game_event == 0 {
        state.game_event = 1;
        state.message_id = 1;
    }
    if z > 100.0 && !state.radio_played {
        state.radio_played = true;
        state.radio_now = true;
        state.message_id = 2;
    }
    if z > 150.0 && state.game_event == 1 {
        state.game_event = 2;
        state.message_id = 3;
    }
    if state.game_event == 2 { state.fog_density = 0.28 + vmath.sin(state.time * 5.0) * 0.1; }
    if z > 350.0 { state.final_triggered = true; }
    if state.final_triggered { state.fade = vmath.clamp(state.fade + 0.3 * vmath.max(0.0, dt), 0.0, 1.0); }
    return state;
}

fn :: SectorShift message(state :: SectorShift.State) -> String {
    if state.message_id == 1 { return "WARNING: HIGH FOG CONCENTRATION"; }
    if state.message_id == 2 { return "INCOMING RADIO SIGNAL..."; }
    if state.message_id == 3 { return "SYSTEM ERROR: BODYCAM MALFUNCTION"; }
    return "OBJECTIVE: PATROL THE CORRIDOR";
}
