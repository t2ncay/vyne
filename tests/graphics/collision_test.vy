ruleset { dynamic_casting };
module vglib;

vglib.init(1920, 1080, 100, "Vyne Pro - Rigid World", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.disable_cursor();

player_pos = [0.0, 1.8, 0.0];
player_size = [0.5, 1.8, 0.5]; # Oyunçunun fiziksel ölçüləri

walls = [
    [0.0, 2.5, 10.0, 5.0],
    [10.0, 2.5, 20.0, 5.0],
    [-15.0, 2.5, 35.0, 5.0]
];

while (vglib.running()) {
    vglib.rotate_view(camera, 0.15);

    old_pos = player_pos;
    
    if (vglib.key_down(vglib.W)) {
        can_move = true;
        
        through wall :: walls -> loop {
            if (vglib.check_collision(player_pos, player_size, [wall[0], wall[1], wall[2]], wall[3])) {
                can_move = false;
            }
        };
        
        if (can_move) {
            vglib.move_forward(camera, 0.15);
        }
    }

    vglib.begin();
        vglib.clear(vglib.rgba(13, 13, 20, 255));
        vglib.begin3d(camera);
            vglib.grid(100, 1.0);
            
            through w :: walls -> loop {
                vglib.cube(w[0], w[1], w[2], w[3], 0.0, vglib.CYAN);
            };
        vglib.end3d();
    vglib.end();
}