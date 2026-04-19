ruleset { dynamic_casting };

module vglib;

fn rgba(r, g, b, a) {
    return (r * 16777216) + (g * 65536) + (b * 256) + a;
}

BG_COLOR = rgba(24, 24, 24, 255);
CYAN = rgba(0, 170, 255, 255);
MAGENTA = rgba(255, 0, 255, 255);

vglib.init(800, 600, "Vyne Engine - 3D Rotating Cube Test");

camera = vglib.camera();

rotation = 0.0;

while (vglib.running()) {
    vglib.begin();
    vglib.clear(BG_COLOR);
    
    vglib.begin3d(camera);
    
    vglib.cube(0.0, 0.0, 0.0, 4.0, rotation, CYAN);
        
    vglib.end3d();

    if (vglib.key_down(32)) {
        rotation = 0.0;
    }
    
    # İndi bu rəqəmi dəyişsən, sürət həqiqətən dəyişəcək!
    rotation = rotation + 5.0; 
    
    vglib.end();
}

vglib.close();