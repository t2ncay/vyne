ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";

# --- 1. INITIALIZE STUDIO ENGINE & WINDOW ---
vglib.init(1600, 950, 60, "VYNE DAW PRO STUDIO v3.0", 0);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# --- 2. GLOBAL TIMELINE & TRANSPORT STATE ---
bpm          :: Float64 = 124.0;
playhead_sec :: Float64 = 0.0;
is_playing   :: Int64   = 0;
zoom_px_sec  :: Float64 = 45.0; # Pixels per second timeline scale
scroll_x     :: Float64 = 0.0;
run_time     :: Float64 = 0.0;

# Render Notification Toast State
is_rendering  :: Int64   = 0;
render_timer  :: Float64 = 0.0;
render_status :: Int64   = 0;

# --- 3. MULTI-TRACK AUDIO ENGINE & CLIP POSITIONS ---
num_tracks :: Int64 = 4;
track_names = ["VOCAL LEAD", "GUITAR RIG", "SYNTH BASS", "DRUM BUS"];
track_colors = [
    vglib.rgba(0, 240, 255, 255),  # Cyan
    vglib.rgba(255, 170, 0, 255),  # Amber
    vglib.rgba(255, 45, 120, 255), # Pink
    vglib.rgba(50, 255, 120, 255)  # Green
];

# Load Audio Track Streams
track_streams :: Array = [
    vaudio.play_stream(configs.Audios.never_fade_away),
    vaudio.play_stream(configs.Audios.ikit1bb),
    vaudio.play_stream(configs.Audios.eternal_sunshine),
    vaudio.play_stream(configs.Audios.cigerlerim)
];

# Track Stream Activation States (Fixes Seek-Thrashing Glitches)
clip_active :: Array = [0, 0, 0, 0];

# Pre-extract Waveform Peak Buffers for Timeline Visualization
wave_peaks_0 :: Array = vaudio.get_waveform(track_streams[0], 120);
wave_peaks_1 :: Array = vaudio.get_waveform(track_streams[1], 120);
wave_peaks_2 :: Array = vaudio.get_waveform(track_streams[2], 120);
wave_peaks_3 :: Array = vaudio.get_waveform(track_streams[3], 120);
track_waveforms :: Array = [wave_peaks_0, wave_peaks_1, wave_peaks_2, wave_peaks_3];

# Track Mixer States
track_gains :: Array = [0.85, 0.75, 0.90, 0.80];
track_pans  :: Array = [0.0, -0.25, 0.15, 0.0];
track_mutes :: Array = [0, 0, 0, 0];

# Clip Timeline Positioning (In Seconds) & Durations
clip_starts :: Array = [0.0, 2.5, 5.0, 1.2];   # X-Axis Offsets in seconds
clip_durs   :: Array = [12.0, 10.0, 14.0, 8.0]; # Clip Durations in seconds

# Mouse Dragging Handles
dragged_clip   :: Int64   = -1;
drag_offset_x  :: Float64 = 0.0;

# Attach Hardware DSP Chains & True Peak Limiter to Every Track
through t :: 0..3 -> loop {
    vaudio.attach_eq(track_streams[t]);
    vaudio.attach_compressor(track_streams[t]);
    vaudio.attach_saturator(track_streams[t]);
    vaudio.attach_reverb(track_streams[t]);
    vaudio.attach_master_limiter(track_streams[t]); # Attached master true-peak limiter
    
    # Start all streams paused
    vaudio.pause_stream(track_streams[t]);
};

# --- 4. FL MASTER RACK / DOCKED CHANNEL FX BUS STATE ---
active_tab     :: Int64 = 0; # 0 = PRO-Q EQ, 1 = OPTO COMP, 2 = SATURATOR, 3 = REVERB
selected_track :: Int64 = 0; # Currently focused track channel for FX editing
active_knob    :: Int64 = 0; # Active dragged UI knob ID (1..5)
active_eq_node :: Int64 = 0; # Active dragged EQ node handle ID (1..7)
prev_mouse_state :: Int64 = 0;

# EQUALIZER (7 BANDS)
b1_f :: Float64 = 60.0;    b1_g :: Float64 = 0.0;   b1_q :: Float64 = 1.0;
b2_f :: Float64 = 180.0;   b2_g :: Float64 = 2.5;   b2_q :: Float64 = 1.4;
b3_f :: Float64 = 500.0;   b3_g :: Float64 = -3.0;  b3_q :: Float64 = 1.2;
b4_f :: Float64 = 1200.0;  b4_g :: Float64 = 1.5;   b4_q :: Float64 = 1.0;
b5_f :: Float64 = 3000.0;  b5_g :: Float64 = -2.0;  b5_q :: Float64 = 1.0;
b6_f :: Float64 = 7500.0;  b6_g :: Float64 = 3.0;   b6_q :: Float64 = 0.8;
b7_f :: Float64 = 14000.0; b7_g :: Float64 = 1.0;   b7_q :: Float64 = 0.7;
eq_on :: Int64 = 1;

# COMPRESSOR STATE
thresh  :: Float64 = -16.0;
ratio   :: Float64 = 4.0;
attack  :: Float64 = 15.0;
release :: Float64 = 120.0;
makeup  :: Float64 = 2.5;
comp_on :: Int64   = 1;
auto_makeup :: Int64 = 1;

# SATURATOR STATE
drive    :: Float64 = 0.22;
sat_mode :: Int64   = 0;
sat_on   :: Int64   = 1;

# REVERB STATE
decay    :: Float64 = 0.75;
mix      :: Float64 = 0.30;
predelay :: Float64 = 18.0;
damping  :: Float64 = 0.35;
rev_on   :: Int64   = 1;

# Peak Meter Holding
peak_hold :: Float64 = 0.0;

# --- 5. UI HELPER FUNCTIONS ---
fn freq_to_x(f, min_x, width) {
    log_min = 1.30103; log_max = 4.30103;
    log_val = vmath.log(vmath.clamp(f, 20.0, 20000.0)) / 2.302585;
    norm = (log_val - log_min) / (log_max - log_min);
    return min_x + (norm * width);
}

fn x_to_freq(x, min_x, width) {
    log_min = 1.30103; log_max = 4.30103;
    norm = vmath.clamp((x - min_x) / width, 0.0, 1.0);
    log_val = log_min + (norm * (log_max - log_min));
    return vmath.pow(10.0, log_val);
}

fn gain_to_y(g, center_y, range_y) {
    norm = vmath.clamp(g / 18.0, -1.0, 1.0);
    return center_y - (norm * range_y);
}

fn y_to_gain(y, center_y, range_y) {
    norm = vmath.clamp((center_y - y) / range_y, -1.0, 1.0);
    return norm * 18.0;
}

fn draw_knob(name, x, y, val_norm, display_val, color, is_active) {
    radius = 34.0;
    vglib.circle(x, y, radius, vglib.BLACK);
    vglib.circle(x, y, radius - 2.0, is_active ? color : vglib.rgba(40, 44, 56, 255));
    vglib.circle(x, y, 26.0, vglib.rgba(20, 22, 28, 255));
    vglib.circle(x, y, 16.0, vglib.rgba(28, 32, 40, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    line_x :: Float64 = x + vmath.sin(rad) * 23.0;
    line_y :: Float64 = y - vmath.cos(rad) * 23.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.0, color);
    vglib.text_ex(vcr_font, name, x - 22, y + 40, 10, vglib.rgba(160, 170, 185, 255));
    vglib.text_ex(vcr_font, display_val, x - 18, y - 4, 9, color);
}

fn draw_eq_node(id_str, x, y, color, is_selected) {
    radius = is_selected ? 14.0 : 10.0;
    vglib.circle(x, y, radius + 2.0, vglib.BLACK);
    vglib.circle(x, y, radius, color);
    vglib.circle(x, y, radius - 3.0, vglib.rgba(18, 20, 26, 255));
    vglib.text_ex(vcr_font, id_str, x - 3.0, y - 4.0, 10, color);
}

# --- 6. MAIN ENGINE LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    # Global Hotkeys
    if (vglib.key_pressed(vglib.SPACE)) {
        is_playing = (is_playing == 1) ? 0 : 1;
        if (is_playing == 0) {
            through t :: 0..3 -> loop {
                vaudio.pause_stream(track_streams[t]);
                clip_active[t] = 0;
            };
        }
    }

    if (vglib.key_pressed(vglib.R)) {
        is_rendering = 1;
        render_status = vaudio.render_offline(configs.Audios.never_fade_away, "master_daw_mix.wav") ? 1 : -1;
        render_timer = 3.0;
    }

    # Advance Global Timeline Playhead
    if (is_playing == 1) { 
        playhead_sec = playhead_sec + 0.016; 
    }

    # --- REAL-TIME PLAYHEAD & CLEAN CLIP TRIGGERING EVALUATION ---
    through t :: 0..3 -> loop {
        vaudio.update_stream(track_streams[t]);

        c_start = clip_starts[t];
        c_end   = c_start + clip_durs[t];

        if (is_playing == 1 && track_mutes[t] == 0) {
            # Check if playhead is inside clip bounds
            if (playhead_sec >= c_start && playhead_sec <= c_end) {
                rel_pos_sec = playhead_sec - c_start;
                
                # Seek & Resume ONLY ONCE when playhead enters clip bounds
                if (clip_active[t] == 0) {
                    vaudio.seek_stream(track_streams[t], rel_pos_sec);
                    vaudio.resume_stream(track_streams[t]);
                    clip_active[t] = 1;
                }
            } else {
                if (clip_active[t] == 1) {
                    vaudio.pause_stream(track_streams[t]);
                    clip_active[t] = 0;
                }
            }
        } else {
            if (clip_active[t] == 1) {
                vaudio.pause_stream(track_streams[t]);
                clip_active[t] = 0;
            }
        }

        # Track Volume Gain (Full volume output; Master Limiter handles peak protection)
        vol = (track_mutes[t] == 1) ? 0.0 : track_gains[t];
        vaudio.sound_volume(track_streams[t], vol);
    };

    # Mouse Wheel Zoom & Scroll
    wheel = vglib.mouse_wheel();
    if (wheel != 0.0 && m[1] < 500) {
        scroll_x = vmath.clamp(scroll_x - (wheel * 35.0), 0.0, 5000.0);
    }

    # --- APPLY DSP PARAMETERS TO MASTER BUS ---
    vaudio.enable_eq(eq_on);
    vaudio.set_eq(0, b1_f, b1_g, b1_q);
    vaudio.set_eq(1, b2_f, b2_g, b2_q);
    vaudio.set_eq(2, b3_f, b3_g, b3_q);
    vaudio.set_eq(3, b4_f, b4_g, b4_q);
    vaudio.set_eq(4, b5_f, b5_g, b5_q);
    vaudio.set_eq(5, b6_f, b6_g, b6_q);
    vaudio.set_eq(6, b7_f, b7_g, b7_q);

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, comp_on, auto_makeup);
    vaudio.set_dsp(sat_on == 1 ? drive : 0.0, sat_mode);
    vaudio.set_reverb(decay, mix, predelay, damping, rev_on);

    rms_val = vaudio.get_rms();
    lufs_val = vaudio.get_lufs();
    gr_db = vaudio.get_gr();

    peak_hold = (rms_val > peak_hold) ? rms_val : vmath.clamp(peak_hold - 0.005, 0.0, 1.0);

    # --- MOUSE INTERACTION & FULL PLUGIN CONTROLLER HANDLER ---
    if (mouse_click) {
        if (prev_mouse_state == 0) {
            # Transport Buttons
            if (m[1] >= 10 && m[1] <= 42) {
                if (m[0] >= 220 && m[0] <= 290) {
                    is_playing = (is_playing == 1) ? 0 : 1;
                    if (is_playing == 0) {
                        through t :: 0..3 -> loop { 
                            vaudio.pause_stream(track_streams[t]); 
                            clip_active[t] = 0;
                        };
                    }
                }
                if (m[0] >= 300 && m[0] <= 370) {
                    is_playing = 0; playhead_sec = 0.0;
                    through t :: 0..3 -> loop {
                        vaudio.pause_stream(track_streams[t]);
                        vaudio.seek_stream(track_streams[t], 0.0);
                        clip_active[t] = 0;
                    };
                }
            }

            # Interactive BPM Adjustment Buttons
            if (m[1] >= 10 && m[1] <= 42) {
                if (m[0] >= 560 && m[0] <= 585) { bpm = vmath.clamp(bpm - 1.0, 40.0, 240.0); }
                if (m[0] >= 590 && m[0] <= 615) { bpm = vmath.clamp(bpm + 1.0, 40.0, 240.0); }
            }

            # Track Selectors & Mute Toggles & Track Gain Dragging
            through t :: 0..3 -> loop {
                ty :: Float64 = 80.0 + (t * 105.0);
                if (m[0] <= 220 && m[1] >= ty && m[1] <= ty + 100.0) { selected_track = t; }
                if (m[0] >= 168 && m[0] <= 204 && m[1] >= ty + 36.0 && m[1] <= ty + 58.0) {
                    track_mutes[t] = (track_mutes[t] == 1) ? 0 : 1;
                }
                if (m[0] >= 12 && m[0] <= 152 && m[1] >= ty + 35.0 && m[1] <= ty + 55.0) {
                    track_gains[t] = vmath.clamp((m[0] - 12.0) / 140.0, 0.0, 1.0);
                }
            };

            # Check Clip Selection for Moving Along X-Axis
            through t :: 0..3 -> loop {
                ty :: Float64 = 80.0 + (t * 105.0);
                cx :: Float64 = 220.0 + (clip_starts[t] * zoom_px_sec) - scroll_x;
                cw :: Float64 = clip_durs[t] * zoom_px_sec;

                if (m[0] >= cx && m[0] <= cx + cw && m[1] >= ty + 6.0 && m[1] <= ty + 94.0) {
                    dragged_clip = t;
                    drag_offset_x = m[0] - cx;
                }
            };

            # Timeline Seek Click
            if (dragged_clip == -1 && m[0] >= 220 && m[1] >= 50 && m[1] <= 500) {
                playhead_sec = vmath.clamp((m[0] - 220.0 + scroll_x) / zoom_px_sec, 0.0, 1000.0);
                # Force reset active clips so streams re-seek cleanly on manual jump
                through t :: 0..3 -> loop { clip_active[t] = 0; };
            }

            # Master Rack Tab Switcher
            if (m[1] >= 515 && m[1] <= 551) {
                if (m[0] >= 240 && m[0] <= 380) { active_tab = 0; }
                if (m[0] >= 390 && m[0] <= 530) { active_tab = 1; }
                if (m[0] >= 540 && m[0] <= 680) { active_tab = 2; }
                if (m[0] >= 690 && m[0] <= 830) { active_tab = 3; }
            }

            # --- KNOB & PLUGIN HITBOX CLICK DETECTOR ---
            if (m[1] >= 560) {
                # TAB 0: EQ NODES SELECTION
                if (active_tab == 0) {
                    if (vmath.abs(m[0] - freq_to_x(b1_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(0.0, 720.0, 140.0)) < 18) { active_eq_node = 1; }
                    if (vmath.abs(m[0] - freq_to_x(b2_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b2_g, 720.0, 140.0)) < 18) { active_eq_node = 2; }
                    if (vmath.abs(m[0] - freq_to_x(b3_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b3_g, 720.0, 140.0)) < 18) { active_eq_node = 3; }
                    if (vmath.abs(m[0] - freq_to_x(b4_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b4_g, 720.0, 140.0)) < 18) { active_eq_node = 4; }
                    if (vmath.abs(m[0] - freq_to_x(b5_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b5_g, 720.0, 140.0)) < 18) { active_eq_node = 5; }
                    if (vmath.abs(m[0] - freq_to_x(b6_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b6_g, 720.0, 140.0)) < 18) { active_eq_node = 6; }
                    if (vmath.abs(m[0] - freq_to_x(b7_f, 240, 1330)) < 18 && vmath.abs(m[1] - gain_to_y(b7_g, 720.0, 140.0)) < 18) { active_eq_node = 7; }
                }

                # TAB 1: COMPRESSOR KNOBS
                if (active_tab == 1) {
                    if (vmath.abs(m[0] - 320) < 35 && vmath.abs(m[1] - 640) < 35) { active_knob = 1; }
                    if (vmath.abs(m[0] - 480) < 35 && vmath.abs(m[1] - 640) < 35) { active_knob = 2; }
                    if (vmath.abs(m[0] - 640) < 35 && vmath.abs(m[1] - 640) < 35) { active_knob = 3; }
                    if (vmath.abs(m[0] - 800) < 35 && vmath.abs(m[1] - 640) < 35) { active_knob = 4; }
                    if (vmath.abs(m[0] - 960) < 35 && vmath.abs(m[1] - 640) < 35) { active_knob = 5; }
                }

                # TAB 2: SATURATOR KNOBS & MODES
                if (active_tab == 2) {
                    if (vmath.abs(m[0] - 340) < 35 && vmath.abs(m[1] - 660) < 35) { active_knob = 1; }
                    through m_idx :: 0..4 -> loop {
                        bx :: Int64 = 520 + (m_idx * 160);
                        if (m[0] >= bx && m[0] <= bx + 140 && m[1] >= 640 && m[1] <= 680) {
                            sat_mode = m_idx;
                        }
                    };
                }

                # TAB 3: REVERB KNOBS
                if (active_tab == 3) {
                    if (vmath.abs(m[0] - 340) < 35 && vmath.abs(m[1] - 660) < 35) { active_knob = 1; }
                    if (vmath.abs(m[0] - 520) < 35 && vmath.abs(m[1] - 660) < 35) { active_knob = 2; }
                    if (vmath.abs(m[0] - 700) < 35 && vmath.abs(m[1] - 660) < 35) { active_knob = 3; }
                    if (vmath.abs(m[0] - 880) < 35 && vmath.abs(m[1] - 660) < 35) { active_knob = 4; }
                }
            }
        }

        # --- CONTINUOUS DRAG LOGIC FOR CLIPS, EQ & KNOBS ---
        if (dragged_clip != -1) {
            new_cx :: Float64 = m[0] - drag_offset_x;
            new_start_sec :: Float64 = (new_cx - 220.0 + scroll_x) / zoom_px_sec;
            clip_starts[dragged_clip] = vmath.clamp(new_start_sec, 0.0, 500.0);
        }

        # Drag EQ Bands Dynamically Across Canvas
        if (active_eq_node > 0) {
            new_f = x_to_freq(m[0], 240.0, 1330.0);
            new_g = y_to_gain(m[1], 720.0, 140.0);

            if (active_eq_node == 1) { b1_f = vmath.clamp(new_f, 20.0, 200.0); }
            if (active_eq_node == 2) { b2_f = vmath.clamp(new_f, 60.0, 500.0); b2_g = new_g; }
            if (active_eq_node == 3) { b3_f = vmath.clamp(new_f, 200.0, 1500.0); b3_g = new_g; }
            if (active_eq_node == 4) { b4_f = vmath.clamp(new_f, 500.0, 4000.0); b4_g = new_g; }
            if (active_eq_node == 5) { b5_f = vmath.clamp(new_f, 1500.0, 8000.0); b5_g = new_g; }
            if (active_eq_node == 6) { b6_f = vmath.clamp(new_f, 4000.0, 15000.0); b6_g = new_g; }
            if (active_eq_node == 7) { b7_f = vmath.clamp(new_f, 8000.0, 20000.0); b7_g = new_g; }
        }

        # Drag Active Knob via Vertical Mouse Delta
        if (active_knob > 0) {
            dy = md[1];

            # COMPRESSOR TAB KNOB ADJUSTMENTS
            if (active_tab == 1) {
                if (active_knob == 1) { thresh  = vmath.clamp(thresh - (dy * 0.2), -40.0, 0.0); }
                if (active_knob == 2) { ratio   = vmath.clamp(ratio - (dy * 0.1), 1.0, 16.0); }
                if (active_knob == 3) { attack  = vmath.clamp(attack - (dy * 0.5), 1.0, 100.0); }
                if (active_knob == 4) { release = vmath.clamp(release - (dy * 2.0), 10.0, 500.0); }
                if (active_knob == 5) { makeup  = vmath.clamp(makeup - (dy * 0.1), 0.0, 12.0); }
            }

            # SATURATOR TAB KNOB ADJUSTMENT
            if (active_tab == 2) {
                if (active_knob == 1) { drive = vmath.clamp(drive - (dy * 0.005), 0.0, 1.0); }
            }

            # REVERB TAB KNOB ADJUSTMENTS
            if (active_tab == 3) {
                if (active_knob == 1) { decay    = vmath.clamp(decay - (dy * 0.005), 0.0, 0.95); }
                if (active_knob == 2) { mix      = vmath.clamp(mix - (dy * 0.005), 0.0, 1.0); }
                if (active_knob == 3) { predelay = vmath.clamp(predelay - (dy * 0.5), 0.0, 100.0); }
                if (active_knob == 4) { damping  = vmath.clamp(damping - (dy * 0.005), 0.0, 1.0); }
            }
        }
    } else {
        dragged_clip   = -1;
        active_knob    = 0;
        active_eq_node = 0;
    }

    prev_mouse_state = mouse_click ? 1 : 0;

    vglib.begin();
        vglib.clear(vglib.rgba(10, 11, 15, 255));

        # ====================================================================
        # TOP WORKSTATION TRANSPORT CONTROL BAR (Y: 0..50)
        # ====================================================================
        vglib.rect(0, 0, 1600, 50, vglib.rgba(16, 20, 28, 255));
        vglib.line(0, 50, 1600, 50, vglib.rgba(40, 48, 64, 255));

        vglib.text_ex(vcr_font, "VYNE FL DAW PRO", 20, 16, 16, vglib.WHITE);
        
        # Play / Stop Buttons
        p_col = (is_playing == 1) ? vglib.rgba(0, 240, 255, 255) : vglib.rgba(35, 42, 54, 255);
        vglib.rect(220, 10, 70, 32, p_col);
        vglib.text_ex(vcr_font, is_playing == 1 ? "PAUSE" : "PLAY", 235, 20, 11, is_playing == 1 ? vglib.BLACK : vglib.WHITE);

        vglib.rect(300, 10, 70, 32, vglib.rgba(35, 42, 54, 255));
        vglib.text_ex(vcr_font, "STOP", 318, 20, 11, vglib.WHITE);

        # Digital Time Readout & Interactive BPM Control
        min_val = int64(playhead_sec / 60.0);
        sec_val = int64(vmath.fmod(playhead_sec, 60.0));
        time_str = (min_val < 10 ? "0" : "") + string(min_val) + ":" + (sec_val < 10 ? "0" : "") + string(sec_val);
        vglib.text_ex(vcr_font, "TIME: " + time_str, 390, 18, 12, vglib.rgba(255, 170, 0, 255));

        vglib.rect(560, 10, 25, 32, vglib.rgba(35, 42, 54, 255));
        vglib.text_ex(vcr_font, "-", 568, 20, 12, vglib.WHITE);
        vglib.rect(590, 10, 25, 32, vglib.rgba(35, 42, 54, 255));
        vglib.text_ex(vcr_font, "+", 598, 20, 12, vglib.WHITE);
        vglib.text_ex(vcr_font, string(vmath.round(bpm)) + " BPM", 625, 18, 12, vglib.WHITE);

        # Master Output LUFS Header Badge
        lufs_col = (lufs_val > -9.0) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(50, 255, 120, 255);
        vglib.text_ex(vcr_font, "MASTER: " + string(vmath.round(lufs_val * 10.0) / 10.0) + " LUFS", 1380, 18, 12, lufs_col);

        # ====================================================================
        # MULTI-TRACK PLAYLIST ARRANGER GRID (Y: 50..500)
        # ====================================================================
        vglib.rect(220, 50, 1380, 25, vglib.rgba(14, 16, 22, 255));
        vglib.line(220, 75, 1600, 75, vglib.rgba(40, 48, 64, 255));

        sec_per_bar :: Float64 = (60.0 / bpm) * 4.0;
        px_per_bar  :: Float64 = sec_per_bar * zoom_px_sec;

        through bar :: 0..60 -> loop {
            bx :: Float64 = 220.0 + (bar * px_per_bar) - scroll_x;
            if (bx >= 220.0 && bx <= 1600.0) {
                vglib.line(bx, 60, bx, 75, vglib.rgba(80, 90, 110, 255));
                vglib.text_ex(vcr_font, string(bar + 1), bx + 4, 58, 9, vglib.rgba(140, 150, 170, 255));
            }
        };

        # Render Track Channel Strips & Movable Audio Clips
        through t :: 0..3 -> loop {
            ty :: Float64 = 80.0 + (t * 105.0);
            t_col = track_colors[t];
            is_focus = (selected_track == t);

            # Left Channel Control Panel
            vglib.rect(0, ty, 220, 100, is_focus ? vglib.rgba(24, 30, 42, 255) : vglib.rgba(18, 22, 30, 255));
            vglib.line(0, ty + 100, 220, ty + 100, vglib.rgba(35, 42, 54, 255));
            vglib.line(0, ty, 0, ty + 100, t_col);

            vglib.text_ex(vcr_font, track_names[t], 12, ty + 14, 11, is_focus ? vglib.WHITE : vglib.rgba(180, 190, 205, 255));

            # Track Volume Gain Bar
            vglib.rect(12, ty + 42, 140, 6, vglib.rgba(30, 36, 48, 255));
            vglib.rect(12, ty + 42, track_gains[t] * 140.0, 6, t_col);

            # Mute Toggle Button
            m_btn_col = (track_mutes[t] == 1) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(40, 48, 60, 255);
            vglib.rect(168, ty + 36, 36, 22, m_btn_col);
            vglib.text_ex(vcr_font, "MUTE", 172, ty + 43, 8, vglib.WHITE);

            # Right Timeline Clip Lane
            vglib.rect(220, ty, 1380, 100, vglib.rgba(12, 14, 18, 255));
            vglib.line(220, ty + 100, 1600, ty + 100, vglib.rgba(25, 30, 40, 255));

            # Render Movable Waveform Clip Card
            clip_x :: Float64 = 220.0 + (clip_starts[t] * zoom_px_sec) - scroll_x;
            clip_w :: Float64 = clip_durs[t] * zoom_px_sec;

            if (clip_x + clip_w > 220.0 && clip_x < 1600.0) {
                draw_x :: Float64 = (clip_x < 220.0) ? 220.0 : clip_x;
                draw_w :: Float64 = clip_w - (draw_x - clip_x);

                is_dragged = (dragged_clip == t);
                clip_bg = is_dragged ? vglib.rgba(35, 45, 65, 255) : vglib.rgba(22, 28, 38, 255);
                vglib.rect(draw_x, ty + 6.0, draw_w, 88.0, clip_bg);
                vglib.line(draw_x, ty + 6.0, draw_x + draw_w, ty + 6.0, t_col);

                # Render Clip Waveform Peaks
                wave_data = track_waveforms[t];
                num_pk = wave_data.length();
                if (num_pk > 0) {
                    step_w :: Float64 = clip_w / float64(num_pk);
                    through idx :: 0..(int64(num_pk) - 1) -> loop {
                        pk = wave_data[idx];
                        px :: Float64 = clip_x + (idx * step_w);
                        if (px >= 220.0 && px <= 1600.0) {
                            ph :: Float64 = pk * 36.0;
                            vglib.rect(px, ty + 50.0 - ph, step_w - 0.5, ph * 2.0, t_col);
                        }
                    };
                }

                vglib.text_ex(vcr_font, track_names[t] + ".WAV", draw_x + 8.0, ty + 14.0, 9, vglib.WHITE);
            }
        };

        # Draw Global Playhead Indicator Line
        ph_px :: Float64 = 220.0 + (playhead_sec * zoom_px_sec) - scroll_x;
        if (ph_px >= 220.0 && ph_px <= 1600.0) {
            vglib.line(ph_px, 50, ph_px, 500, vglib.WHITE);
            vglib.circle(ph_px, 50, 4.0, vglib.rgba(255, 45, 120, 255));
        }

        # ====================================================================
        # DOCKED MASTER FX BUS RACK & MIXER (Y: 500..950)
        # ====================================================================
        vglib.rect(0, 500, 1600, 450, vglib.rgba(14, 16, 22, 255));
        vglib.line(0, 500, 1600, 500, vglib.rgba(50, 60, 80, 255));

        # --- LEFT MASTER MIXER STRIP (X: 0..220) ---
        vglib.rect(10, 515, 200, 420, vglib.rgba(18, 22, 30, 255));
        vglib.line(10, 515, 210, 515, vglib.rgba(255, 170, 0, 255));
        vglib.text_ex(vcr_font, "MASTER STRIP", 55, 528, 11, vglib.WHITE);

        # Stereo VU Meters
        meter_x :: Float64 = 35.0; meter_y :: Float64 = 555.0; meter_h :: Float64 = 320.0;
        vglib.rect(meter_x, meter_y, 22, meter_h, vglib.rgba(24, 28, 36, 255));
        vglib.rect(meter_x + 28, meter_y, 22, meter_h, vglib.rgba(24, 28, 36, 255));

        m_rms = vmath.clamp(rms_val * meter_h, 0.0, meter_h);
        if (m_rms > 1.0) {
            bar_y = (meter_y + meter_h) - m_rms;
            bar_col = (rms_val > 0.85) ? vglib.rgba(255, 60, 60, 255) : vglib.rgba(50, 255, 120, 255);
            vglib.rect(meter_x, bar_y, 22, m_rms, bar_col);
            vglib.rect(meter_x + 28, bar_y, 22, m_rms, bar_col);
        }

        # Peak Hold Line
        peak_y = (meter_y + meter_h) - (peak_hold * meter_h);
        vglib.line(meter_x, peak_y, meter_x + 50, peak_y, vglib.WHITE);

        # Offline Bounce Info Label
        vglib.text_ex(vcr_font, "PRESS 'R' BOUNCE WAV", 25, 890, 9, vglib.rgba(255, 170, 0, 255));

        # --- RIGHT FX RACK TAB HEADER (X: 240..1600) ---
        c_eq   = (active_tab == 0) ? vglib.rgba(0, 240, 255, 255)  : vglib.rgba(30, 36, 48, 255);
        c_comp = (active_tab == 1) ? vglib.rgba(255, 130, 40, 255) : vglib.rgba(30, 36, 48, 255);
        c_sat  = (active_tab == 2) ? vglib.rgba(255, 90, 90, 255)  : vglib.rgba(30, 36, 48, 255);
        c_rev  = (active_tab == 3) ? vglib.rgba(180, 90, 255, 255) : vglib.rgba(30, 36, 48, 255);

        vglib.rect(240, 515, 140, 36, c_eq);
        vglib.text_ex(vcr_font, "PRO-Q EQ", 270, 527, 11, (active_tab == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(390, 515, 140, 36, c_comp);
        vglib.text_ex(vcr_font, "COMPRESSOR", 412, 527, 11, (active_tab == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(540, 515, 140, 36, c_sat);
        vglib.text_ex(vcr_font, "SATURATOR", 565, 527, 11, (active_tab == 2) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(690, 515, 140, 36, c_rev);
        vglib.text_ex(vcr_font, "REVERB", 728, 527, 11, (active_tab == 3) ? vglib.BLACK : vglib.WHITE);

        vglib.text_ex(vcr_font, "TARGET: " + track_names[selected_track], 1340, 527, 12, track_colors[selected_track]);

        # ====================================================================
        # RACK TAB 0: PRO-Q 7-BAND EQUALIZER DISPLAY
        # ====================================================================
        if (active_tab == 0) {
            vglib.rect(240, 560, 1330, 375, vglib.rgba(18, 22, 30, 255));
            vglib.line(240, 720, 1570, 720, vglib.rgba(70, 82, 100, 255));

            # Grid Frequency Lines
            vglib.line(freq_to_x(100.0, 240, 1330), 560, freq_to_x(100.0, 240, 1330), 935, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "100Hz", freq_to_x(100.0, 240, 1330) - 15, 920, 9, vglib.rgba(120, 130, 150, 255));

            vglib.line(freq_to_x(1000.0, 240, 1330), 560, freq_to_x(1000.0, 240, 1330), 935, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "1kHz", freq_to_x(1000.0, 240, 1330) - 12, 920, 9, vglib.rgba(120, 130, 150, 255));

            vglib.line(freq_to_x(10000.0, 240, 1330), 560, freq_to_x(10000.0, 240, 1330), 935, vglib.rgba(32, 38, 50, 255));
            vglib.text_ex(vcr_font, "10kHz", freq_to_x(10000.0, 240, 1330) - 15, 920, 9, vglib.rgba(120, 130, 150, 255));

            # Real-Time EQ Curve Plot
            prev_px :: Float64 = 240.0; prev_py :: Float64 = 720.0;
            curr_x :: Float64 = 240.0;
            while (curr_x <= 1570.0) {
                eval_f = x_to_freq(curr_x, 240.0, 1330.0);
                
                ratio_f = eval_f / b1_f;
                g1 = -10.0 * vmath.log(1.0 + vmath.pow(1.0 / ratio_f, 4.0)) / 2.302585;
                d2 = vmath.log(eval_f / b2_f); g2 = b2_g * vmath.exp(-3.0 * d2 * d2 * b2_q);
                d3 = vmath.log(eval_f / b3_f); g3 = b3_g * vmath.exp(-3.0 * d3 * d3 * b3_q);
                d4 = vmath.log(eval_f / b4_f); g4 = b4_g * vmath.exp(-3.0 * d4 * d4 * b4_q);
                d5 = vmath.log(eval_f / b5_f); g5 = b5_g * vmath.exp(-3.0 * d5 * d5 * b5_q);
                d6 = vmath.log(eval_f / b6_f); g6 = b6_g * vmath.exp(-3.0 * d6 * d6 * b6_q);
                d7 = vmath.log(eval_f / b7_f); g7 = b7_g * vmath.exp(-3.0 * d7 * d7 * b7_q);

                tot = (eq_on == 1) ? (g1 + g2 + g3 + g4 + g5 + g6 + g7) : 0.0;
                curr_y = gain_to_y(tot, 720.0, 140.0);

                if (curr_x > 240.0) { vglib.line(prev_px, prev_py, curr_x, curr_y, vglib.rgba(0, 240, 255, 255)); }
                prev_px = curr_x; prev_py = curr_y; curr_x = curr_x + 8.0;
            }

            # EQ Interactive Node Handles
            draw_eq_node("1", freq_to_x(b1_f, 240, 1330), gain_to_y(0.0, 720.0, 140.0), vglib.rgba(255, 80, 80, 255), active_eq_node == 1);
            draw_eq_node("2", freq_to_x(b2_f, 240, 1330), gain_to_y(b2_g, 720.0, 140.0), vglib.rgba(255, 200, 50, 255), active_eq_node == 2);
            draw_eq_node("3", freq_to_x(b3_f, 240, 1330), gain_to_y(b3_g, 720.0, 140.0), vglib.rgba(50, 220, 120, 255), active_eq_node == 3);
            draw_eq_node("4", freq_to_x(b4_f, 240, 1330), gain_to_y(b4_g, 720.0, 140.0), vglib.rgba(0, 210, 255, 255), active_eq_node == 4);
            draw_eq_node("5", freq_to_x(b5_f, 240, 1330), gain_to_y(b5_g, 720.0, 140.0), vglib.rgba(180, 90, 255, 255), active_eq_node == 5);
            draw_eq_node("6", freq_to_x(b6_f, 240, 1330), gain_to_y(b6_g, 720.0, 140.0), vglib.rgba(255, 110, 200, 255), active_eq_node == 6);
            draw_eq_node("7", freq_to_x(b7_f, 240, 1330), gain_to_y(b7_g, 720.0, 140.0), vglib.rgba(255, 140, 40, 255), active_eq_node == 7);
        }

        # ====================================================================
        # RACK TAB 1: OPTO COMPRESSOR DISPLAY
        # ====================================================================
        if (active_tab == 1) {
            vglib.rect(240, 560, 1330, 375, vglib.rgba(18, 22, 30, 255));
            
            t_norm   = (thresh + 40.0) / 40.0;
            r_norm   = (ratio - 1.0) / 15.0;
            a_norm   = (attack - 1.0) / 99.0;
            rel_norm = (release - 10.0) / 490.0;
            m_norm   = makeup / 12.0;

            draw_knob("THRESH", 320, 640, t_norm, string(vmath.round(thresh)) + "dB", vglib.rgba(255, 130, 40, 255), active_knob == 1);
            draw_knob("RATIO", 480, 640, r_norm, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255), active_knob == 2);
            draw_knob("ATTACK", 640, 640, a_norm, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255), active_knob == 3);
            draw_knob("RELEASE", 800, 640, rel_norm, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255), active_knob == 4);
            draw_knob("MAKEUP", 960, 640, m_norm, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255), active_knob == 5);

            vglib.rect(1120, 590, 400, 30, vglib.rgba(12, 14, 20, 255));
            gr_w = vmath.clamp((gr_db / 24.0) * 400.0, 0.0, 400.0);
            vglib.rect(1120, 590, gr_w, 30, vglib.rgba(255, 60, 60, 255));
            vglib.text_ex(vcr_font, "GAIN REDUCTION: -" + string(vmath.round(gr_db * 10.0) / 10.0) + " dB", 1200, 600, 11, vglib.WHITE);
        }

        # ====================================================================
        # RACK TAB 2: SATURATOR DISPLAY
        # ====================================================================
        if (active_tab == 2) {
            vglib.rect(240, 560, 1330, 375, vglib.rgba(18, 22, 30, 255));

            draw_knob("DRIVE", 340, 660, drive, string(vmath.round(drive * 100.0)) + "%", vglib.rgba(255, 90, 90, 255), active_knob == 1);

            modes = ["SOFT TUBE", "HARD CLIP", "ASYMMETRIC", "TAPE WARMTH", "BITCRUSHER"];
            through m_idx :: 0..4 -> loop {
                bx :: Int64 = 520 + (m_idx * 160);
                b_col = (sat_mode == m_idx) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(35, 42, 54, 255);
                vglib.rect(bx, 640, 140, 40, b_col);
                vglib.text_ex(vcr_font, modes[m_idx], bx + 12, 654, 10, (sat_mode == m_idx) ? vglib.BLACK : vglib.WHITE);
            };
        }

        # ====================================================================
        # RACK TAB 3: SPATIAL REVERB DISPLAY
        # ====================================================================
        if (active_tab == 3) {
            vglib.rect(240, 560, 1330, 375, vglib.rgba(18, 22, 30, 255));

            draw_knob("DECAY", 340, 660, decay / 0.95, string(vmath.round(decay * 100.0)) + "%", vglib.rgba(180, 90, 255, 255), active_knob == 1);
            draw_knob("MIX", 520, 660, mix, string(vmath.round(mix * 100.0)) + "%", vglib.rgba(60, 220, 255, 255), active_knob == 2);
            draw_knob("PRE-DELAY", 700, 660, predelay / 100.0, string(vmath.round(predelay)) + "ms", vglib.rgba(255, 200, 50, 255), active_knob == 3);
            draw_knob("DAMPING", 880, 660, damping, string(vmath.round(damping * 100.0)) + "%", vglib.rgba(255, 90, 120, 255), active_knob == 4);
        }

        # Render Toast Banner Notification
        if (render_timer > 0.0) {
            render_timer = render_timer - 0.016;
            banner_col = (render_status == 1) ? vglib.rgba(50, 255, 120, 230) : vglib.rgba(255, 60, 60, 230);
            banner_msg = (render_status == 1) ? "BOUNCED MASTER: master_daw_mix.wav" : "RENDER FAILED!";
            vglib.rect(600, 440, 400, 50, vglib.rgba(18, 22, 28, 240));
            vglib.text_ex(vcr_font, banner_msg, 630, 458, 12, banner_col);
        }

    vglib.end();
}

vaudio.close_audio();
vglib.close();