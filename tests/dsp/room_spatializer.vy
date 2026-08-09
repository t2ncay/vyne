ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1280, 800, 60, "VYNE 3D ACOUSTIC ROOM SPATIALIZER", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

vaudio.init_audio();
vaudio.volume(1.0);

# Load Audio Stream to Spatialize
audio_emitter = vaudio.load_sound("tests/assets/salam verdim.mp3");

# Color Palette
COLOR_BG     = vglib.rgba(10, 12, 16, 255);
COLOR_PANEL  = vglib.rgba(18, 22, 30, 255);
COLOR_BORDER = vglib.rgba(40, 50, 68, 255);
COLOR_CYAN   = vglib.rgba(0, 240, 255, 255);
COLOR_AMBER  = vglib.rgba(255, 170, 0, 255);
COLOR_PINK   = vglib.rgba(255, 45, 120, 255);

camera_3d :: Int64   = vglib.camera(45.0);
run_time  :: Float64 = 0.0;

# Orbit Camera Coordinates (yaw, pitch, distance)
cam_yaw   :: Float64 = 0.0;
cam_pitch :: Float64 = 0.6;
cam_dist  :: Float64 = 15.0;

# Listener and Emitter Position Vectors in 3D Space
listener_pos :: Array = [0.0, 1.5, 0.0];
source_pos   :: Array = [3.0, 1.5, 2.0];

# Virtual Room Dimensions (Width, Height, Depth)
room_dim :: Array = [10.0, 4.0, 10.0];

while (vglib.running()) {
    run_time = run_time + 0.016;

    # WASD Camera Rotation Input Handling
    if (vglib.key_down(vglib.A)) { cam_yaw = cam_yaw - 0.03; }
    if (vglib.key_down(vglib.D)) { cam_yaw = cam_yaw + 0.03; }
    if (vglib.key_down(vglib.W)) { cam_pitch = vmath.clamp(cam_pitch + 0.03, 0.1, 1.4); }
    if (vglib.key_down(vglib.S)) { cam_pitch = vmath.clamp(cam_pitch - 0.03, 0.1, 1.4); }

    # Compute Camera Vector relative to Room Center
    cam_x :: Float64 = vmath.sin(cam_yaw) * vmath.cos(cam_pitch) * cam_dist;
    cam_y :: Float64 = vmath.sin(cam_pitch) * cam_dist;
    cam_z :: Float64 = vmath.cos(cam_yaw) * vmath.cos(cam_pitch) * cam_dist;

    vglib.set_pos(camera_3d, cam_x, cam_y, cam_z);

    # Orbit Source Around Listener in 3D Room
    source_pos[0] = vmath.sin(run_time * 0.8) * 4.0;
    source_pos[2] = vmath.cos(run_time * 0.8) * 4.0;

    # Calculate Distance Vector
    dx :: Float64 = float64(listener_pos[0]) - float64(source_pos[0]);
    dy :: Float64 = float64(listener_pos[1]) - float64(source_pos[1]);
    dz :: Float64 = float64(listener_pos[2]) - float64(source_pos[2]);
    dist :: Float64 = vmath.sqrt(dx * dx + dy * dy + dz * dz);

    # Apply Native 3D Attenuation to Sound Handle
    vaudio.sound_3d(audio_emitter, listener_pos, source_pos, 12.0, 1.0);

    vglib.begin();
        vglib.clear(COLOR_BG);

        # Header Bar
        vglib.rect(0, 0, 1280, 40, COLOR_PANEL);
        vglib.line(0, 40, 1280, 40, COLOR_BORDER);
        vglib.text_ex(vcr_font, "3D ACOUSTIC ROOM SPATIALIZER & EARLY REFLECTION RAYTRACER", 16, 12, 14, COLOR_CYAN);

        # --- LEFT PANEL: 3D WIREFRAME ACOUSTIC CHAMBER ---
        vglib.rect(20, 60, 800, 680, COLOR_PANEL);
        vglib.line(20, 60, 820, 60, COLOR_CYAN);

        vglib.begin3d(camera_3d);
            vglib.grid(12, 1.0);

            # Render 3D Wireframe Room Bounding Box
            rw :: Float64 = float64(room_dim[0]) * 0.5;
            rh :: Float64 = float64(room_dim[1]);
            rd :: Float64 = float64(room_dim[2]) * 0.5;

            # Floor & Ceiling Outline
            vglib.line_3d(-rw, 0.0, -rd, rw, 0.0, -rd, COLOR_BORDER);
            vglib.line_3d(rw, 0.0, -rd, rw, 0.0, rd, COLOR_BORDER);
            vglib.line_3d(rw, 0.0, rd, -rw, 0.0, rd, COLOR_BORDER);
            vglib.line_3d(-rw, 0.0, rd, -rw, 0.0, -rd, COLOR_BORDER);

            vglib.line_3d(-rw, rh, -rd, rw, rh, -rd, COLOR_BORDER);
            vglib.line_3d(rw, rh, -rd, rw, rh, rd, COLOR_BORDER);
            vglib.line_3d(rw, rh, rd, -rw, rh, rd, COLOR_BORDER);
            vglib.line_3d(-rw, rh, rd, -rw, rh, -rd, COLOR_BORDER);

            # Direct Line-of-Sight Sound Path
            lx :: Float64 = float64(listener_pos[0]);
            ly :: Float64 = float64(listener_pos[1]);
            lz :: Float64 = float64(listener_pos[2]);

            sx :: Float64 = float64(source_pos[0]);
            sy :: Float64 = float64(source_pos[1]);
            sz :: Float64 = float64(source_pos[2]);

            vglib.line_3d(lx, ly, lz, sx, sy, sz, COLOR_CYAN);

            # Render Early Reflection Rays (Wall Bounces)
            vglib.line_3d(sx, sy, sz, rw, sy, (sz + lz) * 0.5, COLOR_AMBER);
            vglib.line_3d(rw, sy, (sz + lz) * 0.5, lx, ly, lz, COLOR_AMBER);

            vglib.line_3d(sx, sy, sz, -rw, sy, (sz + lz) * 0.5, COLOR_AMBER);
            vglib.line_3d(-rw, sy, (sz + lz) * 0.5, lx, ly, lz, COLOR_AMBER);

            # Listener Head Marker
            vglib.cube(lx, ly, lz, 0.4, 0.0, COLOR_CYAN);

            # Audio Source Marker
            vglib.cube(sx, sy, sz, 0.4, 0.0, COLOR_PINK);

        vglib.end3d();

        # --- RIGHT PANEL: ACOUSTIC READOUTS ---
        vglib.rect(840, 60, 420, 680, COLOR_PANEL);
        vglib.line(840, 60, 1260, 60, COLOR_AMBER);
        vglib.text_ex(vcr_font, "REAL-TIME SPATIAL METRICS", 855, 75, 12, COLOR_AMBER);

        vglib.text_ex(vcr_font, "SOURCE DISTANCE: " + string(vmath.round(dist * 100.0) / 100.0) + " m", 855, 110, 11, COLOR_CYAN);
        vglib.text_ex(vcr_font, "DIRECT PATH DELAY: " + string(vmath.round((dist / 343.0) * 1000.0)) + " ms", 855, 135, 11, COLOR_AMBER);

        # Attenuation Meter
        att_norm :: Float64 = vmath.clamp(1.0 - (dist / 12.0), 0.0, 1.0);
        vglib.text_ex(vcr_font, "ATTENUATION GAIN:", 855, 170, 11, COLOR_CYAN);
        vglib.rect(855, 190, 380, 16, COLOR_BG);
        vglib.rect(855, 190, att_norm * 380.0, 16, COLOR_PINK);

        # Bottom Bar
        vglib.rect(0, 760, 1280, 40, COLOR_PANEL);
        vglib.text_ex(vcr_font, "CONTROLS: WASD ORBIT ROTATION | VAUDIO 3D ENGINE", 20, 775, 10, COLOR_CYAN);

    vglib.end();
}

vaudio.close_audio();
vglib.close();