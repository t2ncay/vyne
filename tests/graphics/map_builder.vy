ruleset { dynamic_casting, warnings };
module vglib;
module vmath;
module vfs;

vglib.init(1920, 1080, 60, "Vyne Architect - University Project", vglib.FULLSCREEN);
camera = vglib.camera(70.0);
vglib.set_pos(camera, 0.0, 25.0, -15.0);
vglib.disable_cursor();

tex_paths = [
    "tests/assets/Brick/Brick_16-512x512.png",
    "tests/assets/wall.jpeg", 
    "tests/assets/building.jpg",
    "tests/assets/yusif.jpeg",
    "tests/assets/asphalt_road_3.jpg",
    "tests/assets/Metal/Metal_18-512x512.png",
    "tests/assets/Metal/Metal_18-512x512.png",
    "tests/assets/Metal/Metal_18-512x512.png"
];

tex_slots = [];
through path :: tex_paths -> loop {
    tex_slots = tex_slots + [vglib.load_texture(path)];
};

# --- EDITOR STATE ---
run_time = 0.0;
current_slot = 0;
grid_size = 2.0;
cursor_pos = [0.0, 1.0, 0.0];
map_data = vglib.load_map("tau_map.dat");
message = "EDITOR READY - TAU MAP DESIGN";

map_file = "tau_map.dat";

if (vfs.exists(map_file)) {
    message = "EXISTING MAP FOUND. READY TO OVERWRITE.";
}

while (vglib.running()) {
    run_time = run_time + 0.016;

    cursor_pos = vglib.get_ray_grid(camera, 8.0, grid_size);
    
    if (vglib.key_pressed(vglib.ONE))   { current_slot = 0; }
    if (vglib.key_pressed(vglib.TWO))   { current_slot = 1; }
    if (vglib.key_pressed(vglib.THREE)) { current_slot = 2; }
    if (vglib.key_pressed(vglib.FOUR))  { current_slot = 3; }
    if (vglib.key_pressed(vglib.FIVE))  { current_slot = 4; }
    if (vglib.key_pressed(vglib.SIX))   { current_slot = 5; }
    if (vglib.key_pressed(vglib.SEVEN)) { current_slot = 6; }
    if (vglib.key_pressed(vglib.EIGHT)) { current_slot = 7; }
    
    if (vglib.key_pressed(vglib.SPACE) || vglib.mouse_down(vglib.MOUSE_LEFT)) {
        new_obj = [cursor_pos[0], cursor_pos[1], cursor_pos[2], grid_size, current_slot];
        map_data = map_data + [new_obj];
        message = "OBJECT PLACED AT " + string(cursor_pos[0]) + "," + string(cursor_pos[2]);
    }

    if (vglib.key_pressed(vglib.BACKSPACE) || vglib.mouse_down(vglib.MOUSE_RIGHT)) {
        target_x = cursor_pos[0];
        target_y = cursor_pos[1];
        target_z = cursor_pos[2];

        # map_data-nı süzgəcdən keçiririk (Filter mode)
        # Yalnız hədəf koordinatda OLMAYAN kubları saxlayırıq
        map_data = through obj :: map_data -> filter {
            !(obj[0] == target_x && obj[1] == target_y && obj[2] == target_z)
        };
        
        message = "OBJECT REMOVED AT CURSOR";
    }

    if (vglib.key_pressed(vglib.J)) {
        output_str = "";
        through obj :: map_data -> loop {
            line = string(obj[0]) + "," + string(obj[1]) + "," + string(obj[2]) + "," + string(obj[3]) + "," + string(obj[4]) + ";";
            output_str = output_str + line;
        };
        
        vfs.write(map_file, output_str);
        message = "MAP SAVED TO " + map_file;
    }

    if (vglib.key_pressed(vglib.K)) {
        vglib.export_obj(map_data, "exported_map.obj");
        message = "3D MESH EXPORTED SUCCESSFULLY VIA NATIVE CALL";
    }

    vglib.rotate_view(camera, 0.15);
    cam_pos = vglib.get_pos(camera);
    cam_y = cam_pos[1];

    if (vglib.key_down(vglib.LEFT_SHIFT)) { cam_y = cam_y + 0.5; }
    if (vglib.key_down(vglib.LEFT_CTRL))  { cam_y = cam_y - 0.5; }
    
    fly_speed = 0.4;
    if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, fly_speed); }
    if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, -fly_speed); }
    if (vglib.key_down(vglib.A)) { vglib.move_right(camera, -fly_speed); }
    if (vglib.key_down(vglib.D)) { vglib.move_right(camera, fly_speed); }

    vglib.set_camera_height(camera, cam_y);

    pulse_val = (vmath.sin(run_time * 12.0) * 0.4 + 0.6); 
    
    bright_green = vglib.rgba(50, 255, 50, int64(pulse_val * 180.0 + 75.0));
    
    alpha_val = int64(pulse_val * 145.0 + 55.0); 
    
    preview_x = 1700;
    preview_y = 850;
    preview_size = 150;

    vglib.begin();
        vglib.clear(vglib.rgba(15, 15, 20, 255));
        
        vglib.begin3d(camera);
            vglib.grid(100, grid_size);
            
            through obj :: map_data -> loop {
                vglib.cube_texture(tex_slots[obj[4]], obj[0], obj[1], obj[2], obj[3], vglib.WHITE);
            };
            
            vglib.cube_texture(tex_slots[current_slot], cursor_pos[0], cursor_pos[1], cursor_pos[2], grid_size, vglib.rgba(100, 255, 100, alpha_val));
        vglib.end3d();

        p_x = 1650; p_y = 800; p_size = 200;
        vglib.rect(p_x - 10, p_y - 10, p_size + 20, p_size + 20, vglib.rgba(40, 40, 50, 200));
        
        vglib.draw_texture(tex_slots[current_slot], p_x, p_y, p_size, p_size, vglib.WHITE);
        
        vglib.text("SELECTED TEXTURE", p_x, p_y - 30, 20, vglib.CYAN);
        vglib.text(tex_paths[current_slot], p_x, p_y + p_size + 10, 16, vglib.GRAY);
        
        cross_alpha = int64(pulse_val * 255.0);
        vglib.cube_texture(tex_slots[current_slot], cursor_pos[0], cursor_pos[1], cursor_pos[2], grid_size, bright_green);
        
        vglib.text("VYNE MAP BUILDER 1.0 | " + map_file, 50, 50, 24, vglib.CYAN);
        vglib.text("x :  " + string(cam_pos[0]), 50, 150, 24, vglib.GREEN);
        vglib.text("y :  " + string(cam_pos[1]), 50, 200, 24, vglib.GREEN);
        vglib.text("z :  " + string(cam_pos[2]), 50, 250, 24, vglib.GREEN);
        vglib.text("SLOT: " + string(current_slot + 1), 50, 85, 18, vglib.WHITE);
        vglib.text(message, 50, 1000, 22, vglib.GREEN);
    vglib.end();
}

vglib.close();

