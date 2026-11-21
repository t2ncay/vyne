use "transforms.vy";
use "../engine/assets.vy";
use "../engine/world_render.vy";

# Persist only numeric resource tokens. Reconstruct thin wrappers in the frame.
# Path slots and their boxed headers are preacquired from the owner slab.
interface SEAssets {
    tokens :: Array<Int64>,
    paths :: Array,
    state :: Array<Int64>,
    app :: vraylib.App,
    root :: String,
}
fn se_assets(app :: vraylib.App, paths :: Array, root :: String) -> SEAssets {
    tokens :: Array<Int64> := []; tokens.reserve(64); tokens.resize(64,0);
    state :: Array<Int64> := [0,0];
    return SEAssets(tokens,paths,state,app,root);
}
fn se_asset_relative(bank :: SEAssets,full :: String) -> String {
    if full.size() <= bank.root.size() { return full; }
    matches :: Bool := true;
    i :: Int64 := 0;
    while i < bank.root.size() { if full[i] != bank.root[i] { matches = false; } i = i + 1; }
    if !matches { return full; }
    result :: String := "";
    i = bank.root.size()+1;
    while i < full.size() { ch :: String := full[i]; if ch == "\\" { ch = "/"; } result = result+ch; i = i + 1; }
    return result;
}
fn se_assets_retry(bank :: SEAssets) -> Bool {
    kept :: Int64 := 0; i :: Int64 := 0;
    while i < bank.state[0] {
        if bank.tokens[i] > 0 { bank.tokens[kept] = bank.tokens[i]; se_text_copy(bank.paths[kept],bank.paths[i]); kept = kept+1; }
        i = i+1;
    }
    bank.state[0] = kept;
    return true;
}
fn se_asset(bank :: SEAssets, path :: String) -> vraylib.Model {
    i :: Int64 := 0;
    while i < bank.state[0] {
        if se_text_read(bank.paths[i]) == path {
            if bank.tokens[i] < 0 { throw "Asset unavailable: " + path; }
            return vraylib.Model(bank.tokens[i],bank.app.token);
        }
        i = i + 1;
    }
    if bank.state[0] >= 64 { throw "Asset cache capacity reached (64 models)"; }
    # Validate storage capacity before allocating the native resource.
    se_text_write(bank.paths[i],path);
    model :: vraylib.Model := vraylib.Model(0,bank.app.token);
    try {
        extension :: String := vfs.extension(path);
        if extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" {
            texture :: vraylib.Texture := bank.app.load_texture(path); model = bank.app.cube(texture);
        } else { model = bank.app.load_model(path); }
        bank.tokens[i] = model.token; bank.state[0] = i + 1;
    } catch (error) { bank.tokens[i] = -1; bank.state[0] = i+1; bank.state[1] = bank.state[1]+1; throw error; }
    return model;
}
fn se_node_model(scene :: SEScene, slot :: Int64, bank :: SEAssets, cubes :: Array) -> vraylib.Model {
    path :: String := se_text_read(scene.paths[scene.offset+slot]);
    if path != "" { return se_asset(bank,path); }
    return cubes[scene.meta[slot*8+5]];
}
fn se_update_asset_bounds(scene :: SEScene,cache :: SETransforms,bank :: SEAssets,cubes :: Array) -> Bool {
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot*8] != 0 && scene.meta[slot*8+3] == 1 {
            try {
                model :: vraylib.Model := se_node_model(scene,slot,bank,cubes);
                box :: vraylib.BoundingBox := model.bounds(se_matrix_read(cache.matrices,slot*16));
                cache.boxes[slot*6] = box.min.x; cache.boxes[slot*6+1] = box.min.y; cache.boxes[slot*6+2] = box.min.z;
                cache.boxes[slot*6+3] = box.max.x; cache.boxes[slot*6+4] = box.max.y; cache.boxes[slot*6+5] = box.max.z;
            } catch (error) {}
        }
        slot = slot+1;
    }
    return true;
}
fn se_draw_scene(scene :: SEScene, cache :: SETransforms, bank :: SEAssets, cubes :: Array, editor :: Bool, selected :: Int64) -> Bool {
    return se_draw_scene_resource(scene,cache,bank,cubes,editor,selected,scene.world);
}
fn se_draw_scene_resource(scene :: SEScene, cache :: SETransforms, bank :: SEAssets, cubes :: Array, editor :: Bool, selected :: Int64,world :: SEWorld) -> Bool {
    se_world_draw(world);
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot*8] != 0 {
            id :: Int64 := scene.handle(slot);
            if se_visible(scene,id) {
                kind :: Int64 := scene.meta[slot*8+3];
                if kind == 1 {
                    try {
                        model :: vraylib.Model := se_node_model(scene,slot,bank,cubes);
                        tint :: vraylib.Color := vraylib.rgba(int64(scene.values[slot*16+9]),int64(scene.values[slot*16+10]),int64(scene.values[slot*16+11]),int64(scene.values[slot*16+12]));
                        model.draw_matrix(se_matrix_read(cache.matrices,slot*16),tint);
                    } catch (error) { vraylib.draw_bounding_box(se_box(cache,slot),vraylib.MAGENTA); }
                }
                if editor {
                    position :: vraylib.Vector3 := se_position(cache,slot);
                    if kind == 2 { vraylib.draw_cube_wires(position,0.6,0.4,0.8,vraylib.SKYBLUE); }
                    if kind == 4 { vraylib.draw_cube_wires(vraylib.vec3(position.x,position.y+0.9,position.z),0.8,1.8,0.8,vraylib.ORANGE); }
                    if kind == 3 || selected == id {
                        box :: vraylib.BoundingBox := se_box(cache,slot);
                        vraylib.draw_bounding_box(box,vraylib.YELLOW);
                    }
                }
            }
        }
        slot = slot + 1;
    }
    return true;
}
fn se_pick(scene :: SEScene, cache :: SETransforms, bank :: SEAssets, cubes :: Array, ray :: vraylib.Ray) -> Int64 {
    nearest :: Float64 := 1000000000.0;
    selected :: Int64 := -1;
    if se_world_ray(scene.world,ray) { nearest = scene.world.work[4225]; selected = -2; }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot*8] != 0 {
            id :: Int64 := scene.handle(slot);
            if se_visible(scene,id) {
                kind :: Int64 := scene.meta[slot*8+3];
                if kind != 0 {
                    hit :: vraylib.RayCollision := vraylib.get_ray_collision_box(ray,se_box(cache,slot));
                    if kind == 1 { try { model :: vraylib.Model := se_node_model(scene,slot,bank,cubes); hit = model.pick(ray,se_matrix_read(cache.matrices,slot*16)); } catch (error) {} }
                    if hit.hit { if hit.distance < nearest { nearest = hit.distance; selected = id; } }
                }
            }
        }
        slot = slot + 1;
    }
    return selected;
}
