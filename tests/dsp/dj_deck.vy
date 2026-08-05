ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";

vglib.init(1400, 900, 60, "VYNE DJ CONSOLE v2.0", 0);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# Load Deck Audio Tracks
deck_a_track = vaudio.play_stream(configs.Audios.never_fade_away);
deck_b_track = vaudio.play_stream(configs.Audios.ikit1bb);

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
active_knob    :: Int64   = 0;     # Active dragged UI knob ID
active_deck    :: Int64   = 0;     # 0 = Deck A, 1 = Deck B

# --- CENTER FX RACK STATE ---
active_fx_unit :: Int64   = 0;     # 0 = OFF, 1 = COMP, 2 = SAT, 3 = REV
fx_drive       :: Float64 = 0.25;  # Saturator Drive
fx_comp_thresh :: Float64 = -12.0; # Compressor Threshold
fx_rev_mix     :: Float64 = 0.30;  # Reverb Mix

# --- DECK A STATE ---
deck_a_play    :: Int64   = 0;
deck_a_bpm     :: Float64 = 130.0;
deck_a_pitch   :: Float64 = 0.0;
deck_a_pos     :: Float64 = 0.0;
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
deck_b_eq_hi   :: Float64 = 0.0;
deck_b_eq_mid  :: Float64 = 0.0;
deck_b_eq_low  :: Float64 = 0.0;
deck_b_filter  :: Float64 = 0.0;
deck_b_vol     :: Float64 = 0.85;

# --- COLOR PALETTE DEFINITIONS ---
COLOR_BG       = vglib.rgba(10, 11, 15, 255);    # Deep Obsidian
COLOR_CARD     = vglib.rgba(18, 22, 30, 255);    # Panel Cards
COLOR_CARD_RIM = vglib.rgba(40, 48, 64, 255);    # Subtle Borders
COLOR_DECK_A   = vglib.rgba(0, 240, 255, 255);   # Electric Cyan
COLOR_DECK_B   = vglib.rgba(255, 45, 120, 255);  # Neon Sunset Pink
COLOR_AMBER    = vglib.rgba(255, 170, 0, 255);   # FX / Parameter Amber

# --- HOT CUES & PEAK BUFFERS ---
cues_a :: Array = [0.00, 0.12, 0.25, 0.37, 0.50, 0.62, 0.75, 0.87];
cues_b :: Array = [0.00, 0.12, 0.25, 0.37, 0.50, 0.62, 0.75, 0.87];

wave_peaks_a :: Array = [];
wave_peaks_b :: Array = [];
through i :: 0..149 -> loop {
    wave_peaks_a.push(vmath.abs((vmath.sin(i * 0.1) * 0.5) + (vmath.cos(i * 0.3) * 0.3)));
    wave_peaks_b.push(vmath.abs((vmath.cos(i * 0.12) * 0.6) + (vmath.sin(i * 0.25) * 0.2)));
};

# --- UI HELPER: ROTARY KNOB WITH FIXED BORDER SIZE (NO GREEN HOVER) ---
fn draw_dj_knob(name, x, y, val_norm, display_val, color, is_hovered, is_active) {
    radius_base = 32.0;
    if (is_active) { radius_base = 34.0; } # Tighter active state, zero hover growth

    # Outer Housing
    vglib.circle(x, y, radius_base, vglib.BLACK);
    
    # Outer Active Ring
    if (is_active) {
        vglib.circle(x, y, radius_base - 1.5, color);
    } else {
        vglib.circle(x, y, radius_base - 2.0, vglib.rgba(35, 40, 52, 255));
    }

    # Inner Knob Face
    vglib.circle(x, y, 24.0, vglib.rgba(16, 18, 24, 255));
    vglib.circle(x, y, 15.0, vglib.rgba(26, 30, 38, 255));
    
    # Solid Value Arc (Continuous Radial Band)
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

    # Center Pointer Needle
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

# --- UI HELPER: MPC CUE PAD ---
fn draw_hot_cue_pad(id, x, y, w, h, pad_color, is_active, is_hovered, cue_time) {
    bg_col   = is_active ? pad_color : vglib.rgba(20, 24, 32, 255);
    rim_col  = is_active ? vglib.WHITE : (is_hovered ? pad_color : vglib.rgba(45, 52, 66, 255));
    text_col = is_active ? vglib.BLACK : vglib.WHITE;

    vglib.rect(x, y, w, h, bg_col);
    vglib.line(x, y, x + w, y, rim_col);
    vglib.line(x + w, y, x + w, y + h, rim_col);
    vglib.line(x + w, y + h, x, y + h, rim_col);
    vglib.line(x, y + h, x, y, rim_col);

    vglib.text_ex(vcr_font, "CUE " + string(id + 1), x + 10, y + 10, 11, text_col);
    sec_str = string(vmath.round(cue_time * 180.0 * 10.0) / 10.0) + "s";
    sub_col = is_active ? vglib.BLACK : vglib.rgba(140, 150, 165, 255);
    vglib.text_ex(vcr_font, sec_str, x + 10, y + 28, 9, sub_col);
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

    # Update Playheads
    if (deck_a_play == 1) {
        deck_a_pos = deck_a_pos + 0.0004 * (deck_a_bpm / 130.0);
        if (deck_a_pos > 1.0) { deck_a_pos = 0.0; }
    }
    if (deck_b_play == 1) {
        deck_b_pos = deck_b_pos + 0.0004 * (deck_b_bpm / 128.0);
        if (deck_b_pos > 1.0) { deck_b_pos = 0.0; }
    }

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
        # Buttons A/B Sync
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

        # Master FX Selector Buttons Click Check
        if (m[1] >= 520 && m[1] <= 545) {
            if (m[0] >= 610 && m[0] <= 645) { active_fx_unit = 0; } # OFF
            if (m[0] >= 650 && m[0] <= 685) { active_fx_unit = 1; } # COMP
            if (m[0] >= 690 && m[0] <= 725) { active_fx_unit = 2; } # SAT
            if (m[0] >= 730 && m[0] <= 765) { active_fx_unit = 3; } # REV
        }

        # Cues Deck A / B
        through p :: 0..7 -> loop {
            row :: Int64 = p / 4; col :: Int64 = p % 4;
            px :: Int64 = 50 + (col * 140); py :: Int64 = 580 + (row * 65);
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                deck_a_pos = cues_a[p];
                vaudio.seek_stream(deck_a_track, deck_a_pos * 180.0);
                deck_a_play = 1;
            }
        };

        through p :: 0..7 -> loop {
            row :: Int64 = p / 4; col :: Int64 = p % 4;
            px :: Int64 = 760 + (col * 140); py :: Int64 = 580 + (row * 65);
            if (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55) {
                deck_b_pos = cues_b[p];
                vaudio.seek_stream(deck_b_track, deck_b_pos * 180.0);
                deck_b_play = 1;
            }
        };
    }

    # --- KNOB & CROSSFADER DRAGGING ---
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

    h_fx_knob = vmath.hypot(m[0] - 700, m[1] - 620) < 32; # Center FX Knob
    is_xfader = (m[0] >= 500 && m[0] <= 900 && m[1] >= 750 && m[1] <= 810);

    if (mouse_click) {
        if (active_knob == 0) {
            if (ha_pitch) { active_knob = 1; }
            if (ha_hi)    { active_knob = 2; }
            if (ha_mid)   { active_knob = 3; }
            if (ha_low)   { active_knob = 4; }
            if (ha_flt)   { active_knob = 5; }

            if (hb_pitch) { active_knob = 6; }
            if (hb_hi)    { active_knob = 7; }
            if (hb_mid)   { active_knob = 8; }
            if (hb_low)   { active_knob = 9; }
            if (hb_flt)   { active_knob = 10; }

            if (h_fx_knob) { active_knob = 12; }
            if (is_xfader) { active_knob = 11; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { deck_a_pitch = vmath.clamp(deck_a_pitch - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 2) { deck_a_eq_hi  = vmath.clamp(deck_a_eq_hi - delta, -12.0, 12.0); }
        if (active_knob == 3) { deck_a_eq_mid = vmath.clamp(deck_a_eq_mid - delta, -12.0, 12.0); }
        if (active_knob == 4) { deck_a_eq_low = vmath.clamp(deck_a_eq_low - delta, -12.0, 12.0); }
        if (active_knob == 5) { deck_a_filter = vmath.clamp(deck_a_filter - (delta * 0.02), -1.0, 1.0); }

        if (active_knob == 6) { deck_b_pitch = vmath.clamp(deck_b_pitch - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 7) { deck_b_eq_hi  = vmath.clamp(deck_b_eq_hi - delta, -12.0, 12.0); }
        if (active_knob == 8) { deck_b_eq_mid = vmath.clamp(deck_b_eq_mid - delta, -12.0, 12.0); }
        if (active_knob == 9) { deck_b_eq_low = vmath.clamp(deck_b_eq_low - delta, -12.0, 12.0); }
        if (active_knob == 10){ deck_b_filter = vmath.clamp(deck_b_filter - (delta * 0.02), -1.0, 1.0); }

        if (active_knob == 12) {
            if (active_fx_unit == 1) { fx_comp_thresh = vmath.clamp(fx_comp_thresh - delta, -30.0, 0.0); }
            if (active_fx_unit == 2) { fx_drive       = vmath.clamp(fx_drive - (delta * 0.01), 0.0, 1.0); }
            if (active_fx_unit == 3) { fx_rev_mix     = vmath.clamp(fx_rev_mix - (delta * 0.01), 0.0, 1.0); }
        }

        if (active_knob == 11){ crossfader_pos = vmath.clamp((m[0] - 520.0) / 360.0, 0.0, 1.0); }
    } else {
        active_knob = 0;
    }

    prev_mouse_state = mouse_click ? 1 : 0;

    # Apply Real-Time DSP Parameters
    vaudio.set_pitch(deck_a_track, vmath.pow(2.0, deck_a_pitch / 12.0));
    vaudio.set_pitch(deck_b_track, vmath.pow(2.0, deck_b_pitch / 12.0));

    gain_a = vmath.cos(crossfader_pos * 0.5 * 3.14159265);
    gain_b = vmath.sin(crossfader_pos * 0.5 * 3.14159265);
    vaudio.sound_volume(deck_a_track, deck_a_vol * gain_a);
    vaudio.sound_volume(deck_b_track, deck_b_vol * gain_b);

    # --- 3-BAND DUAL DECK CROSSFADED EQ ROUTING ---
    active_low_gain  = (deck_a_eq_low  * (1.0 - crossfader_pos)) + (deck_b_eq_low  * crossfader_pos);
    active_mid_gain  = (deck_a_eq_mid  * (1.0 - crossfader_pos)) + (deck_b_eq_mid  * crossfader_pos);
    active_hi_gain   = (deck_a_eq_hi   * (1.0 - crossfader_pos)) + (deck_b_eq_hi   * crossfader_pos);

    vaudio.set_eq(1, 100.0,  active_low_gain, 1.0); # Band 1: Low Cut/Boost (100 Hz)
    vaudio.set_eq(2, 1000.0, active_mid_gain, 1.0); # Band 2: Mid Cut/Boost (1 kHz)
    vaudio.set_eq(3, 8000.0, active_hi_gain,  1.0); # Band 3: High Cut/Boost (8 kHz)

    # Set FX Rack Parameters to Engine
    if (active_fx_unit == 1) {
        vaudio.set_compressor(fx_comp_thresh, 4.0, 30.0, 400.0, 0.0, 1, 1);
        vaudio.set_dsp(0.0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0);
    } if (active_fx_unit == 2) {
        vaudio.set_dsp(fx_drive, 4); # Tube Warmth Mode
        vaudio.set_compressor(0.0, 1.0, 10.0, 100.0, 0.0, 0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0);
    } if (active_fx_unit == 3) {
        vaudio.set_reverb(0.80, fx_rev_mix, 20.0, 0.3, 1);
        vaudio.set_dsp(0.0, 0); vaudio.set_compressor(0.0, 1.0, 10.0, 100.0, 0.0, 0, 0);
    } if (active_fx_unit == 0) {
        vaudio.set_dsp(0.0, 0); vaudio.set_reverb(0.0, 0.0, 0.0, 0.0, 0); vaudio.set_compressor(0.0, 1.0, 10.0, 100.0, 0.0, 0, 0);
    }

    vglib.begin();
        vglib.clear(COLOR_BG);

        # TOP HEADER BAR
        vglib.text_ex(vcr_font, "DECK A: 2T1BB - 130BPM", 150, 26, 14, COLOR_DECK_A);
        vglib.text_ex(vcr_font, "DECK B: CIGERLERIM - 128BPM", 950, 26, 14, COLOR_DECK_B);

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

        # ====================================================================
        # REAL-TIME BEATMATCH PHASE SCOPE (ANALOG PHOSPHOR CRT GLOW)
        # ====================================================================
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

        wave_freq  :: Float64 = 15.0 + (smooth_lufs_intensity * 20.0);
        wave_speed :: Float64 = 2.0  + (smooth_lufs_intensity * 4.0);
        wave_amp   :: Float64 = 8.0  + (smooth_lufs_intensity * 30.0);

        dt :: Float64 = 0.016;
        scope_phase_a = scope_phase_a + (dt * wave_speed);
        scope_phase_b = scope_phase_b + (dt * wave_speed * 0.96);

        prev_zx_a :: Float64 = 50.0; prev_zy_a :: Float64 = 320.0;
        prev_zx_b :: Float64 = 50.0; prev_zy_b :: Float64 = 320.0;
        z_step    :: Float64 = 6.0;  curr_zx :: Float64 = 50.0;

        while (curr_zx <= 1350.0) {
            norm_z = (curr_zx - 50.0) / 1300.0;

            za = vmath.sin(norm_z * wave_freq + scope_phase_a) * wave_amp;
            zb = vmath.sin(norm_z * (wave_freq * 0.98) + scope_phase_b) * wave_amp;

            if (smooth_lufs_intensity > 0.8) {
                za = za + (vmath.sin(norm_z * wave_freq * 1.5 + scope_phase_a) * wave_amp * 0.12);
                zb = zb + (vmath.cos(norm_z * wave_freq * 1.5 + scope_phase_b) * wave_amp * 0.12);
            }

            curr_zy_a :: Float64 = 320.0 - za; 
            curr_zy_b :: Float64 = 320.0 - zb;

            if (curr_zx > 50.0) {
                if (deck_a_play == 1 || (deck_a_play == 0 && deck_b_play == 0)) {
                    vglib.line(prev_zx_a, prev_zy_a - 1.0, curr_zx, curr_zy_a - 1.0, vglib.rgba(0, 180, 220, 80));
                    vglib.line(prev_zx_a, prev_zy_a + 1.0, curr_zx, curr_zy_a + 1.0, vglib.rgba(0, 180, 220, 80));
                    vglib.line(prev_zx_a, prev_zy_a, curr_zx, curr_zy_a, COLOR_DECK_A);
                }
                if (deck_b_play == 1) {
                    vglib.line(prev_zx_b, prev_zy_b - 1.0, curr_zx, curr_zy_b - 1.0, vglib.rgba(200, 30, 90, 80));
                    vglib.line(prev_zx_b, prev_zy_b + 1.0, curr_zx, curr_zy_b + 1.0, vglib.rgba(200, 30, 90, 80));
                    vglib.line(prev_zx_b, prev_zy_b, curr_zx, curr_zy_b, COLOR_DECK_B);
                }
            }
            
            prev_zx_a = curr_zx; prev_zy_a = curr_zy_a;
            prev_zx_b = curr_zx; prev_zy_b = curr_zy_b;
            curr_zx = curr_zx + z_step;
        }

        # DUAL CHANNEL EQ RACK
        pa_norm = (deck_a_pitch + 12.0) / 24.0; pa_str = (deck_a_pitch >= 0.0 ? "+" : "") + string(vmath.round(deck_a_pitch * 10.0) / 10.0) + "st";
        pb_norm = (deck_b_pitch + 12.0) / 24.0; pb_str = (deck_b_pitch >= 0.0 ? "+" : "") + string(vmath.round(deck_b_pitch * 10.0) / 10.0) + "st";

        draw_dj_knob("PITCH A", 100, 460, pa_norm, pa_str, COLOR_DECK_A, ha_pitch, active_knob == 1);
        draw_dj_knob("HIGH", 200, 460, (deck_a_eq_hi + 12.0) / 24.0, string(vmath.round(deck_a_eq_hi)) + "dB", COLOR_AMBER, ha_hi, active_knob == 2);
        draw_dj_knob("MID", 300, 460, (deck_a_eq_mid + 12.0) / 24.0, string(vmath.round(deck_a_eq_mid)) + "dB", COLOR_AMBER, ha_mid, active_knob == 3);
        draw_dj_knob("LOW", 400, 460, (deck_a_eq_low + 12.0) / 24.0, string(vmath.round(deck_a_eq_low)) + "dB", COLOR_AMBER, ha_low, active_knob == 4);
        draw_dj_knob("FILTER", 500, 460, (deck_a_filter + 1.0) / 2.0, string(vmath.round(deck_a_filter * 100.0)) + "%", COLOR_DECK_A, ha_flt, active_knob == 5);

        draw_dj_knob("PITCH B", 900, 460, pb_norm, pb_str, COLOR_DECK_B, hb_pitch, active_knob == 6);
        draw_dj_knob("HIGH", 1000, 460, (deck_b_eq_hi + 12.0) / 24.0, string(vmath.round(deck_b_eq_hi)) + "dB", COLOR_AMBER, hb_hi, active_knob == 7);
        draw_dj_knob("MID", 1100, 460, (deck_b_eq_mid + 12.0) / 24.0, string(vmath.round(deck_b_eq_mid)) + "dB", COLOR_AMBER, hb_mid, active_knob == 8);
        draw_dj_knob("LOW", 1200, 460, (deck_b_eq_low + 12.0) / 24.0, string(vmath.round(deck_b_eq_low)) + "dB", COLOR_AMBER, hb_low, active_knob == 9);
        draw_dj_knob("FILTER", 1300, 460, (deck_b_filter + 1.0) / 2.0, string(vmath.round(deck_b_filter * 100.0)) + "%", COLOR_DECK_B, hb_flt, active_knob == 10);

        # ====================================================================
        # CENTER MASTER STATUS & PIONEER EFX UNIT RACK CARD
        # ====================================================================
        vglib.rect(590, 400, 220, 300, COLOR_CARD);
        vglib.line(590, 400, 810, 400, (active_fx_unit != 0) ? COLOR_AMBER : COLOR_CARD_RIM);
        vglib.line(810, 400, 810, 700, COLOR_CARD_RIM);
        vglib.line(810, 700, 590, 700, COLOR_CARD_RIM);
        vglib.line(590, 700, 590, 400, COLOR_CARD_RIM);

        vglib.text_ex(vcr_font, "MASTER FX BUS", 645, 412, 11, vglib.rgba(160, 170, 185, 255));

        # LUFS Readout Header
        lufs_val = vaudio.get_lufs();
        vglib.text_ex(vcr_font, string(vmath.round(lufs_val)) + " LUFS", 660, 432, 14, COLOR_AMBER);

        # Real-time Master VU Meter Bars + Peak Hold Line
        rms_val = vaudio.get_rms();
        if (rms_val > peak_hold_rms) {
            peak_hold_rms = rms_val;
        } else {
            peak_hold_rms = peak_hold_rms - 0.005; # Slow decay
        }

        vglib.rect(610, 460, 180, 10, vglib.rgba(28, 32, 42, 255));
        vglib.rect(610, 460, rms_val * 180.0, 10, COLOR_DECK_A);
        
        peak_x :: Float64 = 610.0 + (peak_hold_rms * 180.0);
        vglib.rect(peak_x, 458, 2, 14, COLOR_DECK_B);

        # FX Unit Selectors
        vglib.line(600, 485, 800, 485, COLOR_CARD_RIM);
        vglib.text_ex(vcr_font, "EFFECT SELECTOR", 640, 498, 10, vglib.WHITE);

        btn_off_c  = (active_fx_unit == 0) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(35, 42, 54, 255);
        btn_comp_c = (active_fx_unit == 1) ? COLOR_AMBER : vglib.rgba(35, 42, 54, 255);
        btn_sat_c  = (active_fx_unit == 2) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(35, 42, 54, 255);
        btn_rev_c  = (active_fx_unit == 3) ? vglib.rgba(160, 90, 255, 255) : vglib.rgba(35, 42, 54, 255);

        vglib.rect(620, 520, 35, 25, btn_off_c);  vglib.text_ex(vcr_font, "OFF", 626, 528, 9, vglib.WHITE);
        vglib.rect(660, 520, 35, 25, btn_comp_c); vglib.text_ex(vcr_font, "CMP", 666, 528, 9, vglib.WHITE);
        vglib.rect(700, 520, 35, 25, btn_sat_c);  vglib.text_ex(vcr_font, "SAT", 706, 528, 9, vglib.WHITE);
        vglib.rect(740, 520, 35, 25, btn_rev_c);  vglib.text_ex(vcr_font, "REV", 746, 528, 9, vglib.WHITE);

        # Center Dedicated Dynamic FX Parameter Knob
        fx_norm :: Float64 = 0.0;
        fx_str  :: String  = "OFF";
        fx_col  = vglib.rgba(120, 130, 150, 255);

        if (active_fx_unit == 1) { fx_norm = (fx_comp_thresh + 30.0) / 30.0; fx_str = string(vmath.round(fx_comp_thresh)) + "dB"; fx_col = btn_comp_c; }
        if (active_fx_unit == 2) { fx_norm = fx_drive; fx_str = string(vmath.round(fx_drive * 100.0)) + "%"; fx_col = btn_sat_c; }
        if (active_fx_unit == 3) { fx_norm = fx_rev_mix; fx_str = string(vmath.round(fx_rev_mix * 100.0)) + "%"; fx_col = btn_rev_c; }

        draw_dj_knob("PARAM", 700, 620, fx_norm, fx_str, fx_col, h_fx_knob, active_knob == 12);

        # HOT CUE BANKS & CROSSFADER
        through p :: 0..7 -> loop {
            row :: Int64 = p / 4; col :: Int64 = p % 4;
            px :: Int64 = 30 + (col * 140); py :: Int64 = 580 + (row * 65);
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            draw_hot_cue_pad(p, px, py, 130, 55, COLOR_DECK_A, false, is_hov, cues_a[p]);
        };

        through p :: 0..7 -> loop {
            row :: Int64 = p / 4; col :: Int64 = p % 4;
            px :: Int64 = 820 + (col * 140); py :: Int64 = 580 + (row * 65);
            is_hov = (m[0] >= px && m[0] <= px + 130 && m[1] >= py && m[1] <= py + 55);
            draw_hot_cue_pad(p, px, py, 130, 55, COLOR_DECK_B, false, is_hov, cues_b[p]);
        };

        vglib.rect(500, 770, 400, 20, vglib.rgba(20, 24, 32, 255));
        vglib.line(500, 780, 900, 780, COLOR_CARD_RIM);
        vglib.line(700, 765, 700, 795, vglib.rgba(100, 110, 130, 255));

        handle_x :: Float64 = 500.0 + (crossfader_pos * 360.0);
        vglib.rect(handle_x, 755, 40, 50, vglib.rgba(220, 230, 245, 255));
        vglib.rect(handle_x + 18, 760, 4, 40, vglib.BLACK);

        vglib.text_ex(vcr_font, "DECK A", 500, 820, 11, COLOR_DECK_A);
        vglib.text_ex(vcr_font, "CROSSFADER", 652, 820, 11, vglib.WHITE);
        vglib.text_ex(vcr_font, "DECK B", 845, 820, 11, COLOR_DECK_B);

        vglib.text_ex(vcr_font, "VYNE AUDIO ENGINE v2.0.0 | DUAL DECK PRO DJ CONSOLE WITH MASTER DSP", 380, 860, 12, vglib.rgba(120, 130, 150, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();