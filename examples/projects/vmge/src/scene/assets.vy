use "transforms.vy";
use "../engine/assets.vy";
use "../engine/world_render.vy";
use "environment.vy";

# Persist only numeric resource tokens. Reconstruct thin wrappers in the frame.
# Path slots and their boxed headers are preacquired from the owner slab.

# Session asset owner caching models, paths, texture bindings, shader programs, environment programs,
# reusable light values, and camera position. Native resources remain owned by the App.
#
# Fields:
#     tokens: Cached managed model tokens parallel to retained canonical model paths.
#     paths: Retained canonical model paths for cache lookup.
#     state: Model-cache count/failure flags and texture-cache counters.
#     app: Live managed application that owns the graphics context and native resources.
#     root: Project or resource root used to resolve relative paths.
#     programs: Retained programs payload.
#     texture_tokens: Managed texture tokens cached separately from persistent resource IDs.
#     texture_paths: Retained paths parallel to the cached material textures.
#     view: Current camera XYZ uploaded to custom lighting shaders.
#     lights: Reusable ambient/sun/four-point-light values collected before drawing.
#     environment_tokens: Managed lit/sky program tokens retained for the application session.
interface SEAssets {
    tokens :: Array<Int64>,
    paths :: Array,
    state :: Array<Int64>,
    app :: vraylib.App,
    root :: String,
    programs :: SEPrograms,
    texture_tokens :: Array<Int64>,
    texture_paths :: Array,
    view :: Array<Float64>,
    lights :: Array<Float64>,
    environment_tokens :: Array<Int64>,
}

# Create the preframe scene asset/cache owner and environment support for a project root.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     paths (Array): Preallocated retained resource-path buffers.
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     SEAssets containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_assets(app :: vraylib.App, paths :: Array, root :: String) -> SEAssets {
    tokens :: Array<Int64> := [];
    tokens.reserve(64);
    tokens.resize(64, 0);
    state :: Array<Int64> := ss_ints(3);
    texture_paths :: Array := [];
    texture_paths.reserve(64);
    i :: Int64 := 0;
    while i < 64 {
        texture_paths.push(ss_text(255));
        i = i + 1;
    }
    return SEAssets(
        tokens,
        paths,
        state,
        app,
        root,
        se_programs(),
        ss_ints(64),
        texture_paths,
        ss_floats(3),
        ss_floats(48),
        ss_ints(2)
    );
}

# Collect current scene lights and lazily prepare the managed environment programs needed for drawing.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#
# Returns:
#     True when the operation completes without throwing.
fn se_environment_prepare(scene :: SEScene, cache :: SETransforms, bank :: SEAssets) -> Bool {
    se_light_collect(scene, cache, bank.lights);
    if bank.lights[0] != 0.0 && bank.environment_tokens[0] == 0 {
        shader :: vraylib.Shader := bank.app.load_shader(
            vfs.join(bank.root, "shaders/environment/lit.vs"),
            vfs.join(bank.root, "shaders/environment/lit.fs")
        );
        bank.environment_tokens[0] = shader.token;
    }
    if se_active_node(scene, 5) != -1 && bank.environment_tokens[1] == 0 {
        shader :: vraylib.Shader := bank.app.load_shader(
            null,
            vfs.join(bank.root, "shaders/environment/sky.fs")
        );
        bank.environment_tokens[1] = shader.token;
    }
    return true;
}

# Convert a full asset path into a project-relative reference when it lies beneath the asset root.
#
# Parameters:
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     full (String): Full asset path to convert into a retained project reference.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_asset_relative(bank :: SEAssets, full :: String) -> String {
    if full.size() <= bank.root.size() {
        return full;
    }
    matches :: Bool := true;
    i :: Int64 := 0;
    while i < bank.root.size() {
        if full[i] != bank.root[i] {
            matches = false;
        }
        i = i + 1;
    }
    if !matches {
        return full;
    }
    result :: String := "";
    i = bank.root.size() + 1;
    while i < full.size() {
        ch :: String := full[i];
        if ch == "\\" {
            ch = "/";
        }
        result = result + ch;
        i = i + 1;
    }
    return result;
}

# Clear remembered asset-load failures so subsequent requests may retry.
#
# Parameters:
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#
# Returns:
#     True when the operation completes without throwing.
fn se_assets_retry(bank :: SEAssets) -> Bool {
    kept :: Int64 := 0;
    i :: Int64 := 0;
    while i < bank.state[0] {
        if bank.tokens[i] > 0 {
            bank.tokens[kept] = bank.tokens[i];
            se_text_copy(bank.paths[kept], bank.paths[i]);
            kept = kept + 1;
        }
        i = i + 1;
    }
    bank.state[0] = kept;
    return true;
}

# Resolve or load a cached model by path, tracking bounded capacity and remembered failures.
#
# Parameters:
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The vraylib.Model value selected by the conditions described above.
#
# Errors:
#     Asset unavailable (with contextual detail).
#     Asset cache capacity reached (64 models).
fn se_asset(bank :: SEAssets, path :: String) -> vraylib.Model {
    i :: Int64 := 0;
    while i < bank.state[0] {
        if se_text_read(bank.paths[i]) == path {
            if bank.tokens[i] < 0 {
                throw "Asset unavailable: " + path;
            }
            return vraylib.Model(bank.tokens[i], bank.app.token);
        }
        i = i + 1;
    }
    if bank.state[0] >= 64 {
        throw "Asset cache capacity reached (64 models)";
    }
    # Validate storage capacity before allocating the native resource.
    se_text_write(bank.paths[i], path);
    model :: vraylib.Model := vraylib.Model(0, bank.app.token);
    try {
        extension :: String := vfs.extension(path);
        if extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" {
            texture :: vraylib.Texture := bank.app.load_texture(path);
            model = bank.app.cube(texture);
        } else {
            model = bank.app.load_model(path);
        }
        bank.tokens[i] = model.token;
        bank.state[0] = i + 1;
    } catch (error) {
        bank.tokens[i] = -1;
        bank.state[0] = i + 1;
        bank.state[1] = bank.state[1] + 1;
        throw error;
    }
    return model;
}

# Resolve a mesh node's external model or reusable cube model for its texture slot.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     cubes (Array): Reusable cube models indexed by palette/texture slot.
#
# Returns:
#     The vraylib.Model value selected by the conditions described above.
fn se_node_model(scene :: SEScene, slot :: Int64, bank :: SEAssets, cubes :: Array) -> vraylib.Model {
    path :: String := se_text_read(scene.paths[scene.offset + slot]);
    if path != "" {
        return se_asset(bank, path);
    }
    return cubes[scene.meta[slot * 8 + 5]];
}

# Refresh cached world-space bounds using the actual asset model geometry for live mesh nodes.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     cubes (Array): Reusable cube models indexed by palette/texture slot.
#
# Returns:
#     True when the operation completes without throwing.
fn se_update_asset_bounds(scene :: SEScene, cache :: SETransforms, bank :: SEAssets, cubes :: Array) -> Bool {
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 && scene.meta[slot * 8 + 3] == 1 {
            try {
                model :: vraylib.Model := se_node_model(scene, slot, bank, cubes);
                box :: vraylib.BoundingBox := model.bounds(se_matrix_read(cache.matrices, slot * 16));
                cache.boxes[slot * 6] = box.min.x;
                cache.boxes[slot * 6 + 1] = box.min.y;
                cache.boxes[slot * 6 + 2] = box.min.z;
                cache.boxes[slot * 6 + 3] = box.max.x;
                cache.boxes[slot * 6 + 4] = box.max.y;
                cache.boxes[slot * 6 + 5] = box.max.z;
            } catch (error) {
            }
        }
        slot = slot + 1;
    }
    return true;
}

# Draw the scene's own world resource and visible nodes using cached models and independent material
# overrides.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     cubes (Array): Reusable cube models indexed by palette/texture slot.
#     editor (Bool): Retained code-editor owner, or editor-visuals flag for scene drawing.
#     selected (Int64): Selected scene handle; negative values represent no node or the world
#         resource.
#
# Returns:
#     The Boolean result of se_draw_scene_resource(scene, cache, bank, cubes, editor, selected,
#     scene.world).
fn se_draw_scene(
    scene :: SEScene,
    cache :: SETransforms,
    bank :: SEAssets,
    cubes :: Array,
    editor :: Bool,
    selected :: Int64
) -> Bool {
    return se_draw_scene_resource(scene, cache, bank, cubes, editor, selected, scene.world);
}

# Draw visible scene nodes with a supplied world/preview resource and optional editor
# helpers/selection outlines.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     cubes (Array): Reusable cube models indexed by palette/texture slot.
#     editor (Bool): Retained code-editor owner, or editor-visuals flag for scene drawing.
#     selected (Int64): Selected scene handle; negative values represent no node or the world
#         resource.
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#
# Returns:
#     True when the operation completes without throwing.
fn se_draw_scene_resource(
    scene :: SEScene,
    cache :: SETransforms,
    bank :: SEAssets,
    cubes :: Array,
    editor :: Bool,
    selected :: Int64,
    world :: SEWorld
) -> Bool {
    bank.lights[9] = bank.view[0];
    bank.lights[10] = bank.view[1];
    bank.lights[11] = bank.view[2];
    if bank.lights[0] != 0.0 && bank.environment_tokens[0] != 0 {
        shader :: vraylib.Shader := vraylib.Shader(bank.environment_tokens[0], bank.app.token);
        se_light_upload(shader, bank.lights, true);
        shader.set("colDiffuse", vraylib.Vector4(1.0, 1.0, 1.0, 1.0));
        shader.begin();
        try {
            se_world_draw(world);
        } finally {
            shader.end();
        }
    } else {
        se_world_draw(world);
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            id :: Int64 := scene.handle(slot);
            if se_visible(scene, id) {
                kind :: Int64 := scene.meta[slot * 8 + 3];
                if kind == 1 {
                    try {
                        model :: vraylib.Model := se_node_model(scene, slot, bank, cubes);
                        tint :: vraylib.Color := vraylib.rgba(
                            int64(scene.values[slot * 16 + 9]),
                            int64(scene.values[slot * 16 + 10]),
                            int64(scene.values[slot * 16 + 11]),
                            int64(scene.values[slot * 16 + 12])
                        );
                        resource :: String := se_text_read(scene.materials.paths[slot]);
                        if resource == "" {
                            if bank.lights[0] != 0.0 && bank.environment_tokens[0] != 0 {
                                shader :: vraylib.Shader := vraylib.Shader(
                                    bank.environment_tokens[0],
                                    bank.app.token
                                );
                                se_light_upload(shader, bank.lights, false);
                                model.draw_material(
                                    se_matrix_read(cache.matrices, slot * 16),
                                    shader,
                                    0,
                                    tint
                                );
                            } else {
                                model.draw_matrix(se_matrix_read(cache.matrices, slot * 16), tint);
                            }
                        } else {
                            if !vraylib.is_path_absolute(resource) {
                                resource = vfs.join(bank.root, resource);
                            }
                            program :: Int64 := se_program_resolve(bank.programs, bank.app, resource);
                            se_material_resolve_textures(
                                scene.materials,
                                scene.materials.instances[slot],
                                bank
                            );
                            d :: SSDocument := bank.programs.documents[program];
                            ss_material_apply(d, scene.materials.instances[slot]);
                            se_light_custom(ss_program(d), bank.lights);
                            parameter :: Int64 := 0;
                            while parameter < d.published.state[0] {
                                if d.published.meta[parameter * 8 + 1] == 35665
                                    && d.published.meta[parameter * 8 + 2] == 1
                                    && d.published.meta[parameter * 8 + 6] == 1 {
                                    if se_text_read(d.published.names[parameter]) == "cameraPosition" {
                                        ss_program(d).set(
                                            "cameraPosition",
                                            vraylib.vec3(bank.view[0], bank.view[1], bank.view[2])
                                        );
                                    }
                                }
                                parameter = parameter + 1;
                            }
                            model.draw_material(
                                se_matrix_read(cache.matrices, slot * 16),
                                ss_program(d),
                                0,
                                tint
                            );
                        }
                    } catch (error) {
                        vraylib.draw_bounding_box(se_box(cache, slot), vraylib.MAGENTA);
                    }
                }
                if editor {
                    position :: vraylib.Vector3 := se_position(cache, slot);
                    if kind == 2 {
                        vraylib.draw_cube_wires(position, 0.6, 0.4, 0.8, vraylib.SKYBLUE);
                    }
                    if kind == 4 {
                        vraylib.draw_cube_wires(
                            vraylib.vec3(position.x, position.y + 0.9, position.z),
                            0.8,
                            1.8,
                            0.8,
                            vraylib.ORANGE
                        );
                    }
                    if kind == 6 {
                        direction :: vraylib.Vector3 := vraylib.vec3(
                            cache.matrices[slot * 16 + 2],
                            cache.matrices[slot * 16 + 6],
                            cache.matrices[slot * 16 + 10]
                        );
                        vraylib.draw_line_3d(
                            position,
                            vraylib.vec3(
                                position.x + direction.x * 2.0,
                                position.y + direction.y * 2.0,
                                position.z + direction.z * 2.0
                            ),
                            vraylib.YELLOW
                        );
                        vraylib.draw_sphere_wires(position, 0.25, 6, 8, vraylib.YELLOW);
                    }
                    if kind == 7 {
                        radius :: Float64 := 0.3;
                        if selected == id {
                            radius = scene.extra[slot * 16 + 1];
                        }
                        vraylib.draw_sphere_wires(position, radius, 8, 12, vraylib.ORANGE);
                    }
                    if kind == 3 || selected == id {
                        box :: vraylib.BoundingBox := se_box(cache, slot);
                        vraylib.draw_bounding_box(box, vraylib.YELLOW);
                    }
                }
            }
        }
        slot = slot + 1;
    }
    return true;
}

# Find the nearest visible node or world-resource ray hit; return a node handle, -2 for world
# geometry, or -1 for no hit.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     cubes (Array): Reusable cube models indexed by palette/texture slot.
#     ray (vraylib.Ray): World-space picking ray with origin and direction.
#
# Returns:
#     Nearest node handle, -2 for world geometry, or -1 for no hit.
fn se_pick(
    scene :: SEScene,
    cache :: SETransforms,
    bank :: SEAssets,
    cubes :: Array,
    ray :: vraylib.Ray
) -> Int64 {
    nearest :: Float64 := 1000000000.0;
    selected :: Int64 := -1;
    if se_world_ray(scene.world, ray) {
        nearest = scene.world.work[4225];
        selected = -2;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            id :: Int64 := scene.handle(slot);
            if se_visible(scene, id) {
                kind :: Int64 := scene.meta[slot * 8 + 3];
                if kind != 0 {
                    hit :: vraylib.RayCollision := vraylib.get_ray_collision_box(ray, se_box(cache, slot));
                    if kind == 1 {
                        try {
                            model :: vraylib.Model := se_node_model(scene, slot, bank, cubes);
                            hit = model.pick(ray, se_matrix_read(cache.matrices, slot * 16));
                        } catch (error) {
                        }
                    }
                    if hit.hit {
                        if hit.distance < nearest {
                            nearest = hit.distance;
                            selected = id;
                        }
                    }
                }
            }
        }
        slot = slot + 1;
    }
    return selected;
}

# Resolve or load a cached material texture by persisted path under explicit size and upload budgets.
#
# Parameters:
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The vraylib.Texture value selected by the conditions described above.
#
# Errors:
#     Host texture cache capacity: 64.
#     Texture reference exceeds 255 bytes.
#     Host texture budget: 4096 per axis / 128 MiB RGBA8 total.
fn se_material_texture(bank :: SEAssets, path :: String) -> vraylib.Texture {
    i :: Int64 := 0;
    while i < 64 {
        if bank.texture_tokens[i] != 0 && se_text_read(bank.texture_paths[i]) == path {
            return vraylib.Texture(bank.texture_tokens[i], bank.app.token, false);
        }
        i = i + 1;
    }
    i = 0;
    while i < 64 && bank.texture_tokens[i] != 0 {
        i = i + 1;
    }
    if i == 64 {
        throw "Host texture cache capacity: 64";
    }
    full :: String := path;
    if !vraylib.is_path_absolute(full) {
        full = vfs.join(bank.root, full);
    }
    if path.size() > 255 {
        throw "Texture reference exceeds 255 bytes";
    }
    t :: vraylib.Texture := bank.app.load_texture(full);
    bytes :: Int64 := t.width() * t.height() * 4;
    if t.width() > 4096 || t.height() > 4096 || bytes + bank.state[2] > 134217728 {
        t.close();
        throw "Host texture budget: 4096 per axis / 128 MiB RGBA8 total";
    }
    se_text_write(bank.texture_paths[i], path);
    bank.texture_tokens[i] = t.token;
    bank.state[2] = bank.state[2] + bytes;
    return t;
}

# Resolve persistent material sampler-resource IDs into borrowed native texture bindings for the
# current session.
#
# Parameters:
#     resources (SEMaterials): Scene material instances, shader references, and persistent texture-
#         resource paths.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     bank (SEAssets): Session asset/model/texture/program cache used to resolve retained resource
#         references.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Missing scene texture resource reference.
fn se_material_resolve_textures(resources :: SEMaterials, m :: MaterialInstance, bank :: SEAssets) -> Bool {
    i :: Int64 := 0;
    while i < m.binding_state[0] {
        id :: Int64 := m.bindings[i * 4 + 1] - 100;
        if id < 0 || id >= resources.resource_state[0] {
            throw "Missing scene texture resource reference";
        }
        t :: vraylib.Texture := se_material_texture(bank, se_text_read(resources.resources[id]));
        ss_material_bind_texture(m, m.bindings[i * 4], id + 100, t);
        i = i + 1;
    }
    return true;
}
