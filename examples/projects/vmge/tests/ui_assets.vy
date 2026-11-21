use "../src/editor/workspace.vy";
use native vmem;

fn uia_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn uia_same(left :: Array,right :: Array) -> Bool {
    if left.size() != right.size() { return false; } i :: Int64 := 0;
    while i < left.size() { if left[i] != right[i] { return false; } i = i+1; } return true;
}
fn uia_u32(bytes :: Array,offset :: Int64,value :: Int64) -> Bool {
    bytes[offset] = value%256; bytes[offset+1] = (value/256)%256; bytes[offset+2] = (value/65536)%256; bytes[offset+3] = (value/16777216)%256; return true;
}
fn uia_glb(json :: String) -> Array {
    text :: String := json; while text.size()%4 != 0 { text = text+" "; }
    utf8 :: Array<Int64> := []; utf8.reserve(text.size()+1); utf8.resize(text.size()+1,0); se_text_write(utf8,text);
    bytes :: Array := []; i :: Int64 := 0; while i < 32+text.size() { bytes.push(0); i = i+1; }
    uia_u32(bytes,0,1179937895); uia_u32(bytes,4,2); uia_u32(bytes,8,32+text.size());
    uia_u32(bytes,12,text.size()); uia_u32(bytes,16,1313821514); i = 1;
    while i <= utf8[0] { bytes[19+i] = utf8[i]; i = i+1; }
    uia_u32(bytes,20+text.size(),4); uia_u32(bytes,24+text.size(),5130562); bytes[28+text.size()] = 0; bytes[29+text.size()] = 127; bytes[30+text.size()] = 255; bytes[31+text.size()] = 0; return bytes;
}
fn uia_reject(root :: String,source :: String,marker :: String) -> Bool {
    failed :: Bool := false;
    try { se_import_asset(root,source); } catch (error) { failed = se_contains(string(error),marker); }
    return uia_check(failed,"expected actionable import rejection: "+marker);
}
fn uia_pixel(target :: vraylib.RenderTarget,x :: Int64,y :: Int64) -> vraylib.Color {
    image :: Int64 := vraylib.load_image_from_texture(target.texture().native_handle()); colors :: Int64 := vraylib.load_image_colors(image);
    pixel :: vraylib.Color := vraylib.buffer_read(colors,(target.height()-1-y)*target.width()+x);
    vraylib.unload_image_colors(colors); vraylib.unload_image(image); return pixel;
}
app :: vraylib.App := vraylib.app_options(400,240,"VMGE imports / previews / views",0,vraylib.FLAG_WINDOW_HIDDEN);
try {
    output :: String := vmge_option("VYNE_UI_TEST_ROOT","build/vmge-ui-owned-imports-20261121");
    uia_check(vfs.is_dir(vfs.dirname(output)),"unique output parent must exist");
    uia_check(vfs.mkdir(output),"create focused output directory");
    root :: String := vfs.join(output,"project"); sources :: String := vfs.join(output,"sources");
    uia_check(vfs.mkdir(root) && vfs.mkdir(sources),"create project/source parents");
    if !vraylib.is_path_absolute(root) { root = vfs.join(vraylib.get_working_directory(),root); }
    if !vraylib.is_path_absolute(sources) { sources = vfs.join(vraylib.get_working_directory(),sources); }
    source :: String := vfs.join(sources,"binary.glb");
    original :: Array := uia_glb("\{\"asset\":\{\"version\":\"2.0\"},\"buffers\":[\{\"byteLength\":4}],\"extras\":\{\"unicode\":\"\\u00e9\",\"flag\":true,\"value\":-1.25e+2,\"null\":null}}");
    uia_check(vfs.write_bytes(source,original),"write GLB source");
    imported :: String := se_import_asset(root,source);
    uia_check(imported == "assets/imports/binary.glb" && uia_same(se_import_read_bytes(vfs.join(root,imported),original.size()),original),"binary GLB copy preserves NUL/high bytes and returns a relative path");
    second :: String := se_import_asset(root,source);
    uia_check(second == "assets/imports/binary-1.glb" && uia_same(se_import_read_bytes(vfs.join(root,imported),original.size()),original),"filename collision creates a new copy without overwriting");
    changed :: Array := uia_glb("\{\"asset\":\{\"version\":\"2.0\"},\"buffers\":[\{\"byteLength\":4}],\"extras\":\"changed\"}");
    uia_check(vfs.write_bytes(source,changed) && uia_same(se_import_read_bytes(vfs.join(root,imported),original.size()),original),"project copy is independent of source mutations");
    bad :: String := vfs.join(sources,"external.glb");
    uia_check(vfs.write_bytes(bad,uia_glb("\{\"asset\":\{\"version\":\"2.0\"},\"buffers\":[\{\"byteLength\":4}],\"images\":[\{\"u\\u0072i\":\"textures/albedo.png\"}]}")),"write escaped external dependency");
    uia_reject(root,bad,"external URI");
    uia_check(!vfs.exists(vfs.join(root,"assets/imports/external.glb")),"rejected dependency writes no project file");
    uia_check(vfs.write_bytes(bad,uia_glb("\{\"asset\":\{\"version\":\"2.0\"},}")),"write invalid JSON"); uia_reject(root,bad,"trailing comma");
    uia_check(vfs.write_bytes(bad,uia_glb("\{\"asset\":\{\"version\":\"2.0\"},\"buffers\":[\{\"byteLength\":8}]}")),"write invalid embedded buffer"); uia_reject(root,bad,"BIN chunk");
    unsupported :: String := vfs.join(sources,"external.gltf"); uia_check(vfs.write(unsupported,"\{}"),"write unsupported source"); uia_reject(root,unsupported,"Re-export");
    bytes :: Array<Int64> := []; bytes.reserve(256); bytes.resize(256,0); ui :: SEUI := se_ui(bytes);
    png :: String := vfs.join(sources,"image.png"); image :: vraylib.Image := app.image(512,256,vraylib.RED);
    uia_check(image.save(png),"write image source"); image.close();
    png_bytes :: Array := se_import_read_bytes(png,vfs.size(png)); png_relative :: String := se_import_asset(root,png);
    uia_check(uia_same(se_import_read_bytes(vfs.join(root,png_relative),png_bytes.size()),png_bytes),"PNG import preserves exact bytes");
    baseline :: Int64 := vraylib.live_handles(); preview :: Int64 := se_preview_request(ui,app,png);
    texture :: vraylib.Texture := vraylib.Texture(preview,app.token,true);
    uia_check(preview > 0 && texture.width() == 128 && texture.height() == 64,"preview atlas upload is bounded and preserves aspect");
    uia_check(se_preview_request(ui,app,png) == preview && ui.browser_state[1] == 1,"repeat selection does not reread/reload");
    image = app.image(512,256,vraylib.BLUE); image.save(png); image.close();
    uia_check(se_preview_request(ui,app,png) == preview,"changed file remains cached until explicit refresh");
    se_preview_invalidate(ui,app); uia_check(!texture.valid() && vraylib.live_handles() == baseline,"refresh closes old texture tokens");
    paths :: Array := []; image = app.image(32,16,vraylib.BLUE); index :: Int64 := 0;
    while index < 17 { path :: String := vfs.join(sources,"thumb-"+string(index)+".png"); image.save(path); paths.push(path); index = index+1; } image.close();
    oldest :: Int64 := se_preview_request(ui,app,paths[0]); index = 1;
    while index < 17 { uia_check(se_preview_request(ui,app,paths[index]) > 0,"load bounded cache entry"); index = index+1; }
    old :: vraylib.Texture := vraylib.Texture(oldest,app.token,true);
    uia_check(!old.valid() && vraylib.live_handles() == baseline+16,"17th selection evicts oldest App-owned preview");
    se_preview_invalidate(ui,app); uia_check(vraylib.live_handles() == baseline,"all cached previews close on invalidation");
    preview = se_preview_request(ui,app,png);
    camera :: vraylib.Camera := app.camera(vraylib.vec3(8.0,6.0,12.0),vraylib.vec3(0.0,0.0,0.0),55.0);
    ui.numbers[6] = 0.0; ui.numbers[7] = 0.0; ui.numbers[8] = 0.0;
    preset :: Int64 := 1;
    while preset <= 3 {
        se_view_preset(ui,camera,preset); snapshot :: vraylib.Camera3D := camera.view();
        left :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(50.0,120.0),400,240); right :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(350.0,120.0),400,240);
        uia_check(snapshot.projection == vraylib.CAMERA_ORTHOGRAPHIC && vraylib.math_vector3_dot_product(left.direction,right.direction) > 0.99999,"orthographic rays are parallel");
        uia_check(vraylib.math_vector3_distance(left.position,right.position) > 0.1,"orthographic rays have distinct origins");
        center :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(200.0,120.0),400,240);
        if preset == 1 { uia_check(center.direction.y < -0.9999 && snapshot.up.z == -1.0,"top view has a nondegenerate up vector"); }
        if preset == 2 { uia_check(center.direction.z < -0.9999,"front view ray"); }
        if preset == 3 { uia_check(center.direction.x < -0.9999,"side view ray"); }
        preset = preset+1;
    }
    se_view_preset(ui,camera,0); snapshot :: vraylib.Camera3D := camera.view();
    uia_check(snapshot.projection == vraylib.CAMERA_PERSPECTIVE && snapshot.fovy == 55.0,"perspective preset restores authored camera FOV");
    left :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(50.0,120.0),400,240); right :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(350.0,120.0),400,240);
    uia_check(vraylib.math_vector3_dot_product(left.direction,right.direction) < 0.999,"perspective rays diverge");
    directory :: Array<Int64> := []; directory.reserve(256); directory.resize(256,0);
    target :: vraylib.RenderTarget := app.target(400,240); loads :: Int64 := ui.browser_state[1]; warm :: Int64 := vmem.commit_total_allocated(); app.limit(4);
    while app.frame() {
        if app.frames() == 0 {
            se_asset_import_ui(ui,app,root,png,directory);
            uia_check(se_text_read(directory) == vfs.join(root,"assets/imports") && se_asset_name(ui,ui.state[28]) == "image-1.png","frame import selects its project-owned copy in retained UTF-8 slots");
        }
        if app.frames() == 1 {
            selected :: String := se_asset_name(ui,ui.state[28]); se_preview_invalidate(ui,app);
            se_asset_select_name(ui,app,se_text_read(directory),selected);
        }
        selected_path :: String := vfs.join(se_text_read(directory),se_asset_name(ui,ui.state[28]));
        target.begin(); app.clear(vraylib.BLACK); se_preview_draw(ui,app,selected_path,se_rect(10.0,10.0,128.0,80.0));
        se_view_controls(ui,camera,se_rect(0.0,0.0,400.0,240.0)); target.end();
        pixel :: vraylib.Color := uia_pixel(target,70,45); uia_check(pixel.b == 241,"refreshed preview shows changed image bytes");
        expected_loads :: Int64 := loads+2; if app.frames() == 0 { expected_loads = loads+1; }
        uia_check(ui.browser_state[1] == expected_loads,"preview drawing performs no loads; only explicit import/refresh loads once");
        app.begin(); target.present(400.0,240.0); app.end(); uia_check(app.passes() == 0,"balanced view/preview passes");
    }
    uia_check(vmem.commit_total_allocated() == warm,"frame import/preview/view actions retain no frame boxes");
} finally { app.close(); }
uia_check(vraylib.live_handles() == 0,"App closes previews and camera without manual teardown");
out("VMGE_UI_ASSETS_OK");
