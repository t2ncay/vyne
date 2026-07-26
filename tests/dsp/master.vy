ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

# Window Setup (1200x900 for full Master Rack resolution)
vglib.init(1200, 900, 60, "VYNE MASTER STUDIO RACK v1.0", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# Load Main Audio Track
track = vaudio.load_sound("tests/assets/fucking hardshit.wav");
vaudio.play_sound(track);

# --- ATTACH FULL MASTER DSP CHAIN ---
vaudio.attach_eq(track);
vaudio.attach_compressor(track);
vaudio.attach_saturator(track);
vaudio.attach_reverb(track);

run_time = 0.0;
active_tab :: Int64 = 0; # 0 = EQ, 1 = COMPRESSOR, 2 = SATURATOR, 3 = REVERB
prev_mouse_state = 0;

# --- EQUALIZER STATE ---
b1_f :: Float64 = 100.0;   b1_g :: Float64 = 3.0;   b1_q :: Float64 = 1.0;
b2_f :: Float64 = 1000.0;  b2_g :: Float64 = -4.0;  b2_q :: Float64 = 1.4;
b3_f :: Float64 = 3500.0;  b3_g :: Float64 = 5.0;   b3_q :: Float64 = 1.2;
b4_f :: Float64 = 10000.0; b4_g :: Float64 = 2.0;   b4_q :: Float64 = 0.8;
eq_on :: Int64 = 1;
active_eq_node = 0;

# --- COMPRESSOR STATE ---
thresh  :: Float64 = -18.0;
ratio   :: Float64 = 4.0;
attack  :: Float64 = 15.0;
release :: Float64 = 120.0;
makeup  :: Float64 = 3.0;
comp_on :: Int64   = 1;
active_comp_knob = 0;

# --- SATURATOR STATE ---
drive   :: Float64 = 0.45;
sat_mode:: Int64   = 0; # 0 = SOFT TUBE, 1 = HARD CLIP, 2 = ASYMMETRIC
sat_on  :: Int64   = 1;
active_sat_knob = 0;

# --- REVERB STATE ---
decay    :: Float64 = 0.85;
mix      :: Float64 = 0.35;
predelay :: Float64 = 20.0;
damping  :: Float64 = 0.30;
rev_on   :: Int64   = 1;
active_rev_knob = 0;

# --- UI HELPER FUNCTIONS ---
fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 39.0, vglib.rgba(45, 48, 58, 255));
    vglib.circle(x, y, 35.0, vglib.rgba(22, 24, 30, 255));
    vglib.circle(x, y, 22.0, vglib.rgba(32, 35, 44, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 31.0;
    line_y :: Float64 = y - vmath.cos(rad) * 31.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.5, color);
    
    vglib.text_ex(vcr_font, name, x - 26, y + 48, 13, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 20, y - 4, 11, color);
}

fn draw_bypass_button(x, y, is_active, active_color) {
    bg_color = vglib.rgba(35, 38, 48, 255);
    btn_color = vglib.rgba(220, 50, 50, 255);
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = active_color;
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 34, bg_color);
    vglib.rect(x + 2, y + 2, 106, 30, btn_color);
    vglib.text_ex(vcr_font, label, x + 18, y + 9, 13, vglib.BLACK);
}

# --- PRO-Q EQ HELPER FUNCTIONS ---
fn freq_to_x(f) {
    log_min = 1.30103; log_max = 4.30103;
    log_val = vmath.log(vmath.clamp(f, 20.0, 20000.0)) / 2.302585;
    norm = (log_val - log_min) / (log_max - log_min);
    return 80.0 + (norm * 1020.0);
}

fn x_to_freq(x) {
    log_min = 1.30103; log_max = 4.30103;
    norm = vmath.clamp((x - 80.0) / 1020.0, 0.0, 1.0);
    log_val = log_min + (norm * (log_max - log_min));
    return vmath.pow(10.0, log_val);
}

fn gain_to_y(g) {
    norm = vmath.clamp(g / 18.0, -1.0, 1.0);
    return 360.0 - (norm * 220.0);
}

fn y_to_gain(y) {
    norm = vmath.clamp((360.0 - y) / 220.0, -1.0, 1.0);
    return norm * 18.0;
}

fn draw_eq_node(id_str, x, y, color, is_selected) {
    radius = 12.0;
    if (is_selected) { radius = 16.0; }

    vglib.circle(x, y, radius + 3.0, vglib.BLACK);
    vglib.circle(x, y, radius, color);
    vglib.circle(x, y, radius - 4.0, vglib.rgba(18, 20, 26, 255));
    vglib.text_ex(vcr_font, id_str, x - 4.0, y - 5.0, 12, color);
}

# --- OPTO COMPRESSOR DISPLAY CARDS ---
fn draw_transfer_graph(x, y, t_dB, r_val, gr_db) {
    vglib.rect(x, y, 420, 240, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 420, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 420, y, x + 420, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 420, y + 240, x, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 240, x, y, vglib.rgba(50, 55, 68, 255));

    vglib.line(x + 210, y, x + 210, y + 240, vglib.rgba(30, 36, 48, 255));
    vglib.line(x, y + 120, x + 420, y + 120, vglib.rgba(30, 36, 48, 255));
    
    vglib.line(x + 20, y + 220, x + 400, y + 20, vglib.rgba(55, 62, 75, 255));

    t_norm = (t_dB + 40.0) / 40.0;
    knee_x :: Float64 = x + 20.0 + (t_norm * 380.0);
    knee_y :: Float64 = y + 220.0 - (t_norm * 200.0);

    vglib.line(x + 20.0, y + 220.0, knee_x, knee_y, vglib.rgba(255, 140, 40, 255));

    end_x :: Float64 = x + 400.0;
    compressed_rise = (200.0 * (1.0 - t_norm)) / r_val;
    end_y :: Float64 = knee_y - compressed_rise;

    vglib.line(knee_x, knee_y, end_x, end_y, vglib.rgba(255, 140, 40, 255));
    vglib.circle(knee_x, knee_y, 4.0, vglib.WHITE);

    if (gr_db > 0.5) {
        gr_norm = vmath.clamp(gr_db / 24.0, 0.0, 1.0);
        dot_x :: Float64 = knee_x + (gr_norm * (end_x - knee_x));
        dot_y :: Float64 = knee_y - (gr_norm * (knee_y - end_y));
        vglib.circle(dot_x, dot_y, 6.0, vglib.rgba(255, 60, 60, 255));
    }

    vglib.text_ex(vcr_font, "TRANSFER FUNCTION & DYNAMICS KNEE", x + 50, y + 252, 11, vglib.rgba(160, 170, 185, 255));
}

fn draw_vu_meter(x, y, gr_db, is_on) {
    vglib.rect(x, y, 320, 240, vglib.rgba(14, 16, 20, 255));
    vglib.line(x, y, x + 320, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 320, y, x + 320, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 320, y + 240, x, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 240, x, y, vglib.rgba(50, 55, 68, 255));

    face_col = (is_on == 1) ? vglib.rgba(240, 230, 190, 255) : vglib.rgba(80, 80, 75, 255);
    vglib.rect(x + 15, y + 15, 290, 180, face_col);

    vglib.text_ex(vcr_font, "0", x + 35, y + 35, 12, vglib.BLACK);
    vglib.text_ex(vcr_font, "-6", x + 105, y + 28, 12, vglib.BLACK);
    vglib.text_ex(vcr_font, "-12", x + 175, y + 28, 12, vglib.BLACK);
    vglib.text_ex(vcr_font, "-24", x + 245, y + 35, 12, vglib.BLACK);

    pivot_x :: Float64 = x + 160.0;
    pivot_y :: Float64 = y + 185.0;

    norm_gr = vmath.clamp(gr_db / 24.0, 0.0, 1.0);
    needle_angle = -45.0 + (norm_gr * 90.0);
    rad = vmath.radians(needle_angle);

    needle_x :: Float64 = pivot_x + vmath.sin(rad) * 140.0;
    needle_y :: Float64 = pivot_y - vmath.cos(rad) * 140.0;

    vglib.line(pivot_x, pivot_y, needle_x, needle_y, vglib.rgba(200, 30, 30, 255));
    vglib.circle(pivot_x, pivot_y, 8.0, vglib.BLACK);

    vglib.text_ex(vcr_font, "GAIN REDUCTION (dB)", x + 90, y + 210, 11, vglib.rgba(160, 170, 185, 255));
}

fn draw_output_rms_card(x, y, rms_val) {
    vglib.rect(x, y, 200, 240, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 200, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 200, y, x + 200, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 200, y + 240, x, y + 240, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 240, x, y, vglib.rgba(50, 55, 68, 255));

    vglib.text_ex(vcr_font, " 0dB", x + 15, y + 25, 10, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-12dB", x + 15, y + 75, 10, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-24dB", x + 15, y + 125, 10, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-48dB", x + 15, y + 175, 10, vglib.rgba(140, 150, 165, 255));

    vglib.rect(x + 90, y + 20, 30, 180, vglib.rgba(24, 28, 36, 255));
    vglib.rect(x + 135, y + 20, 30, 180, vglib.rgba(24, 28, 36, 255));

    rms_h = vmath.clamp(rms_val * 180.0, 0.0, 180.0);
    if (rms_h > 1.0) {
        vglib.rect(x + 90, (y + 200) - rms_h, 30, rms_h, vglib.rgba(50, 255, 120, 255));
        vglib.rect(x + 135, (y + 200) - rms_h, 30, rms_h, vglib.rgba(50, 255, 120, 255));
    }

    vglib.text_ex(vcr_font, "OUTPUT RMS", x + 55, y + 212, 11, vglib.rgba(50, 255, 120, 255));
}

# --- SATURATOR TRANSFER CURVE HELPER ---
fn draw_saturator_curve(x, y, d_val, m_val, is_on) {
    vglib.rect(x, y, 220, 220, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 220, y, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 220, y, x + 220, y + 220, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 220, y + 220, x, y + 220, vglib.rgba(50, 55, 65, 255));
    vglib.line(x, y + 220, x, y, vglib.rgba(50, 55, 65, 255));

    vglib.line(x + 110, y, x + 110, y + 220, vglib.rgba(32, 38, 48, 255));
    vglib.line(x, y + 110, x + 220, y + 110, vglib.rgba(32, 38, 48, 255));

    vglib.line(x + 10, y + 210, x + 210, y + 10, vglib.rgba(50, 55, 65, 255));

    prev_px :: Float64 = x + 10.0;
    prev_py :: Float64 = y + 110.0;
    gain = 1.0 + (d_val * 7.0);

    step = 4.0;
    curr_px :: Float64 = x + 10.0;

    while (curr_px <= x + 210.0) {
        norm_in = ((curr_px - (x + 10.0)) / 200.0 * 2.0) - 1.0;
        sample = norm_in * gain;

        if (is_on == 1) {
            if (m_val == 0) { sample = vmath.tanh(sample); }
            if (m_val == 1) { sample = vmath.clamp(sample, -0.7, 0.7) * 1.42; }
            if (m_val == 2) {
                if (sample > 0.0) { sample = vmath.tanh(sample); }
                else { sample = vmath.tanh(sample * 1.5) * 0.8; }
            }
        } else {
            sample = norm_in;
        }

        norm_out = vmath.clamp(sample * (1.0 / vmath.sqrt(gain)), -1.0, 1.0);
        curr_py :: Float64 = (y + 110.0) - (norm_out * 100.0);

        curve_color = (is_on == 1) ? vglib.rgba(255, 100, 40, 255) : vglib.rgba(80, 80, 95, 255);

        if (curr_px > x + 10.0) {
            vglib.line(prev_px, prev_py, curr_px, curr_py, curve_color);
        }

        prev_px = curr_px;
        prev_py = curr_py;
        curr_px = curr_px + step;
    }

    vglib.text_ex(vcr_font, "TRANSFER CURVE", x + 50, y + 232, 11, vglib.rgba(160, 170, 185, 255));
}

# --- REVERB 3D WIREFRAME ROOM VISUALIZER (FRUITY REEVERB 2 STYLE) ---
fn draw_reverb_room_3d(cx, cy, room_size, decay_val, t_time, is_on) {
    # Dark Backing Card
    vglib.rect(cx - 200, cy - 180, 400, 360, vglib.rgba(16, 18, 24, 255));
    
    # Outer Card Borders
    vglib.line(cx - 200, cy - 180, cx + 200, cy - 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(cx + 200, cy - 180, cx + 200, cy + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(cx + 200, cy + 180, cx - 200, cy + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(cx - 200, cy + 180, cx - 200, cy - 180, vglib.rgba(50, 55, 68, 255));

    glow_color = (is_on == 1) ? vglib.rgba(170, 100, 255, 255) : vglib.rgba(70, 75, 90, 255);
    dim_color  = (is_on == 1) ? vglib.rgba(120, 60, 200, 180) : vglib.rgba(45, 50, 60, 180);

    # 3D Room Cylinder Dimensions (Scaled by Size & Decay)
    r :: Float64 = 55.0 + (room_size * 55.0);
    h :: Float64 = 60.0 + (decay_val * 70.0);
    rot_angle = t_time * 0.8;

    num_pillars = 16;
    
    # Render Top and Bottom Ellipses & Vertical Pillars
    through i :: 0..15 -> loop {
        a1 = (i * 22.5) + rot_angle;
        a2 = ((i + 1) * 22.5) + rot_angle;

        rad1 = vmath.radians(a1);
        rad2 = vmath.radians(a2);

        # Perspective projection Math
        cos1 = vmath.cos(rad1); sin1 = vmath.sin(rad1);
        cos2 = vmath.cos(rad2); sin2 = vmath.sin(rad2);

        # Top Ring Coordinates
        top_x1 :: Float64 = cx + (cos1 * r);
        top_y1 :: Float64 = (cy - h) + (sin1 * r * 0.38);
        top_x2 :: Float64 = cx + (cos2 * r);
        top_y2 :: Float64 = (cy - h) + (sin2 * r * 0.38);

        # Bottom Ring Coordinates
        bot_x1 :: Float64 = cx + (cos1 * r);
        bot_y1 :: Float64 = (cy + h) + (sin1 * r * 0.38);
        bot_x2 :: Float64 = cx + (cos2 * r);
        bot_y2 :: Float64 = (cy + h) + (sin2 * r * 0.38);

        # Depth-based shading (back lines dimmer)
        wire_col = (sin1 < 0.0) ? dim_color : glow_color;

        # Draw Top Ring Segment
        vglib.line(top_x1, top_y1, top_x2, top_y2, wire_col);
        
        # Draw Bottom Ring Segment
        vglib.line(bot_x1, bot_y1, bot_x2, bot_y2, wire_col);

        # Draw Vertical Pillar Lines
        vglib.line(top_x1, top_y1, bot_x1, bot_y1, wire_col);
    };

    vglib.text_ex(vcr_font, "3D ACOUSTIC ROOM MODEL", cx - 95, cy + 192, 11, vglib.rgba(160, 170, 185, 255));
}

# --- REVERB IMPULSE DECAY ENVELOPE GRAPH ---
fn draw_reverb_decay_graph(x, y, dec_val, damp_val, mix_val, is_on) {
    vglib.rect(x, y, 420, 200, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 420, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 420, y, x + 420, y + 200, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 420, y + 200, x, y + 200, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 200, x, y, vglib.rgba(50, 55, 68, 255));

    vglib.line(x, y + 180, x + 420, y + 180, vglib.rgba(35, 42, 54, 255));

    line_col = (is_on == 1) ? vglib.rgba(160, 90, 255, 255) : vglib.rgba(80, 80, 95, 255);

    prev_px :: Float64 = x + 10.0;
    prev_py :: Float64 = y + 180.0;

    decay_factor = 1.0 + (dec_val * 8.0);
    damp_factor  = 1.0 + (damp_val * 5.0);

    step = 6.0;
    curr_px :: Float64 = x + 10.0;

    while (curr_px <= x + 410.0) {
        norm_t = (curr_px - (x + 10.0)) / 400.0;
        
        # Exponential impulse decay curve
        env_val = vmath.exp(-norm_t * (10.0 / decay_factor));
        reflections = vmath.sin(norm_t * 45.0 * damp_factor) * env_val * 0.25;
        
        tot_amp = (env_val + reflections) * mix_val;
        if (is_on == 0) { tot_amp = 0.0; }

        curr_py :: Float64 = (y + 180.0) - (vmath.clamp(tot_amp, 0.0, 1.0) * 150.0);

        if (curr_px > x + 10.0) {
            vglib.line(prev_px, prev_py, curr_px, curr_py, line_col);
        }

        prev_px = curr_px;
        prev_py = curr_py;
        curr_px = curr_px + step;
    }

    vglib.text_ex(vcr_font, "IMPULSE RESPONSE DECAY TAIL", x + 85, y + 212, 11, vglib.rgba(160, 170, 185, 255));
}

# --- MAIN RACK LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    # --- 1. RACK SWITCHING TAB INTERACTION ---
    if (mouse_click && prev_mouse_state == 0) {
        if (m[1] >= 20 && m[1] <= 60) {
            if (m[0] >= 50 && m[0] <= 200) { active_tab = 0; }  # EQ
            if (m[0] >= 210 && m[0] <= 360) { active_tab = 1; } # COMPRESSOR
            if (m[0] >= 370 && m[0] <= 520) { active_tab = 2; } # SATURATOR
            if (m[0] >= 530 && m[0] <= 680) { active_tab = 3; } # REVERB
        }
    }
    prev_mouse_state = mouse_click ? 1 : 0;

    # --- GLOBAL SPACEBAR MECHANIC ---
    if (vglib.key_pressed(vglib.SPACE)) {
        if (active_tab == 0) { eq_on = (eq_on == 1) ? 0 : 1; }
        if (active_tab == 1) { comp_on = (comp_on == 1) ? 0 : 1; }
        if (active_tab == 2) { sat_on = (sat_on == 1) ? 0 : 1; } 
        if (active_tab == 3) { rev_on = (rev_on == 1) ? 0 : 1; }
    }

    if (vglib.key_pressed(vglib.ENTER)) {
        if (active_tab == 2) { sat_mode = (sat_mode + 1) % 3; } # saturator cycle
    }

    # --- UPDATE ALL DSP PROCESSORS IN REAL-TIME ---
    vaudio.enable_eq(eq_on);
    vaudio.set_eq(0, b1_f, b1_g, b1_q);
    vaudio.set_eq(1, b2_f, b2_g, b2_q);
    vaudio.set_eq(2, b3_f, b3_g, b3_q);
    vaudio.set_eq(3, b4_f, b4_g, b4_q);

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, comp_on);
    
    # Pass drive and mode to C++ DSP (Drive set to 0 if bypassed)
    effective_drive = (sat_on == 1) ? drive : 0.0;
    vaudio.set_dsp(effective_drive, sat_mode);
    
    vaudio.set_reverb(decay, mix, rev_on);

    gr_db   = vaudio.get_gr();
    rms_val = vaudio.get_rms();

    vglib.begin();
        vglib.clear(vglib.rgba(12, 14, 18, 255)); # Studio Chassis Dark

        # --- TOP MASTER RACK TAB BAR ---
        c_eq   = (active_tab == 0) ? vglib.rgba(0, 220, 255, 255)   : vglib.rgba(30, 35, 45, 255);
        c_comp = (active_tab == 1) ? vglib.rgba(255, 120, 40, 255)  : vglib.rgba(30, 35, 45, 255);
        c_sat  = (active_tab == 2) ? vglib.rgba(255, 90, 90, 255)   : vglib.rgba(30, 35, 45, 255);
        c_rev  = (active_tab == 3) ? vglib.rgba(160, 90, 255, 255)  : vglib.rgba(30, 35, 45, 255);

        vglib.rect(50, 20, 150, 40, c_eq);
        vglib.text_ex(vcr_font, "1. PRO-Q EQ", 70, 32, 13, (active_tab == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(210, 20, 150, 40, c_comp);
        vglib.text_ex(vcr_font, "2. COMPRESSOR", 220, 32, 13, (active_tab == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(370, 20, 150, 40, c_sat);
        vglib.text_ex(vcr_font, "3. SATURATOR", 382, 32, 13, (active_tab == 2) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(530, 20, 150, 40, c_rev);
        vglib.text_ex(vcr_font, "4. REVERB", 552, 32, 13, (active_tab == 3) ? vglib.BLACK : vglib.WHITE);

        # Header Title
        vglib.text_ex(vcr_font, "VYNE MASTER STUDIO BUS", 780, 28, 20, vglib.WHITE);
        vglib.line(50, 65, 1150, 65, vglib.rgba(60, 70, 85, 255));

        # ====================================================================
        # RACK 1: PRO-Q EQUALIZER DISPLAY
        # ====================================================================
        if (active_tab == 0) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_eq_node == 0) {
                    if (vmath.hypot(m[0] - freq_to_x(b1_f), m[1] - gain_to_y(b1_g)) < 22) { active_eq_node = 1; }
                    if (vmath.hypot(m[0] - freq_to_x(b2_f), m[1] - gain_to_y(b2_g)) < 22) { active_eq_node = 2; }
                    if (vmath.hypot(m[0] - freq_to_x(b3_f), m[1] - gain_to_y(b3_g)) < 22) { active_eq_node = 3; }
                    if (vmath.hypot(m[0] - freq_to_x(b4_f), m[1] - gain_to_y(b4_g)) < 22) { active_eq_node = 4; }
                }

                if (active_eq_node == 1) { b1_f = x_to_freq(m[0]); b1_g = y_to_gain(m[1]); }
                if (active_eq_node == 2) { b2_f = x_to_freq(m[0]); b2_g = y_to_gain(m[1]); }
                if (active_eq_node == 3) { b3_f = x_to_freq(m[0]); b3_g = y_to_gain(m[1]); }
                if (active_eq_node == 4) { b4_f = x_to_freq(m[0]); b4_g = y_to_gain(m[1]); }
            } else { active_eq_node = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (eq_on == 1) { eq_on = 0; } else { eq_on = 1; }
                }
            }

            vglib.rect(80, 100, 1020, 520, vglib.rgba(18, 22, 30, 255));
            vglib.line(80, 100, 1100, 100, vglib.rgba(40, 48, 62, 255));
            vglib.line(1100, 100, 1100, 620, vglib.rgba(40, 48, 62, 255));
            vglib.line(1100, 620, 80, 620, vglib.rgba(40, 48, 62, 255));
            vglib.line(80, 620, 80, 100, vglib.rgba(40, 48, 62, 255));

            vglib.line(80, 360, 1100, 360, vglib.rgba(70, 82, 100, 255));
            vglib.text_ex(vcr_font, "0 dB", 35, 354, 10, vglib.rgba(120, 130, 150, 255));
            vglib.text_ex(vcr_font, "+18dB", 30, 100, 10, vglib.rgba(120, 130, 150, 255));
            vglib.text_ex(vcr_font, "-18dB", 30, 610, 10, vglib.rgba(120, 130, 150, 255));

            vglib.line(freq_to_x(100.0), 100, freq_to_x(100.0), 620, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "100Hz", freq_to_x(100.0) - 15, 628, 10, vglib.rgba(100, 110, 130, 255));

            vglib.line(freq_to_x(1000.0), 100, freq_to_x(1000.0), 620, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "1kHz", freq_to_x(1000.0) - 12, 628, 10, vglib.rgba(100, 110, 130, 255));

            vglib.line(freq_to_x(10000.0), 100, freq_to_x(10000.0), 620, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "10kHz", freq_to_x(10000.0) - 15, 628, 10, vglib.rgba(100, 110, 130, 255));

            prev_px :: Float64 = 80.0; prev_py :: Float64 = 360.0;
            curr_x :: Float64 = 80.0;
            while (curr_x <= 1100.0) {
                eval_f = x_to_freq(curr_x);
                d1 = vmath.log(eval_f / b1_f); g1 = b1_g * vmath.exp(-3.0 * d1 * d1 * b1_q);
                d2 = vmath.log(eval_f / b2_f); g2 = b2_g * vmath.exp(-3.0 * d2 * d2 * b2_q);
                d3 = vmath.log(eval_f / b3_f); g3 = b3_g * vmath.exp(-3.0 * d3 * d3 * b3_q);
                d4 = vmath.log(eval_f / b4_f); g4 = b4_g * vmath.exp(-3.0 * d4 * d4 * b4_q);
                tot = (eq_on == 1) ? (g1 + g2 + g3 + g4) : 0.0;
                curr_y = gain_to_y(tot);

                curve_color = (eq_on == 1) ? vglib.rgba(0, 230, 255, 255) : vglib.rgba(80, 90, 105, 255);

                if (curr_x > 80.0) { vglib.line(prev_px, prev_py, curr_x, curr_y, curve_color); }
                prev_px = curr_x; prev_py = curr_y; curr_x = curr_x + 8.0;
            }

            c1 = vglib.rgba(255, 80, 80, 255);
            c2 = vglib.rgba(255, 200, 50, 255);
            c3 = vglib.rgba(50, 220, 120, 255);
            c4 = vglib.rgba(180, 90, 255, 255);

            draw_eq_node("1", freq_to_x(b1_f), gain_to_y(b1_g), c1, active_eq_node == 1);
            draw_eq_node("2", freq_to_x(b2_f), gain_to_y(b2_g), c2, active_eq_node == 2);
            draw_eq_node("3", freq_to_x(b3_f), gain_to_y(b3_g), c3, active_eq_node == 3);
            draw_eq_node("4", freq_to_x(b4_f), gain_to_y(b4_g), c4, active_eq_node == 4);

            vglib.rect(80, 680, 230, 60, vglib.rgba(20, 24, 32, 255));
            vglib.text_ex(vcr_font, "BAND 1 (LOW)", 90, 688, 12, c1);
            vglib.text_ex(vcr_font, "F: " + string(vmath.round(b1_f)) + "Hz", 90, 706, 11, vglib.WHITE);
            vglib.text_ex(vcr_font, "G: " + string(vmath.round(b1_g)) + "dB", 90, 722, 11, vglib.WHITE);

            vglib.rect(340, 680, 230, 60, vglib.rgba(20, 24, 32, 255));
            vglib.text_ex(vcr_font, "BAND 2 (MID)", 350, 688, 12, c2);
            vglib.text_ex(vcr_font, "F: " + string(vmath.round(b2_f)) + "Hz", 350, 706, 11, vglib.WHITE);
            vglib.text_ex(vcr_font, "G: " + string(vmath.round(b2_g)) + "dB", 350, 722, 11, vglib.WHITE);

            vglib.rect(600, 680, 230, 60, vglib.rgba(20, 24, 32, 255));
            vglib.text_ex(vcr_font, "BAND 3 (HI-MID)", 610, 688, 12, c3);
            vglib.text_ex(vcr_font, "F: " + string(vmath.round(b3_f)) + "Hz", 610, 706, 11, vglib.WHITE);
            vglib.text_ex(vcr_font, "G: " + string(vmath.round(b3_g)) + "dB", 610, 722, 11, vglib.WHITE);

            vglib.rect(860, 680, 230, 60, vglib.rgba(20, 24, 32, 255));
            vglib.text_ex(vcr_font, "BAND 4 (AIR)", 870, 688, 12, c4);
            vglib.text_ex(vcr_font, "F: " + string(vmath.round(b4_f)) + "Hz", 870, 706, 11, vglib.WHITE);
            vglib.text_ex(vcr_font, "G: " + string(vmath.round(b4_g)) + "dB", 870, 722, 11, vglib.WHITE);

            draw_bypass_button(50, 810, eq_on, vglib.rgba(0, 220, 255, 255));
        }

        # ====================================================================
        # RACK 2: OPTO COMPRESSOR DISPLAY
        # ====================================================================
        if (active_tab == 1) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_comp_knob == 0) {
                    if (vmath.hypot(m[0] - 180, m[1] - 650) < 45) { active_comp_knob = 1; }
                    if (vmath.hypot(m[0] - 380, m[1] - 650) < 45) { active_comp_knob = 2; }
                    if (vmath.hypot(m[0] - 580, m[1] - 650) < 45) { active_comp_knob = 3; }
                    if (vmath.hypot(m[0] - 780, m[1] - 650) < 45) { active_comp_knob = 4; }
                    if (vmath.hypot(m[0] - 980, m[1] - 650) < 45) { active_comp_knob = 5; }
                }

                delta = md[1] * 0.3;
                if (active_comp_knob == 1) { thresh = vmath.clamp(thresh - delta, -40.0, 0.0); }
                if (active_comp_knob == 2) { ratio  = vmath.clamp(ratio - (delta * 0.1), 1.0, 16.0); }
                if (active_comp_knob == 3) { attack = vmath.clamp(attack - delta, 1.0, 100.0); }
                if (active_comp_knob == 4) { release = vmath.clamp(release - (delta * 2.0), 10.0, 500.0); }
                if (active_comp_knob == 5) { makeup = vmath.clamp(makeup - (delta * 0.1), 0.0, 12.0); }
            } else { active_comp_knob = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (comp_on == 1) { comp_on = 0; } else { comp_on = 1; }
                }
            }

            draw_transfer_graph(80, 140, thresh, ratio, gr_db);
            draw_vu_meter(530, 140, gr_db, comp_on);
            draw_output_rms_card(880, 140, rms_val);

            t_norm   = (thresh + 40.0) / 40.0;
            r_norm   = (ratio - 1.0) / 15.0;
            a_norm   = (attack - 1.0) / 99.0;
            rel_norm = (release - 10.0) / 490.0;
            m_norm   = makeup / 12.0;

            orange_glow = (comp_on == 1) ? vglib.rgba(255, 130, 40, 255) : vglib.rgba(90, 90, 100, 255);

            draw_knob("THRESH", 180, 650, t_norm, string(vmath.round(thresh)) + "dB", orange_glow);
            draw_knob("RATIO", 380, 650, r_norm, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255));
            draw_knob("ATTACK", 580, 650, a_norm, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255));
            draw_knob("RELEASE", 780, 650, rel_norm, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255));
            draw_knob("MAKEUP", 980, 650, m_norm, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255));

            draw_bypass_button(50, 810, comp_on, vglib.rgba(255, 120, 40, 255));
        }

        # ====================================================================
        # RACK 3: SATURATOR DISPLAY
        # ====================================================================
        if (active_tab == 2) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_sat_knob == 0) {
                    if (vmath.hypot(m[0] - 220, m[1] - 300) < 55) { active_sat_knob = 1; }
                }

                delta = md[1] * 0.3;
                if (active_sat_knob == 1) { drive = vmath.clamp(drive - (delta * 0.01), 0.0, 1.0); }
            } else {
                active_sat_knob = 0;
            }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 480 && m[0] <= 640 && m[1] >= 220 && m[1] <= 255) { sat_mode = 0; }
                if (m[0] >= 480 && m[0] <= 640 && m[1] >= 270 && m[1] <= 305) { sat_mode = 1; }
                if (m[0] >= 480 && m[0] <= 640 && m[1] >= 320 && m[1] <= 355) { sat_mode = 2; }

                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (sat_on == 1) { sat_on = 0; } else { sat_on = 1; }
                }
            }

            sat_orange = (sat_on == 1) ? vglib.rgba(255, 100, 40, 255) : vglib.rgba(90, 90, 100, 255);
            draw_knob("DRIVE", 220, 300, drive, string(vmath.round(drive * 100.0)) + "%", sat_orange);

            vglib.rect(460, 180, 200, 195, vglib.rgba(22, 26, 34, 255));
            vglib.text_ex(vcr_font, "SATURATION MODE", 480, 195, 12, vglib.WHITE);

            btn0_col = (sat_mode == 0) ? sat_orange : vglib.rgba(40, 45, 58, 255);
            btn1_col = (sat_mode == 1) ? sat_orange : vglib.rgba(40, 45, 58, 255);
            btn2_col = (sat_mode == 2) ? sat_orange : vglib.rgba(40, 45, 58, 255);

            vglib.rect(480, 220, 160, 35, btn0_col);
            vglib.text_ex(vcr_font, "SOFT TUBE", 510, 232, 12, (sat_mode == 0) ? vglib.BLACK : vglib.WHITE);

            vglib.rect(480, 270, 160, 35, btn1_col);
            vglib.text_ex(vcr_font, "HARD CLIP", 510, 282, 12, (sat_mode == 1) ? vglib.BLACK : vglib.WHITE);

            vglib.rect(480, 320, 160, 35, btn2_col);
            vglib.text_ex(vcr_font, "ASYMMETRIC", 502, 332, 12, (sat_mode == 2) ? vglib.BLACK : vglib.WHITE);

            # 3. Live Non-Linear Transfer Curve
            draw_saturator_curve(720, 155, drive, sat_mode, sat_on);

            # 4. Waveform Visualizer Bars (Lowered to match y=600 card)
            vglib.rect(80, 600, 1020, 70, vglib.rgba(22, 26, 34, 255));
            
            through i :: 0..100 -> loop {
                x_p = 90 + (i * 10);
                wave_in = vmath.sin(run_time * 8.0 + i * 0.15);
                
                if (sat_on == 1) {
                    if (sat_mode == 0) { wave_in = vmath.tanh(wave_in * (1.0 + drive * 5.0)); }
                    if (sat_mode == 1) { wave_in = vmath.clamp(wave_in * (1.0 + drive * 5.0), -0.7, 0.7); }
                    if (sat_mode == 2) {
                        if (wave_in > 0.0) { wave_in = vmath.tanh(wave_in * (1.0 + drive * 5.0)); }
                        else { wave_in = vmath.tanh(wave_in * (1.0 + drive * 7.5)) * 0.8; }
                    }
                }
                
                h = vmath.abs(wave_in) * 28.0;
                vglib.rect(x_p, 635 - h, 6, h * 2.0, sat_orange); # Centered vertically at Y = 635
            };

            draw_bypass_button(50, 810, sat_on, vglib.rgba(255, 100, 40, 255));
        }

        # ====================================================================
        # RACK 4: SPATIAL REVERB DISPLAY (FRUITY REEVERB 2 DESIGN)
        # ====================================================================
        if (active_tab == 3) {
            # Knob Dragging Interaction
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_rev_knob == 0) {
                    if (vmath.hypot(m[0] - 180, m[1] - 650) < 45) { active_rev_knob = 1; }
                    if (vmath.hypot(m[0] - 380, m[1] - 650) < 45) { active_rev_knob = 2; }
                    if (vmath.hypot(m[0] - 580, m[1] - 650) < 45) { active_rev_knob = 3; }
                    if (vmath.hypot(m[0] - 780, m[1] - 650) < 45) { active_rev_knob = 4; }
                }

                delta = md[1] * 0.3;
                if (active_rev_knob == 1) { decay    = vmath.clamp(decay - (delta * 0.01), 0.0, 0.95); }
                if (active_rev_knob == 2) { mix      = vmath.clamp(mix - (delta * 0.01), 0.0, 1.0); }
                if (active_rev_knob == 3) { predelay = vmath.clamp(predelay - delta, 0.0, 100.0); }
                if (active_rev_knob == 4) { damping  = vmath.clamp(damping - (delta * 0.01), 0.0, 1.0); }
            } else { active_rev_knob = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (rev_on == 1) { rev_on = 0; } else { rev_on = 1; }
                }
            }

            # 1. Fruity Reeverb 2 Style 3D Wireframe Room Visualizer
            size_norm = predelay / 100.0; # Map predelay/size parameter
            draw_reverb_room_3d(300, 300, size_norm, decay, run_time, rev_on);

            # 2. Reverb Impulse Response Decay Curve
            draw_reverb_decay_graph(620, 200, decay, damping, mix, rev_on);

            # 3. Parameter Controls (Bottom Row)
            purple_glow = (rev_on == 1) ? vglib.rgba(160, 90, 255, 255) : vglib.rgba(90, 90, 100, 255);

            draw_knob("DECAY", 180, 650, decay / 0.95, string(vmath.round(decay * 100.0)) + "%", purple_glow);
            draw_knob("MIX", 380, 650, mix, string(vmath.round(mix * 100.0)) + "%", vglib.rgba(60, 220, 255, 255));
            draw_knob("PRE-DELAY", 580, 650, predelay / 100.0, string(vmath.round(predelay)) + "ms", vglib.rgba(255, 200, 50, 255));
            draw_knob("DAMPING", 780, 650, damping, string(vmath.round(damping * 100.0)) + "%", vglib.rgba(255, 90, 120, 255));

            draw_bypass_button(50, 810, rev_on, vglib.rgba(160, 90, 255, 255));
        }

        # --- GLOBAL MASTER STEREO OUTPUT METER (FAR RIGHT EDGE) ---
        vglib.rect(1150, 100, 20, 680, vglib.rgba(20, 24, 32, 255));
        m_rms = vmath.clamp(rms_val * 680.0, 0.0, 680.0);
        if (m_rms > 1.0) {
            vglib.rect(1150, 780 - m_rms, 20, m_rms, vglib.rgba(50, 255, 120, 255));
        }
        vglib.text_ex(vcr_font, "MASTER OUT", 1100, 820, 11, vglib.rgba(50, 255, 120, 255));

        # Footer
        vglib.text_ex(vcr_font, "Vyne Studio Engine v1.0.0", 900, 850, 12, vglib.rgba(150, 160, 180, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();