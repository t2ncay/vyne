ruleset { dynamic_casting, warnings };
module vglib;
module vmath;
module vfs;

vglib.init(1920, 1080, 60, "Vyne Architect - University Project", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.set_pos(camera, 0.0, 25.0, -15.0);
vglib.disable_cursor();

tex_paths = [
    "tests/assets/asphalt_road_3.jpg", 
    "tests/assets/wall.jpeg", 
    "tests/assets/building.jpg",
    "tests/assets/yusif.jpeg" # Stalker/Entity spawn
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
map_data = [];
message = "EDITOR READY - TAU MAP DESIGN";

map_file = "tau_map.dat";

if (vfs.exists(map_file)) {
    message = "EXISTING MAP FOUND. READY TO OVERWRITE.";
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    if (vglib.key_pressed(vglib.ONE))   { current_slot = 0; }
    if (vglib.key_pressed(vglib.TWO))   { current_slot = 1; }
    if (vglib.key_pressed(vglib.THREE)) { current_slot = 2; }
    if (vglib.key_pressed(vglib.FOUR))  { current_slot = 3; }

    if (vglib.key_pressed(vglib.UP))    { cursor_pos[2] = cursor_pos[2] + grid_size; }
    if (vglib.key_pressed(vglib.DOWN))  { cursor_pos[2] = cursor_pos[2] - grid_size; }
    if (vglib.key_pressed(vglib.LEFT))  { cursor_pos[0] = cursor_pos[0] - grid_size; }
    if (vglib.key_pressed(vglib.RIGHT)) { cursor_pos[0] = cursor_pos[0] + grid_size; }
    
    if (vglib.key_pressed(vglib.U)) { cursor_pos[1] = cursor_pos[1] + grid_size; }
    if (vglib.key_pressed(vglib.O)) { cursor_pos[1] = cursor_pos[1] - grid_size; }

    if (vglib.key_pressed(vglib.SPACE)) {
        new_obj = [cursor_pos[0], cursor_pos[1], cursor_pos[2], grid_size, current_slot];
        map_data = map_data + [new_obj];
        message = "OBJECT PLACED AT GRID";
    }

    if (vglib.key_pressed(vglib.BACKSPACE)) {
        if (map_data.size() > 0) {
            map_data.pop();
            message = "LAST OBJECT REMOVED";
        }
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

    vglib.rotate_view(camera, 0.15);
    if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, 0.2); }
    if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, -0.2); }

    vglib.begin();
        vglib.clear(vglib.rgba(20, 20, 25, 255));
        
        vglib.begin3d(camera);
            vglib.grid(100, grid_size); # Vizual grid
            
            through obj :: map_data -> loop {
                vglib.cube_texture(tex_slots[obj[4]], obj[0], obj[1], obj[2], obj[3], vglib.WHITE);
            };
            
            pulse = int64((vmath.sin(run_time * 8.0) * 0.5 + 0.5) * 180.0);
            vglib.cube_texture(tex_slots[current_slot], cursor_pos[0], cursor_pos[1], cursor_pos[2], grid_size, vglib.rgba(255, 255, 255, pulse));
        vglib.end3d();

        # UI
        vglib.text("VYNE MAP BUILDER 1.0 | " + map_file, 50, 50, 24, vglib.CYAN);
        vglib.text("SLOT: " + string(current_slot + 1) + " (" + tex_paths[current_slot] + ")", 50, 85, 18, vglib.WHITE);
        vglib.text("OBJS: " + string(map_data.size()), 50, 115, 18, vglib.GRAY);
        vglib.text(message, 50, 1000, 22, vglib.GREEN);
        
        # Crosshair
        vglib.rect(955, 535, 10, 10, vglib.rgba(255, 255, 255, 150));
    vglib.end();
}

vglib.close();