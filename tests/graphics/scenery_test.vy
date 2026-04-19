ruleset { dynamic_casting };

module vglib;

CYAN_BG = vglib.rgba(0, 100, 150, 255);
NEON_PINK = vglib.rgba(255, 0, 255, 255);
WHITE = vglib.rgba(255, 255, 255, 255);

vglib.init(1280, 720, "Vyne Engine - The Grid");
camera = vglib.camera();
rotation = 0.0;

while (vglib.running()) {
    vglib.begin();
    vglib.clear(CYAN_BG);
    
    vglib.begin3d(camera);
        
        vglib.grid(20, 1.0); 
        
        vglib.cube(0.0, 1.0, 0.0, 2.0, rotation, NEON_PINK);
        
    vglib.end3d();
    
    vglib.text("VYNE ENGINE v0.1", 20, 20, 20, WHITE);
    vglib.text("FPS: 60", 20, 50, 20, WHITE); # gələcəkdə bura vglib.get_fps() qoyarıq
    
    rotation = rotation + 1.2;
    vglib.end();
}

vglib.close();