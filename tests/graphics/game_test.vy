ruleset { dynamic_casting };

module vglib;
module vaudio;

BG = vglib.rgba(20, 20, 25, 255);
CUBE_COLOR = vglib.rgba(0, 150, 200, 255);

vglib.init(1920, 1080, 60, "Vyne Ultra - Fullscreen Mode", vglib.FULLSCREEN);
camera = vglib.camera();

posX = 0.0;
posZ = 0.0;
speed = 0.15;
rotation = 0.0;

while (vglib.running()) {
    vaudio.play_sound(ambiance);
    if (vglib.key_down(87)) { posZ = posZ - speed; }
    if (vglib.key_down(83)) { posZ = posZ + speed; }
    if (vglib.key_down(65)) { posX = posX - speed; }
    if (vglib.key_down(68)) { posX = posX + speed; }

    vglib.begin();
    vglib.clear(BG);
    
    vglib.begin3d(camera);
        vglib.grid(50, 1.0);

        vglib.cube(posX, 1.0, posZ, 2.0, rotation, CUBE_COLOR);

        if (vglib.key_down(vglib.SPACE)) { 
            vglib.cube(posX, 1.0, 0.0, 2.0, rotation, vglib.MAGENTA);
        }
    vglib.end3d();
    
    vglib.text("Use WASD to Move, SPACE to spawn Ghost Cube", 20, 20, 20, vglib.WHITE);
    
    #rotation = rotation + 1.0;
    vglib.end();
}

vaudio.close_audio();
vglib.close();