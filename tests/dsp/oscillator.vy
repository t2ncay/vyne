ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1100, 700, 60, "VYNE SERUM WAVETABLE SYNTH v1.0", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

vaudio.init_audio();

# Synth Parameters
wt_pos :: Float64 = 0.0; # 0.0 to 1.0 (Wavetable Morph Position)
cutoff :: Float64 = 2000.0;
resonance :: Float64 = 0.7;

# ADSR State
attack  :: Float64 = 10.0;
decay   :: Float64 = 150.0;
sustain :: Float64 = 0.7;
release :: Float64 = 300.0;

fn draw_wavetable_display(x, y, w, h, wt_pos) {
    vglib.rect(x, y, w, h, vglib.rgba(12, 14, 18, 255));
    vglib.line(x, y, x + w, y, vglib.rgba(0, 220, 255, 100));

    # Draw simulated 2D single-cycle frame at current WT position
    prev_px :: Float64 = x;
    prev_py :: Float64 = y + (h / 2);

    curr_x :: Float64 = x;
    while (curr_x <= x + w) {
        norm_x = (curr_x - x) / w;
        
        # Simulated morph math between Sine and Saw for visualization
        sine_val = vmath.sin(norm_x * 6.28318);
        saw_val  = (norm_x < 0.5) ? (norm_x * 2.0) : ((norm_x - 1.0) * 2.0);
        
        val = ((1.0 - wt_pos) * sine_val) + (wt_pos * saw_val);
        curr_y = (y + (h / 2)) - (val * (h * 0.4));

        if (curr_x > x) {
            vglib.line(prev_px, prev_py, curr_x, curr_y, vglib.rgba(0, 230, 255, 255));
        }

        prev_px = curr_x;
        prev_py = curr_y;
        curr_x = curr_x + 4.0;
    }

    vglib.text_ex(vcr_font, "WAVETABLE OSC A", x + 10, y + 10, 12, vglib.WHITE);
}

while (vglib.running()) {
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    # Drag Wavetable Position with Left Mouse
    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (m[0] >= 100 && m[0] <= 500 && m[1] >= 100 && m[1] <= 300) {
            wt_pos = vmath.clamp((m[0] - 100.0) / 400.0, 0.0, 1.0);
        }
    }

    vglib.begin();
        vglib.clear(vglib.rgba(18, 20, 26, 255));

        # Header Title
        vglib.text_ex(vcr_font, "VYNE SERUM-1 WT SYNTH", 400, 30, 20, vglib.WHITE);

        # Render Visualizer
        draw_wavetable_display(100, 100, 400, 200, wt_pos);

    vglib.end();
}

vaudio.close_audio();
vglib.close();