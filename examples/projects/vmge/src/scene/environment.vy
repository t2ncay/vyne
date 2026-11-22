use "transforms.vy";

# Fixed preframe context: enabled, ambient, sun direction/color, point count;
# four point slots at 16, stride 8: XYZ, radius, RGB, intensity.
fn se_light_collect(scene :: SEScene,cache :: SETransforms,lights :: Array<Float64>) -> Bool {
    i :: Int64 := 0; while i < 48 { lights[i] = 0.0; i = i+1; }
    lights[1] = 0.15; lights[3] = 1.0;
    sky :: Int64 := se_active_node(scene,5); preferred :: Int64 := -1;
    if sky != -1 && se_visible(scene,sky) { lights[1] = scene.extra[(sky%128)*16+10]; preferred = int64(scene.extra[(sky%128)*16+9]); }
    sun :: Int64 := -1; slot :: Int64 := 0;
    while slot < 128 { if scene.meta[slot*8] != 0 && se_visible(scene,scene.handle(slot)) {
        kind :: Int64 := scene.meta[slot*8+3]; b :: Int64 := slot*16;
        if kind == 6 && (sun == -1 || slot == preferred) { sun = slot; }
        if kind == 7 {
            count :: Int64 := int64(lights[8]); if count >= 4 { throw "Visible point-light budget: four lights"; }
            p :: Int64 := 16+count*8;
            if sky == -1 && count == 0 { lights[1] = scene.extra[b+2]; }
            lights[p] = cache.matrices[b+3]; lights[p+1] = cache.matrices[b+7]; lights[p+2] = cache.matrices[b+11]; lights[p+3] = scene.extra[b+1];
            lights[p+4] = scene.values[b+9]/255.0; lights[p+5] = scene.values[b+10]/255.0; lights[p+6] = scene.values[b+11]/255.0; lights[p+7] = scene.extra[b];
            lights[8] = lights[8]+1.0; lights[0] = 1.0;
        }
    } slot = slot+1; }
    if sun >= 0 {
        b :: Int64 := sun*16; dx :: Float64 := -cache.matrices[b+2]; dy :: Float64 := -cache.matrices[b+6]; dz :: Float64 := -cache.matrices[b+10]; length :: Float64 := vmath.sqrt(dx*dx+dy*dy+dz*dz);
        if length < 0.000001 { throw "Degenerate directional light transform"; }
        lights[2] = dx/length; lights[3] = dy/length; lights[4] = dz/length;
        lights[5] = scene.values[b+9]/255.0*scene.extra[b]; lights[6] = scene.values[b+10]/255.0*scene.extra[b]; lights[7] = scene.values[b+11]/255.0*scene.extra[b];
        if sky == -1 { lights[1] = scene.extra[b+2]; } lights[0] = 1.0;
    } return true;
}
fn se_light_upload(shader :: vraylib.Shader,lights :: Array,world :: Bool) -> Bool {
    mode :: Float64 := 0.0; if world { mode = 1.0; }
    shader.set("seAmbient",lights[1]); shader.set("seWorldMode",mode);
    shader.set("seSunDirection",vraylib.vec3(lights[2],lights[3],lights[4])); shader.set("seSunColor",vraylib.vec3(lights[5],lights[6],lights[7]));
    shader.set("seCameraPosition",vraylib.vec3(lights[9],lights[10],lights[11]));
    shader.set("sePointCount",int64(lights[8])); i :: Int64 := 0; while i < 4 { b :: Int64 := 16+i*8;
        shader.set("sePoint"+string(i),vraylib.Vector4(lights[b],lights[b+1],lights[b+2],lights[b+3]));
        shader.set("sePointColor"+string(i),vraylib.vec3(lights[b+4]*lights[b+7],lights[b+5]*lights[b+7],lights[b+6]*lights[b+7])); i = i+1;
    } return true;
}
fn se_light_custom(shader :: vraylib.Shader,lights :: Array) -> Bool {
    # Engine uniforms are opt-in and not ordinary authored parameters. A shader
    # declaring seAmbient adopts this contract; unlit/custom shaders stay intact.
    rows :: Array := shader.uniforms(); i :: Int64 := 0; while i < rows.size() { row :: Array := rows[i];
        if row[0] == "seAmbient" && int64(row[1]) == 5126 && int64(row[2]) == 1 && int64(row[3]) >= 0 { se_light_upload(shader,lights,false); return true; }
        i = i+1;
    } return false;
}
fn se_sky_draw(app :: vraylib.App,scene :: SEScene,camera :: vraylib.Camera,shader :: vraylib.Shader,lights :: Array<Float64>,width :: Int64,height :: Int64) -> Bool {
    id :: Int64 := se_active_node(scene,5); if id == -1 || !se_visible(scene,id) { return false; }
    b :: Int64 := (id%128)*16; view :: vraylib.Camera3D := camera.view(); forward :: vraylib.Vector3 := vraylib.direction(view.position,view.target);
    right :: vraylib.Vector3 := vraylib.math_vector3_normalize(vraylib.math_vector3_cross_product(forward,view.up)); up :: vraylib.Vector3 := vraylib.math_vector3_cross_product(right,forward);
    shader.set("skyForward",forward); shader.set("skyRight",right); shader.set("skyUp",up);
    shader.set("skyResolution",vraylib.vec2(float64(width),float64(height))); shader.set("skyTanFov",vmath.tan(view.fovy*0.00872664626));
    shader.set("skyZenith",vraylib.vec3(scene.extra[b],scene.extra[b+1],scene.extra[b+2])); shader.set("skyHorizon",vraylib.vec3(scene.extra[b+3],scene.extra[b+4],scene.extra[b+5]));
    shader.set("skyBrightness",scene.extra[b+6]); shader.set("skySunSize",scene.extra[b+7]*0.01745329252); shader.set("skySunEnergy",scene.extra[b+8]);
    shader.set("skySunDirection",vraylib.vec3(lights[2],lights[3],lights[4]));
    shader.begin(); try { vraylib.draw_rectangle(0,0,width,height,vraylib.WHITE); } finally { shader.end(); } return true;
}
