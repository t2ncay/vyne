ruleset { dynamic_casting };

module vglib;

TOP_SKY = vglib.rgba(44, 0, 62, 255);
BOT_SKY = vglib.rgba(10, 10, 10, 255);
GRID_COLOR = vglib.rgba(0, 255, 150, 100);

vglib.init(1280, 720, "Vyne Engine - Scenery Test");
camera = vglib.camera();
rotation = 0.0;

while (vglib.running()) {
    vglib.begin();
    
    vglib.clear_gradient(TOP_SKY, BOT_SKY);
    
    vglib.begin3d(camera);
        
    vglib.plane(0.0, -2.0, 0.0, 100.0, 100.0, GRID_COLOR);
        
    vglib.cube(0.0, 0.0, 0.0, 2.0, rotation, vglib.rgba(255, 0, 255, 255));
        
    vglib.end3d();
    
    rotation = rotation + 1.5;
    vglib.end();
}

vglib.close();