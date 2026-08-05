ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";

vglib.init(1400, 900, 60, "VYNE SAMPLER CONSOLE v2.0", 0);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# Load Audio Track
track = vaudio.play_stream(configs.Audios.ikit1bb);
vaudio.attach_bpm(track);

# --- COLOR PALETTE DEFINITIONS (MATCHED TO DJ DECK CONSOLE) ---
COLOR_BG       = vglib.rgba(10, 11, 15, 255);    # Deep Obsidian
COLOR_CARD     = vglib.rgba(18, 22, 30, 255);    # Panel Cards
COLOR_CARD_RIM = vglib.rgba(40, 48, 64, 255);    # Subtle Borders
COLOR_DECK_A   = vglib.rgba(0, 240, 255, 255);   # Electric Cyan
COLOR_DECK_B   = vglib.rgba(255, 45, 120, 255);  # Neon Sunset Pink
COLOR_AMBER    = vglib.rgba(255, 170, 0, 255);   # FX / Parameter Amber

run_time              :: Float64 = 0.0;
prev_mouse_state      :: Int64   = 0;
smooth_lufs_intensity :: Float64 = 0.0;

# --- SAMPLER ENGINE STATE ---
pitch_shift    :: Float64 = 0.0;   # -12 to +12 semitones
speed_rate     :: Float64 = 1.0;   # 0.5x to 2.0x time stretch
attack_ms      :: Float64 = 5.0;   # Envelope attack
release_ms     :: Float64 = 150.0; # Envelope release
slice_mode     :: Int64   = 1;     # 0 = MANUAL, 1 = BPM CHOP (10 PADS), 2 = TRANSIENT
active_pad     :: Int64   = 0;     # 0 to 9 active pad
active_knob    :: Int64   = 0;     # Dragged parameter knob

track_duration :: Float64 = 180.0; # Total track length in seconds
playhead_pos   :: Float64 = 0.0;   # Normalized playhead position [0.0 .. 1.0]

# --- 10-PAD SLICE BANK ARRAYS ---
num_pads  :: Int64 = 10;
pad_start :: Array = [];
pad_end   :: Array = [];

# Initial Equal Slice Division for 10 Pads
through p :: 0..9 -> loop {
    start_p :: Float64 = p * (1.0 / float64(num_pads));
    end_p   :: Float64 = (p + 1) * (1.0 / float64(num_pads));
    pad_start.push(start_p);
    pad_end.push(end_p);
};

# --- DYNAMIC WAVEFORM PEAK BUFFER ---
wave_peaks :: Array = [];
through i :: 0..299 -> loop {
    val = (vmath.sin(i * 0.08) * 0.4) + (vmath.cos(i * 0.23) * 0.35) + (vmath.sin(i * 0.51) * 0.25);
    wave_peaks.push(vmath.abs(val));
};

pad_colors :: Array = [
    vglib.rgba(255, 70, 70, 255),   vglib.rgba(255, 130, 40, 255),
    COLOR_AMBER,                    vglib.rgba(255, 230, 50, 255),
    vglib.rgba(0, 200, 240, 255),   COLOR_DECK_A,
    vglib.rgba(120, 120, 255, 255), COLOR_DECK_B,
    vglib.rgba(180, 90, 255, 255),  vglib.rgba(255, 100, 200, 255)
];

# --- REFACTORED UI HELPER: ROTARY KNOB WITH SOLID ARC TRACKING ---
fn draw_sampler_knob(name, x, y, val_norm, display_val, color, is_hovered, is_active) {
    radius_base = 36.0;
    if (is_active) { radius_base = 38.0; } # Tighter active state, zero hover growth

    # Outer Housing
    vglib.circle(x, y, radius_base, vglib.BLACK);
    
    # Outer Active Ring
    if (is_active) {
        vglib.circle(x, y, radius_base - 1.5, color);
    } else {
        vglib.circle(x, y, radius_base - 2.0, vglib.rgba(35, 40, 52, 255));
    }

    # Inner Knob Face
    vglib.circle(x, y, 28.0, vglib.rgba(16, 18, 24, 255));
    vglib.circle(x, y, 18.0, vglib.rgba(26, 30, 38, 255));
    
    # Solid Value Arc (Continuous Radial Band)
    arc_steps :: Int64 = int64(val_norm * 40.0);
    if (arc_steps > 0) {
        prev_ax :: Float64 = x + vmath.sin(vmath.radians(-135.0)) * 23.0;
        prev_ay :: Float64 = y - vmath.cos(vmath.radians(-135.0)) * 23.0;

        through a :: 1..40 -> loop {
            if (a <= arc_steps) {
                ang = ((a / 40.0) * 270.0) - 135.0;
                rad = vmath.radians(ang);
                curr_ax :: Float64 = x + vmath.sin(rad) * 23.0;
                curr_ay :: Float64 = y - vmath.cos(rad) * 23.0;

                vglib.line(prev_ax, prev_ay, curr_ax, curr_ay, color);
                prev_ax = curr_ax;
                prev_ay = curr_ay;
            }
        };
    }

    # Center Pointer Needle
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 25.0;
    line_y :: Float64 = y - vmath.cos(rad) * 25.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.0, color);
    
    label_col = is_hovered ? COLOR_DECK_A : vglib.rgba(160, 170, 185, 255);
    vglib.text_ex(vcr_font, name, x - 22, y + 42, 11, label_col);
    vglib.text_ex(vcr_font, display_val, x - 18, y - 4, 10, color);
}

# --- REFACTORED UI HELPER: MPC PAD BUTTON ---
fn draw_mpc_pad(id, x, y, w, h, pad_color, is_active, is_hovered, start_t, end_t) {
    bg_col   = is_active ? pad_color : vglib.rgba(20, 24, 32, 255);
    rim_col  = is_active ? vglib.WHITE : (is_hovered ? pad_color : COLOR_CARD_RIM);
    text_col = is_active ? vglib.BLACK : vglib.WHITE;

    vglib.rect(x, y, w, h, bg_col);
    vglib.line(x, y, x + w, y, rim_col);
    vglib.line(x + w, y, x + w, y + h, rim_col);
    vglib.line(x + w, y + h, x, y + h, rim_col);
    vglib.line(x, y + h, x, y, rim_col);

    pad_str = "PAD " + string(id + 1);
    vglib.text_ex(vcr_font, pad_str, x + 10, y + 10, 12, text_col);

    start_sec = vmath.round(start_t * track_duration * 10.0) / 10.0;
    end_sec   = vmath.round(end_t * track_duration * 10.0) / 10.0;
    range_str = string(start_sec) + "s - " + string(end_sec) + "s";
    
    sub_col = is_active ? vglib.BLACK : vglib.rgba(140, 150, 165, 255);
    vglib.text_ex(vcr_font, range_str, x + 10, y + 30, 9, sub_col);

    if (is_active) {
        vglib.rect(x + w - 40, y + 8, 32, 14, vglib.BLACK);
        vglib.text_ex(vcr_font, "PLAY", x + w - 36, y + 10, 8, COLOR_DECK_A);
    }
}

# --- MAIN SAMPLER ENGINE RENDER LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    # 1. UPDATE STREAM
    vaudio.update_stream(track);

    # Get Dynamic BPM
    detected_bpm :: Float64 = vaudio.get_bpm(track);
    if (detected_bpm <= 0.0) { detected_bpm = 130.0; }

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    # 2. UPDATE PLAYHEAD & LOOP BOUNDARIES PER ACTIVE SLICE
    playhead_pos = playhead_pos + (0.0003 * speed_rate);
    if (playhead_pos >= pad_end[active_pad]) {
        playhead_pos = pad_start[active_pad];
        start_seconds :: Float64 = pad_start[active_pad] * track_duration;
        vaudio.seek_stream(track, start_seconds);
        vaudio.resume_stream(track);
    }

    # --- MOUSE CLICK DETECTIONS ---
    if (mouse_click && prev_mouse_state == 0) {
        # 1. Slice Mode Switching Bar
        if (m[1] >= 20 && m[1] <= 52) {
            if (m[0] >= 800 && m[0] <= 920) { slice_mode = 0; }
            if (m[0] >= 930 && m[0] <= 1050) {
                slice_mode = 1;
                
                sec_per_beat   :: Float64 = 60.0 / detected_bpm;
                beats_per_pad  :: Float64 = 4.0; # 1 bar per chop (4 beats)
                pad_len_sec    :: Float64 = beats_per_pad * sec_per_beat;
                pad_len_norm   :: Float64 = pad_len_sec / track_duration;

                through p :: 0..9 -> loop {
                    st_norm :: Float64 = p * pad_len_norm;
                    en_norm :: Float64 = (p + 1) * pad_len_norm;
                    if (en_norm > 1.0) { en_norm = 1.0; }
                    
                    pad_start[p] = st_norm;
                    pad_end[p]   = en_norm;
                };
            }
            if (m[0] >= 1060 && m[0] <= 1180) { slice_mode = 2; }
        }

        # 2. Clicking Main Overview Waveform
        if (m[0] >= 50 && m[0] <= 1350 && m[1] >= 80 && m[1] <= 240) {
            norm_click = (m[0] - 50.0) / 1300.0;
            playhead_pos = norm_click;
            
            through p :: 0..9 -> loop {
                if (norm_click >= pad_start[p] && norm_click <= pad_end[p]) {
                    active_pad = p;
                }
            };

            target_sec :: Float64 = playhead_pos * track_duration;
            vaudio.seek_stream(track, target_sec);
            vaudio.resume_stream(track);
        }

        # 3. MPC 10-PAD GRID INTERACTION (2 Rows of 5)
        pad_w :: Int64 = 250;
        pad_h :: Int64 = 85;
        through p :: 0..9 -> loop {
            row :: Int64 = p / 5;
            col :: Int64 = p % 5;
            px :: Int64 = 50 + (col * 262);
            py :: Int64 = 580 + (row * 105);

            if (m[0] >= px && m[0] <= px + pad_w && m[1] >= py && m[1] <= py + pad_h) {
                active_pad = p;
                playhead_pos = pad_start[p];
                
                start_seconds :: Float64 = pad_start[p] * track_duration;
                vaudio.seek_stream(track, start_seconds);
                vaudio.resume_stream(track);
            }
        };
    }

    # --- KNOB DRAGGING INTERACTION ---
    hk1 = vmath.hypot(m[0] - 120, m[1] - 470) < 38;
    hk2 = vmath.hypot(m[0] - 260, m[1] - 470) < 38;
    hk3 = vmath.hypot(m[0] - 400, m[1] - 470) < 38;
    hk4 = vmath.hypot(m[0] - 540, m[1] - 470) < 38;

    if (mouse_click) {
        if (active_knob == 0) {
            if (hk1) { active_knob = 1; }
            if (hk2) { active_knob = 2; }
            if (hk3) { active_knob = 3; }
            if (hk4) { active_knob = 4; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { pitch_shift = vmath.clamp(pitch_shift - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 2) { speed_rate  = vmath.clamp(speed_rate - (delta * 0.01), 0.5, 2.0); }
        if (active_knob == 3) { attack_ms   = vmath.clamp(attack_ms - delta, 0.1, 100.0); }
        if (active_knob == 4) { release_ms  = vmath.clamp(release_ms - (delta * 2.0), 10.0, 1000.0); }
    } else {
        active_knob = 0;
    }

    prev_mouse_state = mouse_click ? 1 : 0;

    vaudio.set_pitch(track, vmath.pow(2.0, pitch_shift / 12.0));

    vglib.begin();
        vglib.clear(COLOR_BG);

        # HEADER BAR
        vglib.text_ex(vcr_font, "VYNE SERATO SAMPLER", 50, 26, 18, vglib.WHITE);
        bpm_str = "KEY: Fm | BPM: " + string(vmath.round(detected_bpm * 10.0) / 10.0);
        vglib.text_ex(vcr_font, bpm_str, 300, 28, 12, COLOR_DECK_A);

        mode0_col = (slice_mode == 0) ? COLOR_DECK_A : vglib.rgba(30, 36, 48, 255);
        mode1_col = (slice_mode == 1) ? COLOR_DECK_A : vglib.rgba(30, 36, 48, 255);
        mode2_col = (slice_mode == 2) ? COLOR_DECK_A : vglib.rgba(30, 36, 48, 255);

        vglib.rect(800, 20, 120, 32, mode0_col);
        vglib.text_ex(vcr_font, "MANUAL", 830, 30, 11, (slice_mode == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(930, 20, 120, 32, mode1_col);
        vglib.text_ex(vcr_font, "CHOP (10)", 950, 30, 11, (slice_mode == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(1060, 20, 120, 32, mode2_col);
        vglib.text_ex(vcr_font, "TRANSIENT", 1076, 30, 11, (slice_mode == 2) ? vglib.BLACK : vglib.WHITE);

        vglib.line(50, 62, 1350, 62, COLOR_CARD_RIM);

        # MODULE 1: FULL TRACK OVERVIEW WAVEFORM
        vglib.rect(50, 80, 1300, 160, COLOR_CARD);
        vglib.line(50, 80, 1350, 80, COLOR_CARD_RIM);
        vglib.line(1350, 80, 1350, 240, COLOR_CARD_RIM);
        vglib.line(1350, 240, 50, 240, COLOR_CARD_RIM);
        vglib.line(50, 240, 50, 80, COLOR_CARD_RIM);

        through p :: 0..9 -> loop {
            p_st :: Float64 = pad_start[p];
            p_en :: Float64 = pad_end[p];

            rx :: Float64 = 50.0 + (p_st * 1300.0);
            rw :: Float64 = (p_en - p_st) * 1300.0;
            p_col = pad_colors[p];

            if (p == active_pad) {
                vglib.rect(rx, 81, rw, 158, vglib.rgba(255, 255, 255, 25));
            }

            vglib.line(rx, 80, rx, 240, p_col);
            vglib.rect(rx, 80, 20, 16, p_col);
            vglib.text_ex(vcr_font, string(p + 1), rx + 6, 83, 10, vglib.BLACK);
        };

        num_peaks :: Float64 = wave_peaks.length();
        step_w    :: Float64 = 1300.0 / num_peaks;
        cy        :: Float64 = 160.0;

        through idx :: 0..(int64(num_peaks) - 1) -> loop {
            pk = wave_peaks[idx];
            px :: Float64 = 50.0 + (idx * step_w);
            ph :: Float64 = pk * 65.0;

            norm_x = idx / num_peaks;
            peak_col = vglib.rgba(120, 135, 160, 180);
            through p :: 0..9 -> loop {
                if (norm_x >= pad_start[p] && norm_x <= pad_end[p]) {
                    peak_col = pad_colors[p];
                }
            };

            vglib.rect(px, cy - ph, step_w - 0.5, ph * 2.0, peak_col);
        };

        ph_x :: Float64 = 50.0 + (playhead_pos * 1300.0);
        vglib.line(ph_x, 80, ph_x, 240, vglib.WHITE);
        vglib.circle(ph_x, 80, 5.0, COLOR_DECK_A);

        # MODULE 2: ZOOMED ACTIVE SLICE WAVEFORM (CRT GLOW EFFECT)
        vglib.rect(50, 260, 1300, 140, vglib.rgba(8, 10, 14, 255));
        vglib.line(50, 260, 1350, 260, COLOR_CARD_RIM);
        vglib.line(1350, 260, 1350, 400, COLOR_CARD_RIM);
        vglib.line(1350, 400, 50, 400, COLOR_CARD_RIM);
        vglib.line(50, 400, 50, 260, COLOR_CARD_RIM);

        act_col = pad_colors[active_pad];
        vglib.text_ex(vcr_font, "ZOOMED SLICE VIEW - PAD " + string(active_pad + 1), 65, 272, 11, act_col);

        prev_zx :: Float64 = 50.0;
        prev_zy :: Float64 = 330.0;
        z_step  :: Float64 = 10.0;
        curr_zx :: Float64 = 50.0;

        while (curr_zx <= 1350.0) {
            norm_z = (curr_zx - 50.0) / 1300.0;
            z_amp  = vmath.sin(norm_z * 40.0 + run_time * 5.0) * vmath.cos(norm_z * 12.0) * 50.0;
            curr_zy :: Float64 = 330.0 - z_amp;

            if (curr_zx > 50.0) {
                # Phosphor Glow Overlay
                vglib.line(prev_zx, prev_zy - 1.0, curr_zx, curr_zy - 1.0, vglib.rgba(0, 180, 220, 60));
                vglib.line(prev_zx, prev_zy + 1.0, curr_zx, curr_zy + 1.0, vglib.rgba(0, 180, 220, 60));
                # Core Line
                vglib.line(prev_zx, prev_zy, curr_zx, curr_zy, act_col);
            }
            prev_zx = curr_zx;
            prev_zy = curr_zy;
            curr_zx = curr_zx + z_step;
        }

        vglib.line(50, 330, 1350, 330, vglib.rgba(35, 42, 54, 255));

        # MODULE 3: CONTROLS
        p_norm   = (pitch_shift + 12.0) / 24.0;
        s_norm   = (speed_rate - 0.5) / 1.5;
        a_norm   = (attack_ms - 0.1) / 99.9;
        r_norm   = (release_ms - 10.0) / 990.0;

        p_str = (pitch_shift >= 0.0 ? "+" : "") + string(vmath.round(pitch_shift * 10.0) / 10.0) + "st";
        s_str = string(vmath.round(speed_rate * 100.0) / 100.0) + "x";
        a_str = string(vmath.round(attack_ms)) + "ms";
        r_str = string(vmath.round(release_ms)) + "ms";

        draw_sampler_knob("PITCH", 120, 470, p_norm, p_str, COLOR_DECK_A, hk1, active_knob == 1);
        draw_sampler_knob("SPEED", 260, 470, s_norm, s_str, COLOR_AMBER, hk2, active_knob == 2);
        draw_sampler_knob("ATTACK", 400, 470, a_norm, a_str, COLOR_DECK_A, hk3, active_knob == 3);
        draw_sampler_knob("RELEASE", 540, 470, r_norm, r_str, COLOR_DECK_B, hk4, active_knob == 4);

        vglib.rect(680, 425, 670, 95, COLOR_CARD);
        vglib.line(680, 425, 1350, 425, COLOR_CARD_RIM);
        vglib.line(1350, 425, 1350, 520, COLOR_CARD_RIM);
        vglib.line(1350, 520, 680, 520, COLOR_CARD_RIM);
        vglib.line(680, 520, 680, 425, COLOR_CARD_RIM);

        vglib.text_ex(vcr_font, "ACTIVE CUE INFO", 700, 440, 11, vglib.rgba(160, 170, 185, 255));
        st_val = vmath.round(pad_start[active_pad] * track_duration * 100.0) / 100.0;
        en_val = vmath.round(pad_end[active_pad] * track_duration * 100.0) / 100.0;
        
        info_str = "PAD " + string(active_pad + 1) + " | START: " + string(st_val) + "s | END: " + string(en_val) + "s";
        vglib.text_ex(vcr_font, info_str, 700, 462, 13, act_col);
        vglib.text_ex(vcr_font, "ALGORITHM: VYNE TIME-STRETCH v1.0 (FORMANT PRESERVED)", 700, 490, 10, vglib.rgba(100, 110, 130, 255));

        # MODULE 4: MPC 10-PAD GRID
        pad_w :: Int64 = 250;
        pad_h :: Int64 = 85;

        through p :: 0..9 -> loop {
            row :: Int64 = p / 5;
            col :: Int64 = p % 5;

            px :: Int64 = 50 + (col * 262);
            py :: Int64 = 580 + (row * 105);

            is_act = (p == active_pad) && (vaudio.is_stream_playing(track) == 1);
            is_hov = (m[0] >= px && m[0] <= px + pad_w && m[1] >= py && m[1] <= py + pad_h);
            
            draw_mpc_pad(p, px, py, pad_w, pad_h, pad_colors[p], is_act, is_hov, pad_start[p], pad_end[p]);
        };

        vglib.text_ex(vcr_font, "VYNE AUDIO ENGINE v2.0.0 | PRO SAMPLER & TIME-STRETCH ENGINE", 380, 860, 12, vglib.rgba(120, 130, 150, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();