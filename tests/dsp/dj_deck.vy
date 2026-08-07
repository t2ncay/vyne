ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";
use lib "vcolors.vy";

vglib.init(1400, 900, 60, "VYNE PRO DJ CONSOLE v2.1", 0);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

scale    :: Float64 = 1.0;
offset_x :: Float64 = 0.0;
offset_y :: Float64 = 0.0;

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# Load Deck Audio Tracks
deck_a_track = vaudio.play_stream(configs.Audios.vanished);
deck_b_track = vaudio.play_stream(configs.Audios.when_the_sun_hits);

wave_peaks_a :: Array = vaudio.get_waveform(deck_a_track, 150);
wave_peaks_b :: Array = vaudio.get_waveform(deck_b_track, 150);

# --- ATTACH DSP ENGINE CHAINS ---
vaudio.attach_bpm(deck_a_track);
vaudio.attach_bpm(deck_b_track);

vaudio.attach_eq(deck_a_track);
vaudio.attach_eq(deck_b_track);

vaudio.attach_compressor(deck_a_track);
vaudio.attach_compressor(deck_b_track);

vaudio.attach_saturator(deck_a_track);
vaudio.attach_saturator(deck_b_track);

vaudio.attach_reverb(deck_a_track);
vaudio.attach_reverb(deck_b_track);

vaudio.enable_eq(2);

# --- INITIALIZE GLOBAL STATE VARIABLES ---
run_time              :: Float64 = 0.0;
prev_mouse_state      :: Int64   = 0;
smooth_lufs_intensity :: Float64 = 0.0;
scope_phase_a         :: Float64 = 0.0;
scope_phase_b         :: Float64 = 0.0;
peak_hold_rms         :: Float64 = 0.0;

# --- GLOBAL DJ MIXER STATE ---
crossfader_pos :: Float64 = 0.5;   # 0.0 = Deck A Only, 1.0 = Deck B Only
master_volume  :: Float64 = 1.0;
active_knob    :: Int64   = 0;     # Active dragged UI element ID
active_deck    :: Int64   = 0;     # 0 = Deck A, 1 = Deck B

# --- SEPARATE FX RACK STATE PER DECK ---
active_fx_unit_a :: Int64   = 0;     # 0 = OFF, 1 = COMP, 2 = SAT, 3 = REV
fx_drive_a       :: Float64 = 0.25;
fx_comp_thresh_a :: Float64 = -12.0;
fx_rev_mix_a     :: Float64 = 0.30;

active_fx_unit_b :: Int64   = 0;     # 0 = OFF, 1 = COMP, 2 = SAT, 3 = REV
fx_drive_b       :: Float64 = 0.25;
fx_comp_thresh_b :: Float64 = -12.0;
fx_rev_mix_b     :: Float64 = 0.30;

# --- DYNAMIC EQ PEAK FREQUENCY TRACKER STATE ---
track_low_freq :: Float64 = 100.0;
track_mid_freq :: Float64 = 1000.0;
track_hi_freq  :: Float64 = 8000.0;

# --- DECK A STATE ---
deck_a_play    :: Int64   = 0;
deck_a_bpm     :: Float64 = 130.0;
deck_a_pitch   :: Float64 = 0.0;
deck_a_pos     :: Float64 = 0.0;
deck_a_gain    :: Float64 = 1.0;
deck_a_eq_hi   :: Float64 = 0.0;
deck_a_eq_mid  :: Float64 = 0.0;
deck_a_eq_low  :: Float64 = 0.0;
deck_a_filter  :: Float64 = 0.0;
deck_a_vol     :: Float64 = 0.85;

# --- DECK B STATE ---
deck_b_play    :: Int64   = 0;
deck_b_bpm     :: Float64 = 128.0;
deck_b_pitch   :: Float64 = 0.0;
deck_b_pos     :: Float64 = 0.0;
deck_b_gain    :: Float64 = 1.0;
deck_b_eq_hi   :: Float64 = 0.0;
deck_b_eq_mid  :: Float64 = 0.0;
deck_b_eq_low  :: Float64 = 0.0;
deck_b_filter  :: Float64 = 0.0;
deck_b_vol     :: Float64 = 0.85;

# --- PERFORMANCE ROLL & VINYL BRAKE STATE (WITH SLIP TIMERS) ---
roll_a_active   :: Int64   = 0;     # 0=OFF, 1=1/4, 2=1/8, 3=1/16
roll_a_start_s  :: Float64 = 0.0;
roll_a_slip_s   :: Float64 = 0.0;   # Phantom timeline position for Slip Mode
brake_a_active  :: Int64   = 0;
brake_a_speed   :: Float64 = 1.0;

roll_b_active   :: Int64   = 0;     # 0=OFF, 1=1/4, 2=1/8, 3=1/16
roll_b_start_s  :: Float64 = 0.0;
roll_b_slip_s   :: Float64 = 0.0;   # Phantom timeline position for Slip Mode
brake_b_active  :: Int64   = 0;
brake_b_speed   :: Float64 = 1.0;

# --- COLOR PALETTE DEFINITIONS ---
COLOR_BG       = vglib.rgba(10, 11, 15, 255);
COLOR_CARD     = vglib.rgba(18, 22, 30, 255);
COLOR_CARD_RIM = vglib.rgba(40, 48, 64, 255);
COLOR_DECK_A   = vglib.rgba(0, 240, 255, 255);
COLOR_DECK_B   = vglib.rgba(255, 45, 120, 255);
COLOR_AMBER    = vglib.rgba(255, 170, 0, 255);

cues_a :: Array = [0.00, 0.25, 0.50, 0.75];
cues_b :: Array = [0.00, 0.25, 0.50, 0.75];

# --- UI HELPER: ROTARY KNOB ---
fn draw_dj_knob(name, x, y, val_norm, display_val, color, is_hovered, is_active) {
    radius_base = 32.0;
    if (is_active) { radius_base = 34.0; }

    vglib.circle(x, y, radius_base, vglib.BLACK);
    
    if (is_active) {
        vglib.circle(x, y, radius_base - 1.5, color);
    } else {
        vglib.circle(x, y, radius_base - 2.0, vglib.rgba(35, 40, 52, 255));
    }

    vglib.circle(x, y, 24.0, vglib.rgba(16, 18, 24, 255));
    vglib.circle(x, y, 15.0, vglib.rgba(26, 30, 38, 255));
    
    arc_steps :: Int64 = int64(val_norm * 40.0);
    if (arc_steps > 0) {
        prev_ax :: Float64 = x + vmath.sin(vmath.radians(-135.0)) * 20.0;
        prev_ay :: Float64 = y - vmath.cos(vmath.radians(-135.0)) * 20.0;

        through a :: 1..40 -> loop {
            if (a <= arc_steps) {
                ang = ((a / 40.0) * 270.0) - 135.0;
                rad = vmath.radians(ang);
                curr_ax :: Float64 = x + vmath.sin(rad) * 20.0;
                curr_ay :: Float64 = y - vmath.cos(rad) * 20.0;

                vglib.line(prev_ax, prev_ay, curr_ax, curr_ay, color);
                prev_ax = curr_ax;
                prev_ay = curr_ay;
            }
        };
    }

    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 21.0;
    line_y :: Float64 = y - vmath.cos(rad) * 21.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 2.5, color);
    
    label_col = is_hovered ? COLOR_DECK_A : vglib.rgba(160, 170, 185, 255);
    vglib.text_ex(vcr_font, name, x - 20, y + 38, 10, label_col);
    vglib.text_ex(vcr_font, display_val, x - 16, y - 4, 9, color);
}

fn draw_gain_slider_bar(label, x, y, width, gain_val, accent_color, is_active) {
    norm_val = gain_val / 2.0;
    
    vglib.rect(x, y, width, 24, vglib.rgba(18, 22, 30, 255));
    vglib.line(x, y, x + width, y, COLOR_CARD_RIM);
    vglib.line(x + width, y, x + width, y + 24, COLOR_CARD_RIM);
    vglib.line(x + width, y + 24, x, y + 24, COLOR_CARD_RIM);
    vglib.line(x, y + 24, x, y, COLOR_CARD_RIM);

    vglib.rect(x + 55, y + 9, 50, 6, vglib.rgba(30, 36, 48, 255));
    track_w :: Float64 = 50.0 * norm_val;
    vglib.rect(x + 55, y + 9, track_w, 6, accent_color);

    thumb_x :: Float64 = x + 55.0 + track_w;
    thumb_col = is_active ? vglib.WHITE : accent_color;
    vglib.rect(thumb_x - 4.0, y + 4, 8, 16, thumb_col);
    vglib.line(thumb_x, y + 6, thumb_x, y + 18, vglib.BLACK);

    vglib.text_ex(vcr_font, label, x + 8, y + 7, 9, vglib.rgba(160, 170, 185, 255));
    val_str = string(vmath.round(gain_val * 100.0)) + "%";
    vglib.text_ex(vcr_font, val_str, x + width - 38, y + 7, 9, accent_color);
}

fn draw_perf_pad(label, sub_label, x, y, w, h, accent_color, is_active, is_hovered) {
    bg_col   = is_active ? accent_color : vglib.rgba(20, 24, 32, 255);
    rim_col  = is_active ? vglib.WHITE : (is_hovered ? accent_color : vglib.rgba(45, 52, 66, 255));
    text_col = is_active ? vglib.BLACK : vglib.WHITE;

    vglib.rect(x, y, w, h, bg_col);
    vglib.line(x, y, x + w, y, rim_col);
    vglib.line(x + w, y, x + w, y + h, rim_col);
    vglib.line(x + w, y + h, x, y + h, rim_col);
    vglib.line(x, y + h, x, y, rim_col);

    vglib.text_ex(vcr_font, label, x + 8, y + 10, 11, text_col);
    sub_col = is_active ? vglib.BLACK : vglib.rgba(140, 150, 165, 255);
    vglib.text_ex(vcr_font, sub_label, x + 8, y + 28, 9, sub_col);
}

# --- MAIN DJ WORKSTATION RENDER LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    vaudio.update_stream(deck_a_track);
    vaudio.update_stream(deck_b_track);

    deck_a_bpm = vaudio.get_bpm(deck_a_track);
    deck_b_bpm = vaudio.get_bpm(deck_b_track);

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    if (deck_a_play == 1) { deck_a_pos = vaudio.get_stream_pos(deck_a_track); }
    if (deck_b_play == 1) { deck_b_pos = vaudio.get_stream_pos(deck_b_track); }

    # Keyboard Controls
    if (vglib.key_pressed(vglib.SPACE)) { deck_b_bpm = deck_a_bpm; }
    if (vglib.key_pressed(vglib.Q)) {
        deck_a_play = (deck_a_play == 1) ? 0 : 1;
        if (deck_a_play == 1) { vaudio.resume_stream(deck_a_track); } 
        else { vaudio.pause_stream(deck_a_track); }
    }
    if (vglib.key_pressed(vglib.P)) {
        deck_b_play = (deck_b_play == 1) ? 0 : 1;
        if (deck_b_play == 1) { vaudio.resume_stream(deck_b_track); } 
        else { vaudio.pause_stream(deck_b_track); }
    }

    if (vglib.key_down(vglib.LEFT))  { crossfader_pos = vmath.clamp(crossfader_pos - 0.01, 0.0, 1.0); }
    if (vglib.key_down(vglib.RIGHT)) { crossfader_pos = vmath.clamp(crossfader_pos + 0.01, 0.0, 1.0); }

    # --- MOUSE INTERACTION DETECTIONS ---
    if (mouse_click && prev_mouse_state == 0) {
        if (m[0] >= 50 && m[0] <= 140 && m[1] >= 20 && m[1] <= 52) {
            deck_a_play = (deck_a_play == 1) ? 0 : 1;
            if (deck_a_play == 1) { vaudio.resume_stream(deck_a_track); }
            else { vaudio.pause_stream(deck_a_track); }
        }
        if (m[0] >= 1260 && m[0] <= 1350 && m[1] >= 20 && m[1] <= 52) {
            deck_b_play = (deck_b_play == 1) ? 0 : 1;
            if (deck_b_play == 1) { vaudio.resume_stream(deck_b_track); }
            else { vaudio.pause_stream(deck_b_track); }
        }
        if (m[0] >= 650 && m[0] <= 750 && m[1] >= 20 && m[1] <= 52) {
            deck_b_bpm = deck_a_bpm; deck_b_pitch = deck_a_pitch;
        }

        # DECK A FX SELECTORS
        if (m[0] >= 605 && m[0] <= 640) {
            if (m[1] >= 505 && m[1] <= 527) { active_fx_unit_a = 0; }
            if (m[1] >= 530 && m[1] <= 552) { active_fx_unit_a = 1; }
            if (m[1] >= 555 && m[1] <= 577) { active_fx_unit_a = 2; }
            if (m[1] >= 580 && m[1] <= 602) { active_fx_unit_a = 3; }
        }
        # DECK B FX SELECTORS
        if (m[0] >= 760 && m[0] <= 795) {
            if (m[1] >= 505 && m[1] <= 527) { active_fx_unit_b = 0; }
            if (m[1] >= 530 && m[1] <= 552) { active_fx_unit_b = 1; }
            if (m[1] >= 555 && m[1] <= 577) { active_fx_unit_b = 2; }
            if (m[1] >= 580 && m[1] <= 602) { active_fx_unit_b = 3; }
        }

        # --- DECK A TOP ROW: PERFORMANCE ROLL & BRAKE PADS ---
        through col :: 0..3 -> loop {
            px :: Int64 = 30 + (col * 140); py :: Int64 = 580;
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                if (roll_a_active == 0) { roll_a_slip_s = deck_a_pos * 180.0; }
                if (col == 0) { roll_a_active = 1; roll_a_start_s = deck_a_pos * 180.0; }
                if (col == 1) { roll_a_active = 2; roll_a_start_s = deck_a_pos * 180.0; }
                if (col == 2) { roll_a_active = 3; roll_a_start_s = deck_a_pos * 180.0; }
                if (col == 3) { brake_a_active = 1; }
            }
        };

        # --- DECK A BOTTOM ROW: CUES 1..4 ---
        through col :: 0..3 -> loop {
            px :: Int64 = 30 + (col * 140); py :: Int64 = 645;
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                deck_a_pos = cues_a[col];
                vaudio.seek_stream(deck_a_track, deck_a_pos * 180.0);
                deck_a_play = 1;
            }
        };

        # --- DECK B TOP ROW: PERFORMANCE ROLL & BRAKE PADS ---
        through col :: 0..3 -> loop {
            px :: Int64 = 820 + (col * 140); py :: Int64 = 580;
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                if (roll_b_active == 0) { roll_b_slip_s = deck_b_pos * 180.0; }
                if (col == 0) { roll_b_active = 1; roll_b_start_s = deck_b_pos * 180.0; }
                if (col == 1) { roll_b_active = 2; roll_b_start_s = deck_b_pos * 180.0; }
                if (col == 2) { roll_b_active = 3; roll_b_start_s = deck_b_pos * 180.0; }
                if (col == 3) { brake_b_active = 1; }
            }
        };

        # --- DECK B BOTTOM ROW: CUES 1..4 ---
        through col :: 0..3 -> loop {
            px :: Int64 = 820 + (col * 140); py :: Int64 = 645;
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                deck_b_pos = cues_b[col];
                vaudio.seek_stream(deck_b_track, deck_b_pos * 180.0);
                deck_b_play = 1;
            }
        };
    }

    # --- MOUSE RELEASE HANDLER (RESTORES SLIP TIMELINE) ---
    if (!mouse_click) {
        if (roll_a_active > 0) {
            vaudio.seek_stream(deck_a_track, roll_a_slip_s);
            roll_a_active = 0;
        }
        if (roll_b_active > 0) {
            vaudio.seek_stream(deck_b_track, roll_b_slip_s);
            roll_b_active = 0;
        }
    }

    # --- BEAT ROLL & VINYL BRAKE REAL-TIME EVALUATION ---
    if (roll_a_active > 0 && deck_a_play == 1) {
        roll_a_slip_s = roll_a_slip_s + 0.016; # Advance background phantom position
        div :: Float64 = (roll_a_active == 1) ? 4.0 : ((roll_a_active == 2) ? 8.0 : 16.0);
        step_s :: Float64 = (60.0 / deck_a_bpm) * (4.0 / div);
        curr_s :: Float64 = deck_a_pos * 180.0;
        if (curr_s - roll_a_start_s >= step_s) {
            vaudio.seek_stream(deck_a_track, roll_a_start_s);
        }
    }

    if (roll_b_active > 0 && deck_b_play == 1) {
        roll_b_slip_s = roll_b_slip_s + 0.016; # Advance background phantom position
        div :: Float64 = (roll_b_active == 1) ? 4.0 : ((roll_b_active == 2) ? 8.0 : 16.0);
        step_s :: Float64 = (60.0 / deck_b_bpm) * (4.0 / div);
        curr_s :: Float64 = deck_b_pos * 180.0;
        if (curr_s - roll_b_start_s >= step_s) {
            vaudio.seek_stream(deck_b_track, roll_b_start_s);
        }
    }

    if (brake_a_active == 1) {
        brake_a_speed = brake_a_speed * 0.90;
        if (brake_a_speed < 0.02) {
            brake_a_speed = 0.0;
            deck_a_play = 0;
            vaudio.pause_stream(deck_a_track);
            brake_a_active = 0;
            brake_a_speed = 1.0;
        }
    }

    if (brake_b_active == 1) {
        brake_b_speed = brake_b_speed * 0.90;
        if (brake_b_speed < 0.02) {
            brake_b_speed = 0.0;
            deck_b_play = 0;
            vaudio.pause_stream(deck_b_track);
            brake_b_active = 0;
            brake_b_speed = 1.0;
        }
    }

    # --- SYNCHRONIZED HITBOX & BOUNDARY DETECTIONS ---
    is_gain_a = (m[0] >= 230 && m[0] <= 380 && m[1] >= 525 && m[1] <= 555);
    is_gain_b = (m[0] >= 1030 && m[0] <= 1180 && m[1] >= 525 && m[1] <= 555);

    ha_pitch = vmath.hypot(m[0] - 100, m[1] - 460) < 32;
    ha_hi    = vmath.hypot(m[0] - 200, m[1] - 460) < 32;
    ha_mid   = vmath.hypot(m[0] - 300, m[1] - 460) < 32;
    ha_low   = vmath.hypot(m[0] - 400, m[1] - 460) < 32;
    ha_flt   = vmath.hypot(m[0] - 500, m[1] - 460) < 32;

    hb_pitch = vmath.hypot(m[0] - 900, m[1] - 460) < 32;
    hb_hi    = vmath.hypot(m[0] - 1000, m[1] - 460) < 32;
    hb_mid   = vmath.hypot(m[0] - 1100, m[1] - 460) < 32;
    hb_low   = vmath.hypot(m[0] - 1200, m[1] - 460) < 32;
    hb_flt   = vmath.hypot(m[0] - 1300, m[1] - 460) < 32;

    h_fx_knob_a = vmath.hypot(m[0] - 650, m[1] - 640) < 32;
    h_fx_knob_b = vmath.hypot(m[0] - 750, m[1] - 640) < 32;
    is_xfader   = (m[0] >= 500 && m[0] <= 900 && m[1] >= 750 && m[1] <= 810);

    if (mouse_click) {
        if (active_knob == 0) {
            if (is_gain_a) { active_knob = 14; active_deck = 0; }
            if (is_gain_b) { active_knob = 15; active_deck = 1; }
            if (ha_pitch)  { active_knob = 1;  active_deck = 0; }
            if (ha_hi)     { active_knob = 2;  active_deck = 0; }
            if (ha_mid)    { active_knob = 3;  active_deck = 0; }
            if (ha_low)    { active_knob = 4;  active_deck = 0; }
            if (ha_flt)    { active_knob = 5;  active_deck = 0; }

            if (hb_pitch)  { active_knob = 6;  active_deck = 1; }
            if (hb_hi)     { active_knob = 7;  active_deck = 1; }
            if (hb_mid)    { active_knob = 8;  active_deck = 1; }
            if (hb_low)    { active_knob = 9;  active_deck = 1; }
            if (hb_flt)    { active_knob = 10; active_deck = 1; }

            if (h_fx_knob_a) { active_knob = 12; active_deck = 0; }
            if (h_fx_knob_b) { active_knob = 13; active_deck = 1; }
            if (is_xfader)   { active_knob = 11; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 14) { deck_a_gain = vmath.clamp(((m[0] - 285.0) / 50.0) * 2.0, 0.0, 2.0); }
        if (active_knob == 15) { deck_b_gain = vmath.clamp(((m[0] - 1085.0) / 50.0) * 2.0, 0.0, 2.0); }

        if (active_knob == 1)  { deck_a_pitch = vmath.clamp(deck_a_pitch - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 2)  { deck_a_eq_hi  = vmath.clamp(deck_a_eq_hi - delta, -12.0, 12.0); }
        if (active_knob == 3)  { deck_a_eq_mid = vmath.clamp(deck_a_eq_mid - delta, -12.0, 12.0); }
        if (active_knob == 4)  { deck_a_eq_low = vmath.clamp(deck_a_eq_low - delta, -12.0, 12.0); }
        if (active_knob == 5)  { deck_a_filter = vmath.clamp(deck_a_filter - (delta * 0.02), -1.0, 1.0); }

        if (active_knob == 6)  { deck_b_pitch = vmath.clamp(deck_b_pitch - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 7)  { deck_b_eq_hi  = vmath.clamp(deck_b_eq_hi - delta, -12.0, 12.0); }
        if (active_knob == 8)  { deck_b_eq_mid = vmath.clamp(deck_b_eq_mid - delta, -12.0, 12.0); }
        if (active_knob == 9)  { deck_b_eq_low = vmath.clamp(deck_b_eq_low - delta, -12.0, 12.0); }
        if (active_knob == 10) { deck_b_filter = vmath.clamp(deck_b_filter - (delta * 0.02), -1.0, 1.0); }

        if (active_knob == 12) {
            if (active_fx_unit_a == 1) { fx_comp_thresh_a = vmath.clamp(fx_comp_thresh_a - delta, -30.0, 0.0); }
            if (active_fx_unit_a == 2) { fx_drive_a       = vmath.clamp(fx_drive_a - (delta * 0.01), 0.0, 1.0); }
            if (active_fx_unit_a == 3) { fx_rev_mix_a     = vmath.clamp(fx_rev_mix_a - (delta * 0.01), 0.0, 1.0); }
        }

        if (active_knob == 13) {
            if (active_fx_unit_b == 1) { fx_comp_thresh_b = vmath.clamp(fx_comp_thresh_b - delta, -30.0, 0.0); }
            if (active_fx_unit_b == 2) { fx_drive_b       = vmath.clamp(fx_drive_b - (delta * 0.01), 0.0, 1.0); }
            if (active_fx_unit_b == 3) { fx_rev_mix_b     = vmath.clamp(fx_rev_mix_b - (delta * 0.01), 0.0, 1.0); }
        }

        if (active_knob == 11){ crossfader_pos = vmath.clamp((m[0] - 520.0) / 360.0, 0.0, 1.0); }
    } else {
        active_knob = 0;
    }

    prev_mouse_state = mouse_click ? 1 : 0;

    # Apply Pitch with Vinyl Brake Multiplier
    vaudio.set_pitch(deck_a_track, vmath.pow(2.0, deck_a_pitch / 12.0) * brake_a_speed);
    vaudio.set_pitch(deck_b_track, vmath.pow(2.0, deck_b_pitch / 12.0) * brake_b_speed);

    # Gain-scaled crossfader volume
    gain_a = vmath.cos(crossfader_pos * 0.5 * 3.14159265);
    gain_b = vmath.sin(crossfader_pos * 0.5 * 3.14159265);
    vaudio.sound_volume(deck_a_track, deck_a_vol * deck_a_gain * gain_a);
    vaudio.sound_volume(deck_b_track, deck_b_vol * deck_b_gain * gain_b);

    # --- DYNAMIC EQ & BIPOLAR FILTER SWITCHING ---
    peaks :: Array = vaudio.get_eq_peaks();
    if (peaks.length() >= 3) {
        track_low_freq = track_low_freq + (peaks[0] - track_low_freq) * 0.15;
        track_mid_freq = track_mid_freq + (peaks[1] - track_mid_freq) * 0.15;
        track_hi_freq  = track_hi_freq  + (peaks[2] - track_hi_freq)  * 0.15;
    }

    cur_eq_low = deck_a_eq_low;
    cur_eq_mid = deck_a_eq_mid;
    cur_eq_hi  = deck_a_eq_hi;
    cur_flt    = deck_a_filter;

    if (active_deck == 1 || (active_knob == 0 && crossfader_pos >= 0.5)) {
        cur_eq_low = deck_b_eq_low;
        cur_eq_mid = deck_b_eq_mid;
        cur_eq_hi  = deck_b_eq_hi;
        cur_flt    = deck_b_filter;
    }

    # --- BAND 0: BIPOLAR DJ FILTER SWEEP ---
    flt_q :: Float64 = 0.707 + (vmath.abs(cur_flt) * 0.5);

    if (cur_flt < -0.01) {
        # LPF MODE (Mode 0): Cutoff sweeps down from 20kHz to 20Hz
        lpf_freq :: Float64 = 20000.0 * vmath.pow(10.0, 3.0 * cur_flt);
        vaudio.set_eq(0, lpf_freq, 0.0, flt_q, 0);
    } else if (cur_flt > 0.01) {
        # HPF MODE (Mode 1): Cutoff sweeps up from 20Hz to 16kHz
        hpf_freq :: Float64 = 20.0 * vmath.pow(10.0, 2.9 * cur_flt);
        vaudio.set_eq(0, hpf_freq, 0.0, flt_q, 1);
    } else {
        # NEUTRAL / BYPASS (Subsonic HPF at 20Hz)
        vaudio.set_eq(0, 20.0, 0.0, 0.707, 1);
    }

    # --- BANDS 1..3: 3-BAND PEAKING EQ ---
    wide_q :: Float64 = 0.5;
    vaudio.set_eq(1, track_low_freq, cur_eq_low, wide_q, 2);
    vaudio.set_eq(2, track_mid_freq, cur_eq_mid, wide_q, 2);
    vaudio.set_eq(3, track_hi_freq,  cur_eq_hi,  wide_q, 2);

    # --- STREAM-ISOLATED SINGLE-PASS DSP EVALUATION ---
    cur_unit  = (crossfader_pos < 0.5) ? active_fx_unit_a : active_fx_unit_b;
    cur_thresh= (crossfader_pos < 0.5) ? fx_comp_thresh_a : fx_comp_thresh_b;
    cur_drive = (crossfader_pos < 0.5) ? fx_drive_a       : fx_drive_b;
    cur_rev   = (crossfader_pos < 0.5) ? fx_rev_mix_a     : fx_rev_mix_b;

    if (cur_unit == 1) {
        vaudio.set_compressor(cur_thresh, 4.0, 30.0, 400.0, 0.0, 1, 1);
        vaudio.set_dsp(0.0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0);
    } if (cur_unit == 2) {
        vaudio.set_dsp(cur_drive, 4);
        vaudio.set_compressor(0.0, 1.0, 0.0, 100.0, 0.0, 0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0);
    } if (cur_unit == 3) {
        vaudio.set_reverb(0.15, cur_rev, 20.0, 0.3, 1);
        vaudio.set_dsp(0.0, 0); vaudio.set_compressor(0.0, 1.0, 10.0, 100.0, 0.0, 0, 0);
    } if (cur_unit == 0) {
        vaudio.set_dsp(0.0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0); vaudio.set_compressor(0.0, 1.0, 10.0, 100.0, 0.0, 0, 0);
    }

    vglib.begin();
        vglib.clear(COLOR_BG);

        # TOP HEADER BAR
        header_a_str = "DECK A: VANISHED - " + string(vmath.round(deck_a_bpm)) + "BPM";
        header_b_str = "DECK B: CRIMEWAVE - " + string(vmath.round(deck_b_bpm)) + "BPM";
        vglib.text_ex(vcr_font, header_a_str, 150, 26, 14, COLOR_DECK_A);
        vglib.text_ex(vcr_font, header_b_str, 950, 26, 14, COLOR_DECK_B);

        btn_a_col = (deck_a_play == 1) ? COLOR_DECK_A : vglib.rgba(35, 42, 54, 255);
        vglib.rect(50, 20, 90, 32, btn_a_col);
        vglib.text_ex(vcr_font, (deck_a_play == 1 ? "PAUSE" : "PLAY A"), 62, 30, 11, (deck_a_play == 1 ? vglib.BLACK : vglib.WHITE));

        vglib.rect(650, 20, 100, 32, COLOR_AMBER);
        vglib.text_ex(vcr_font, "BEAT SYNC", 662, 30, 11, vglib.BLACK);

        btn_b_col = (deck_b_play == 1) ? COLOR_DECK_B : vglib.rgba(35, 42, 54, 255);
        vglib.rect(1260, 20, 90, 32, btn_b_col);
        vglib.text_ex(vcr_font, (deck_b_play == 1 ? "PAUSE" : "PLAY B"), 1272, 30, 11, (deck_b_play == 1 ? vglib.BLACK : vglib.WHITE));

        vglib.line(50, 62, 1350, 62, COLOR_CARD_RIM);

        # WAVEFORM OVERVIEW CARDS
        vglib.rect(50, 80, 630, 160, COLOR_CARD);
        vglib.line(50, 80, 680, 80, COLOR_DECK_A);
        vglib.line(50, 240, 680, 240, COLOR_CARD_RIM);
        vglib.line(50, 80, 50, 240, COLOR_CARD_RIM);
        vglib.line(680, 80, 680, 240, COLOR_CARD_RIM);

        num_pk_a = wave_peaks_a.length(); step_w_a :: Float64 = 630.0 / num_pk_a;
        through idx :: 0..(int64(num_pk_a) - 1) -> loop {
            pk = wave_peaks_a[idx]; px :: Float64 = 50.0 + (idx * step_w_a); ph :: Float64 = pk * 60.0;
            bar_col = (idx % 2 == 0) ? COLOR_DECK_A : COLOR_AMBER;
            vglib.rect(px, 160.0 - ph, step_w_a - 0.5, ph * 2.0, bar_col);
        };
        ph_a_x :: Float64 = 50.0 + (deck_a_pos * 630.0);
        vglib.line(ph_a_x, 80, ph_a_x, 240, vglib.WHITE);
        vglib.circle(ph_a_x, 80, 4.0, COLOR_DECK_A);

        vglib.rect(720, 80, 630, 160, COLOR_CARD);
        vglib.line(720, 80, 1350, 80, COLOR_DECK_B);
        vglib.line(720, 240, 1350, 240, COLOR_CARD_RIM);
        vglib.line(720, 80, 720, 240, COLOR_CARD_RIM);
        vglib.line(1350, 80, 1350, 240, COLOR_CARD_RIM);

        num_pk_b = wave_peaks_b.length(); step_w_b :: Float64 = 630.0 / num_pk_b;
        through idx :: 0..(int64(num_pk_b) - 1) -> loop {
            pk = wave_peaks_b[idx]; px :: Float64 = 720.0 + (idx * step_w_b); ph :: Float64 = pk * 60.0;
            bar_col = (idx % 2 == 0) ? COLOR_DECK_B : COLOR_AMBER;
            vglib.rect(px, 160.0 - ph, step_w_b - 0.5, ph * 2.0, bar_col);
        };
        ph_b_x :: Float64 = 720.0 + (deck_b_pos * 630.0);
        vglib.line(ph_b_x, 80, ph_b_x, 240, vglib.WHITE);
        vglib.circle(ph_b_x, 80, 4.0, COLOR_DECK_B);

        # BEATMATCH PHASE SCOPE
        vglib.rect(50, 260, 1300, 120, vglib.rgba(8, 10, 14, 255));
        vglib.line(50, 260, 1350, 260, COLOR_CARD_RIM);
        vglib.line(1350, 260, 1350, 380, COLOR_CARD_RIM);
        vglib.line(1350, 380, 50, 380, COLOR_CARD_RIM);
        vglib.line(50, 380, 50, 260, COLOR_CARD_RIM);

        vglib.line(50, 320, 1350, 320, vglib.rgba(35, 42, 54, 255));
        vglib.text_ex(vcr_font, "REAL-TIME BEATMATCH PHASE SCOPE", 65, 270, 11, vglib.rgba(160, 170, 185, 255));

        raw_lufs = vaudio.get_lufs();
        target_intensity :: Float64 = vmath.clamp((raw_lufs + 70.0) / 70.0, 0.0, 1.0);

        smooth_lufs_intensity = smooth_lufs_intensity + (target_intensity - smooth_lufs_intensity) * 0.1;

        wave_speed :: Float64 = 2.0  + (smooth_lufs_intensity * 4.0);

        dt :: Float64 = 0.016;
        scope_phase_a = scope_phase_a + (dt * wave_speed);
        scope_phase_b = scope_phase_b + (dt * wave_speed * 0.96);

        vglib.draw_phase_scope(50.0, 1350.0, 320.0, scope_phase_a, scope_phase_b, smooth_lufs_intensity, deck_a_play == 1, deck_b_play == 1);
        
        # --- SCROLLING BACKGROUND GRAY WAVEFORM (130 BARS - ZOOMED SLICE) ---
        bg_wave_col = vglib.rgba(160, 170, 185, 30);
        num_scope_bars :: Int64 = 130;
        bar_w :: Float64 = 10.0;        # 1300px width / 130 bars = 10px per bar
        window_size :: Float64 = 15.0;  # Zooms into a slice of 15 peaks across the scope

        scroll_speed :: Float64 = (deck_a_play == 1 || deck_b_play == 1) ? 1.5 : 0.2;
        scroll_offset :: Float64 = run_time * scroll_speed;

        active_peaks = (crossfader_pos < 0.5) ? wave_peaks_a : wave_peaks_b;
        total_pks = active_peaks.length();

        if (total_pks > 0) {
            through b :: 0..129 -> loop {
                sample_float :: Float64 = scroll_offset + ((b / 130.0) * window_size);
                sample_idx = int64(sample_float) % total_pks;
                
                pk_val = active_peaks[sample_idx];
                
                bar_x :: Float64 = 50.0 + (b * bar_w);
                bar_h :: Float64 = pk_val * 48.0;

                vglib.rect(bar_x + 1.0, 320.0 - bar_h, bar_w - 2.0, bar_h * 2.0, bg_wave_col);
            };
        }

        # DUAL CHANNEL EQ RACK
        pa_norm = (deck_a_pitch + 12.0) / 24.0; pa_str = (deck_a_pitch >= 0.0 ? "+" : "") + string(vmath.round(deck_a_pitch * 10.0) / 10.0) + "st";
        pb_norm = (deck_b_pitch + 12.0) / 24.0; pb_str = (deck_b_pitch >= 0.0 ? "+" : "") + string(vmath.round(deck_b_pitch * 10.0) / 10.0) + "st";

        draw_dj_knob("PITCH A", 100, 460, pa_norm, pa_str, COLOR_DECK_A, ha_pitch, active_knob == 1);
        draw_dj_knob("HIGH", 200, 460, (deck_a_eq_hi + 12.0) / 24.0, string(vmath.round(deck_a_eq_hi)) + "dB", COLOR_AMBER, ha_hi, active_knob == 2);
        draw_dj_knob("MID", 300, 460, (deck_a_eq_mid + 12.0) / 24.0, string(vmath.round(deck_a_eq_mid)) + "dB", COLOR_AMBER, ha_mid, active_knob == 3);
        draw_dj_knob("LOW", 400, 460, (deck_a_eq_low + 12.0) / 24.0, string(vmath.round(deck_a_eq_low)) + "dB", COLOR_AMBER, ha_low, active_knob == 4);
        flt_a_str = (deck_a_filter < -0.01) ? "LPF" : ((deck_a_filter > 0.01) ? "HPF" : "OFF");
        flt_a_col = (deck_a_filter < -0.01) ? COLOR_DECK_A : ((deck_a_filter > 0.01) ? COLOR_DECK_B : vglib.rgba(160, 170, 185, 255));
        draw_dj_knob("FILTER A", 500, 460, (deck_a_filter + 1.0) / 2.0, flt_a_str, flt_a_col, ha_flt, active_knob == 5);

        draw_dj_knob("PITCH B", 900, 460, pb_norm, pb_str, COLOR_DECK_B, hb_pitch, active_knob == 6);
        draw_dj_knob("HIGH", 1000, 460, (deck_b_eq_hi + 12.0) / 24.0, string(vmath.round(deck_b_eq_hi)) + "dB", COLOR_AMBER, hb_hi, active_knob == 7);
        draw_dj_knob("MID", 1100, 460, (deck_b_eq_mid + 12.0) / 24.0, string(vmath.round(deck_b_eq_mid)) + "dB", COLOR_AMBER, hb_mid, active_knob == 8);
        draw_dj_knob("LOW", 1200, 460, (deck_b_eq_low + 12.0) / 24.0, string(vmath.round(deck_b_eq_low)) + "dB", COLOR_AMBER, hb_low, active_knob == 9);
        flt_b_str = (deck_b_filter < -0.01) ? "LPF" : ((deck_b_filter > 0.01) ? "HPF" : "OFF");
        flt_b_col = (deck_b_filter < -0.01) ? COLOR_DECK_A : ((deck_b_filter > 0.01) ? COLOR_DECK_B : vglib.rgba(160, 170, 185, 255));
        draw_dj_knob("FILTER B", 1300, 460, (deck_b_filter + 1.0) / 2.0, flt_b_str, flt_b_col, hb_flt, active_knob == 10);

        # GAIN SLIDERS AT Y = 530
        draw_gain_slider_bar("GAIN A", 230, 530, 150, deck_a_gain, COLOR_DECK_A, active_knob == 14);
        draw_gain_slider_bar("GAIN B", 1030, 530, 150, deck_b_gain, COLOR_DECK_B, active_knob == 15);

        # CENTER MASTER STATUS & DUAL DECK EFX UNIT RACK CARD
        vglib.rect(590, 400, 220, 300, COLOR_CARD);
        has_any_fx = (active_fx_unit_a != 0) || (active_fx_unit_b != 0);
        vglib.line(590, 400, 810, 400, has_any_fx ? COLOR_AMBER : COLOR_CARD_RIM);
        vglib.line(810, 400, 810, 700, COLOR_CARD_RIM);
        vglib.line(810, 700, 590, 700, COLOR_CARD_RIM);
        vglib.line(590, 700, 590, 400, COLOR_CARD_RIM);

        vglib.text_ex(vcr_font, "MASTER FX BUS", 645, 412, 11, vglib.rgba(160, 170, 185, 255));

        lufs_val = vaudio.get_lufs();
        vglib.text_ex(vcr_font, string(vmath.round(lufs_val)) + " LUFS", 660, 432, 14, COLOR_AMBER);

        vglib.rect(605, 740, 190, 30, vglib.rgba(12, 14, 20, 255));
        vglib.draw_spectrum_analyzer(605.0, 740.0, 190.0, 30.0, 20, run_time, smooth_lufs_intensity);

        vglib.line(600, 482, 800, 482, COLOR_CARD_RIM);

        rms_val = vaudio.get_rms();
        if (rms_val > peak_hold_rms) {
            peak_hold_rms = rms_val;
        } else {
            peak_hold_rms = peak_hold_rms - 0.005;
        }

        vglib.rect(610, 460, 180, 10, vglib.rgba(28, 32, 42, 255));
        vglib.rect(610, 460, rms_val * 180.0, 10, COLOR_DECK_A);
        
        peak_x :: Float64 = 610.0 + (peak_hold_rms * 180.0);
        vglib.rect(peak_x, 458, 2, 14, COLOR_DECK_B);

        vglib.line(600, 482, 800, 482, COLOR_CARD_RIM);
        vglib.text_ex(vcr_font, "DECK A FX", 605, 492, 9, COLOR_DECK_A);
        vglib.text_ex(vcr_font, "DECK B FX", 725, 492, 9, COLOR_DECK_B);

        btn_off_a  = (active_fx_unit_a == 0) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(35, 42, 54, 255);
        btn_comp_a = (active_fx_unit_a == 1) ? COLOR_AMBER : vglib.rgba(35, 42, 54, 255);
        btn_sat_a  = (active_fx_unit_a == 2) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(35, 42, 54, 255);
        btn_rev_a  = (active_fx_unit_a == 3) ? vglib.rgba(160, 90, 255, 255) : vglib.rgba(35, 42, 54, 255);

        vglib.rect(605, 505, 35, 22, btn_off_a);  vglib.text_ex(vcr_font, "OFF", 611, 512, 8, vglib.WHITE);
        vglib.rect(605, 530, 35, 22, btn_comp_a); vglib.text_ex(vcr_font, "CMP", 611, 537, 8, vglib.WHITE);
        vglib.rect(605, 555, 35, 22, btn_sat_a);  vglib.text_ex(vcr_font, "SAT", 611, 562, 8, vglib.WHITE);
        vglib.rect(605, 580, 35, 22, btn_rev_a);  vglib.text_ex(vcr_font, "REV", 611, 587, 8, vglib.WHITE);

        btn_off_b  = (active_fx_unit_b == 0) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(35, 42, 54, 255);
        btn_comp_b = (active_fx_unit_b == 1) ? COLOR_AMBER : vglib.rgba(35, 42, 54, 255);
        btn_sat_b  = (active_fx_unit_b == 2) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(35, 42, 54, 255);
        btn_rev_b  = (active_fx_unit_b == 3) ? vglib.rgba(160, 90, 255, 255) : vglib.rgba(35, 42, 54, 255);

        vglib.rect(760, 505, 35, 22, btn_off_b);  vglib.text_ex(vcr_font, "OFF", 766, 512, 8, vglib.WHITE);
        vglib.rect(760, 530, 35, 22, btn_comp_b); vglib.text_ex(vcr_font, "CMP", 766, 537, 8, vglib.WHITE);
        vglib.rect(760, 555, 35, 22, btn_sat_b);  vglib.text_ex(vcr_font, "SAT", 766, 562, 8, vglib.WHITE);
        vglib.rect(760, 580, 35, 22, btn_rev_b);  vglib.text_ex(vcr_font, "REV", 766, 587, 8, vglib.WHITE);

        fx_norm_a :: Float64 = 0.0; fx_str_a :: String = "OFF"; fx_col_a = vglib.rgba(120, 130, 150, 255);
        if (active_fx_unit_a == 1) { fx_norm_a = (fx_comp_thresh_a + 30.0) / 30.0; fx_str_a = string(vmath.round(fx_comp_thresh_a)) + "dB"; fx_col_a = btn_comp_a; }
        if (active_fx_unit_a == 2) { fx_norm_a = fx_drive_a; fx_str_a = string(vmath.round(fx_drive_a * 100.0)) + "%"; fx_col_a = btn_sat_a; }
        if (active_fx_unit_a == 3) { fx_norm_a = fx_rev_mix_a; fx_str_a = string(vmath.round(fx_rev_mix_a * 100.0)) + "%"; fx_col_a = btn_rev_a; }

        draw_dj_knob("PARAM A", 650, 640, fx_norm_a, fx_str_a, COLOR_DECK_A, h_fx_knob_a, active_knob == 12);

        fx_norm_b :: Float64 = 0.0; fx_str_b :: String = "OFF"; fx_col_b = vglib.rgba(120, 130, 150, 255);
        if (active_fx_unit_b == 1) { fx_norm_b = (fx_comp_thresh_b + 30.0) / 30.0; fx_str_b = string(vmath.round(fx_comp_thresh_b)) + "dB"; fx_col_b = btn_comp_b; }
        if (active_fx_unit_b == 2) { fx_norm_b = fx_drive_b; fx_str_b = string(vmath.round(fx_drive_b * 100.0)) + "%"; fx_col_b = btn_sat_b; }
        if (active_fx_unit_b == 3) { fx_norm_b = fx_rev_mix_b; fx_str_b = string(vmath.round(fx_rev_mix_b * 100.0)) + "%"; fx_col_b = btn_rev_b; }

        draw_dj_knob("PARAM B", 750, 640, fx_norm_b, fx_str_b, COLOR_DECK_B, h_fx_knob_b, active_knob == 13);

        # --- CENTER MINI MONITORING PANEL (RMS dB + GAIN REDUCTION) ---
        mon_x :: Float64 = 648.0;
        mon_y :: Float64 = 505.0;
        mon_w :: Float64 = 104.0;
        mon_h :: Float64 = 97.0;

        # Fetch Real-Time Data from C++ Audio Engine
        raw_rms :: Float64 = vaudio.get_rms();  # 0.0 to 1.0 (Mapped -60dB to 0dB)
        raw_gr  :: Float64 = vaudio.get_gr();   # 0.0 to 24.0+ dB Reduction

        # Convert normalized RMS to true dB readout
        rms_db :: Float64 = (raw_rms * 60.0) - 60.0;
        rms_str = (rms_db <= -59.0) ? "-INF" : (string(vmath.round(rms_db)) + "dB");

        # --- 1. MINI RMS METER ROW ---
        vglib.text_ex(vcr_font, "RMS", mon_x + 6, mon_y + 8, 8, vglib.rgba(160, 170, 185, 255));
        vglib.text_ex(vcr_font, rms_str, mon_x + mon_w - 36, mon_y + 8, 8, COLOR_DECK_A);

        vglib.rect(mon_x + 6, mon_y + 20, 92, 10, vglib.rgba(24, 28, 36, 255));
        
        rms_bar_w :: Float64 = vmath.clamp(raw_rms * 92.0, 0.0, 92.0);
        rms_col = (raw_rms > 0.85) ? COLOR_AMBER : ((raw_rms >= 0.95) ? vglib.rgba(255, 50, 50, 255) : COLOR_DECK_A);
        
        if (rms_bar_w > 1.0) {
            vglib.rect(mon_x + 6, mon_y + 20, rms_bar_w, 10, rms_col);
        }

        # --- 2. MINI GAIN REDUCTION (GR) METER ROW ---
        gr_db_str = (raw_gr < 0.1) ? "0.0dB" : ("-" + string(vmath.round(raw_gr * 10.0) / 10.0) + "dB");
        gr_col = (raw_gr > 0.5) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(100, 110, 130, 255);

        vglib.text_ex(vcr_font, "GR", mon_x + 6, mon_y + 38, 8, vglib.rgba(160, 170, 185, 255));
        vglib.text_ex(vcr_font, gr_db_str, mon_x + mon_w - 42, mon_y + 38, 8, gr_col);

        vglib.rect(mon_x + 6, mon_y + 50, 92, 10, vglib.rgba(24, 28, 36, 255));

        # Gain Reduction fills rightwards in glowing red/amber when compressor clamps down
        gr_bar_w :: Float64 = vmath.clamp((raw_gr / 18.0) * 92.0, 0.0, 92.0);
        if (gr_bar_w > 1.0) {
            vglib.rect(mon_x + 6, mon_y + 50, gr_bar_w, 10, vglib.rgba(255, 60, 60, 255));
        }

        # Status Footer Indicator
        comp_active = (active_fx_unit_a == 1 || active_fx_unit_b == 1);
        status_txt  = comp_active ? "COMP ACTIVE" : "BYPASSED";
        status_col  = comp_active ? COLOR_AMBER : vglib.rgba(80, 90, 105, 255);
        vglib.text_ex(vcr_font, status_txt, mon_x + 16, mon_y + 74, 8, status_col);

        # --- DECK A DUAL PERFORMANCE PAD BANK ---
        # Top Row: Performance Roll / Brake
        through col :: 0..3 -> loop {
            px :: Int64 = 30 + (col * 140); py :: Int64 = 580;
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            if (col == 0) { draw_perf_pad("ROLL 1/4", "LOOP 1/4", px, py, 130, 55, COLOR_AMBER, roll_a_active == 1, is_hov); }
            if (col == 1) { draw_perf_pad("ROLL 1/8", "LOOP 1/8", px, py, 130, 55, COLOR_AMBER, roll_a_active == 2, is_hov); }
            if (col == 2) { draw_perf_pad("ROLL 1/16", "LOOP 1/16", px, py, 130, 55, COLOR_AMBER, roll_a_active == 3, is_hov); }
            if (col == 3) { draw_perf_pad("BRAKE", "STOP RAMP", px, py, 130, 55, vglib.rgba(255, 60, 60, 255), brake_a_active == 1, is_hov); }
        };

        # Bottom Row: Hot Cues 1..4
        through col :: 0..3 -> loop {
            px :: Int64 = 30 + (col * 140); py :: Int64 = 645;
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            sec_str = string(vmath.round(cues_a[col] * 180.0 * 10.0) / 10.0) + "s";
            draw_perf_pad("CUE " + string(col + 1), sec_str, px, py, 130, 55, COLOR_DECK_A, false, is_hov);
        };

        # --- DECK B DUAL PERFORMANCE PAD BANK ---
        # Top Row: Performance Roll / Brake
        through col :: 0..3 -> loop {
            px :: Int64 = 820 + (col * 140); py :: Int64 = 580;
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            if (col == 0) { draw_perf_pad("ROLL 1/4", "LOOP 1/4", px, py, 130, 55, COLOR_AMBER, roll_b_active == 1, is_hov); }
            if (col == 1) { draw_perf_pad("ROLL 1/8", "LOOP 1/8", px, py, 130, 55, COLOR_AMBER, roll_b_active == 2, is_hov); }
            if (col == 2) { draw_perf_pad("ROLL 1/16", "LOOP 1/16", px, py, 130, 55, COLOR_AMBER, roll_b_active == 3, is_hov); }
            if (col == 3) { draw_perf_pad("BRAKE", "STOP RAMP", px, py, 130, 55, vglib.rgba(255, 60, 60, 255), brake_b_active == 1, is_hov); }
        };

        # Bottom Row: Hot Cues 1..4
        through col :: 0..3 -> loop {
            px :: Int64 = 820 + (col * 140); py :: Int64 = 645;
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            sec_str = string(vmath.round(cues_b[col] * 180.0 * 10.0) / 10.0) + "s";
            draw_perf_pad("CUE " + string(col + 1), sec_str, px, py, 130, 55, COLOR_DECK_B, false, is_hov);
        };

        # CROSSFADER
        vglib.rect(500, 770, 400, 20, vglib.rgba(20, 24, 32, 255));
        vglib.line(500, 780, 900, 780, COLOR_CARD_RIM);
        vglib.line(700, 765, 700, 795, vglib.rgba(100, 110, 130, 255));

        handle_x :: Float64 = 500.0 + (crossfader_pos * 360.0);
        vglib.rect(handle_x, 755, 40, 50, vglib.rgba(220, 230, 245, 255));
        vglib.rect(handle_x + 18, 760, 4, 40, vglib.BLACK);

        vglib.text_ex(vcr_font, "DECK A", 500, 820, 11, COLOR_DECK_A);
        vglib.text_ex(vcr_font, "CROSSFADER", 652, 820, 11, vglib.WHITE);
        vglib.text_ex(vcr_font, "DECK B", 845, 820, 11, COLOR_DECK_B);

        vglib.text_ex(vcr_font, "VYNE AUDIO ENGINE v2.1.0 | DUAL DECK PERFORMANCE PRO CONSOLE", 380, 860, 12, vglib.rgba(120, 130, 150, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();