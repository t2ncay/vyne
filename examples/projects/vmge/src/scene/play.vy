use "assets.vy";
use "../engine/collision.vy";
use "../engine/world_character_motion.vy";

# Authoring and play stores never share numeric/text payloads. Geometry queries
# use conservative world AABBs (rotated box collision is intentionally AABB).

# Deep-copy authoring into Play, choose the active camera and first character, and optionally resolve
# a generated-pad spawn.
#
# Parameters:
#     authored (SEScene): Authored scene owner; mutable payloads are separate from Play storage.
#     runtime (SEScene): Isolated Play scene owner; runtime edits do not modify authoring.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     player (VMGEPlayer): Managed player camera plus retained height, vertical velocity, and grounded
#         flag.
#
# Returns:
#     The first live character handle, or -1 when the runtime has no character.
#
# Lifecycle:
#     Authoring and runtime must have distinct numeric/text/world/material owners; starting Play
#     copies payloads without sharing mutable authoring state.
fn se_play_start(
    authored :: SEScene,
    runtime :: SEScene,
    cache :: SETransforms,
    player :: VMGEPlayer
) -> Int64 {
    se_copy(runtime, authored);
    se_update_transforms(runtime, cache);
    player.reset();
    character :: Int64 := -1;
    camera :: Int64 := se_active_node(runtime, 2);
    slot :: Int64 := 0;
    while slot < 128 {
        if runtime.meta[slot * 8] != 0 {
            if runtime.meta[slot * 8 + 3] == 4 && character == -1 {
                character = runtime.handle(slot);
            }
        }
        slot = slot + 1;
    }
    if camera != -1 {
        matrix :: vraylib.Matrix := se_world(runtime, cache, camera);
        eye :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(0.0, 0.0, 0.0), matrix);
        target :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(0.0, 0.0, 1.0), matrix);
        player.camera.set_view(eye, target);
        snapshot :: vraylib.Camera3D := player.camera.view();
        snapshot.fovy = runtime.number(camera, 13);
        vraylib.buffer_write(
            vraylib.require_managed(player.camera.session, player.camera.token),
            0,
            snapshot
        );
    } else {
        if character != -1 {
            position :: vraylib.Vector3 := se_position(cache, character % 128);
            player.camera.set_position(vraylib.vec3(position.x, position.y + 1.8, position.z));
        }
    }
    if vmge_setting("VYNE_VMGE_SPAWN_PAD", 0) == 1 && se_world_spawn_point(runtime.world, player.height()) {
        player.camera.set_position(vraylib.vec3(
            runtime.world.work[4235],
            runtime.world.work[4236],
            runtime.world.work[4237]
        ));
        player.state[1] = 0.0;
        player.state[2] = 1.0;
    }
    return character;
}

# Advance the runtime character and write its feet position back in parent-local coordinates; no-op
# when character is -1.
#
# Parameters:
#     runtime (SEScene): Isolated Play scene owner; runtime edits do not modify authoring.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     player (VMGEPlayer): Managed player camera plus retained height, vertical velocity, and grounded
#         flag.
#     character (Int64): Controlled character handle; -1 means no character was found.
#     forward (Float64): Signed forward movement input; negative moves backward.
#     right (Float64): Signed lateral movement input; negative moves left.
#     sprint (Bool): Whether to apply the sprint speed multiplier.
#     crouch (Bool): Whether to request the shorter crouched player body.
#     jump (Bool): Edge-triggered jump request, accepted only while grounded.
#     dt (Float64): Frame delta time in seconds.
#
# Returns:
#     True when the operation completes without throwing.
fn se_play_update(
    runtime :: SEScene,
    cache :: SETransforms,
    player :: VMGEPlayer,
    character :: Int64,
    forward :: Float64,
    right :: Float64,
    sprint :: Bool,
    crouch :: Bool,
    jump :: Bool,
    dt :: Float64
) -> Bool {
    if character == -1 {
        return true;
    }
    se_character_update(runtime, cache, player, character, forward, right, sprint, crouch, jump, dt);
    position :: vraylib.Vector3 := player.camera.position();
    feet :: vraylib.Vector3 := vraylib.vec3(position.x, position.y - player.height(), position.z);
    parent :: Int64 := runtime.parent(character);
    if parent != -1 {
        feet = vraylib.math_vector3_transform(
            feet,
            vraylib.math_matrix_invert(se_world(runtime, cache, parent))
        );
    }
    runtime.property(character, 0, feet.x);
    runtime.property(character, 1, feet.y);
    runtime.property(character, 2, feet.z);
    se_update_transforms(runtime, cache);
    return true;
}

# Report whether a live mesh/collision node belongs outside the excluded character subtree.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#     exclude (Int64): Character subtree handle to omit from collision queries; -1 excludes nothing.
#
# Returns:
#     True for a live Mesh/Collision node outside the excluded subtree; false otherwise.
fn se_collidable(scene :: SEScene, slot :: Int64, exclude :: Int64) -> Bool {
    if scene.meta[slot * 8] == 0 {
        return false;
    }
    kind :: Int64 := scene.meta[slot * 8 + 3];
    if kind != 1 && kind != 3 {
        return false;
    }
    if exclude != -1 {
        if scene.descendant(scene.handle(slot), exclude) {
            return false;
        }
    }
    return true;
}

# Test a player body against world cells, terrain penetration, and conservative scene-node bounds.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     position (vraylib.Vector3): Position or UTF-8 byte offset, according to the declared type.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#     exclude (Int64): Character subtree handle to omit from collision queries; -1 excludes nothing.
#
# Returns:
#     True for intersecting cells/node bounds or terrain penetration; false when the body is clear.
fn se_body_blocked(
    scene :: SEScene,
    cache :: SETransforms,
    position :: vraylib.Vector3,
    height :: Float64,
    exclude :: Int64
) -> Bool {
    body :: vraylib.BoundingBox := vmge_player_box(position, height);
    data :: Array<Float64> := scene.world.data;
    count :: Int64 := int64(data[0]);
    i :: Int64 := 0;
    while i < count {
        p :: Int64 := 16 + i * 5;
        r :: Float64 := data[p + 3] * 0.5;
        if body.min.x <= data[p]
            +r
            && body.max.x >= data[p] - r
            && body.min.y <= data[p + 1]
            +r
            && body.max.y >= data[p + 1] - r
            && body.min.z <= data[p + 2]
            +r
            && body.max.z >= data[p + 2] - r {
            return true;
        }
        i = i + 1;
    }
    if se_terrain_contains(scene.world, position.x, position.z) {
        if position.y - height < se_terrain_height(scene.world, position.x, position.z) - 0.15 {
            return true;
        }
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if se_collidable(scene, slot, exclude) {
            if vraylib.check_collision_boxes(body, se_box(cache, slot)) {
                return true;
            }
        }
        slot = slot + 1;
    }
    return false;
}

# Advance scene/world character movement with clamped time, sliding, slope support, crouch clearance,
# jump/gravity, ceilings, and landing.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     player (VMGEPlayer): Managed player camera plus retained height, vertical velocity, and grounded
#         flag.
#     character (Int64): Controlled character handle; -1 means no character was found.
#     input_forward (Float64): Signed forward movement input, normalized with lateral input for
#         diagonal movement.
#     input_right (Float64): Signed lateral movement input, normalized with forward input for diagonal
#         movement.
#     sprint (Bool): Whether to apply the sprint speed multiplier.
#     crouch (Bool): Whether to request the shorter crouched player body.
#     jump (Bool): Edge-triggered jump request, accepted only while grounded.
#     delta (Float64): Frame delta time in seconds; character motion clamps it to at most 0.05.
#
# Returns:
#     True when the operation completes without throwing.
fn se_character_update(
    scene :: SEScene,
    cache :: SETransforms,
    player :: VMGEPlayer,
    character :: Int64,
    input_forward :: Float64,
    input_right :: Float64,
    sprint :: Bool,
    crouch :: Bool,
    jump :: Bool,
    delta :: Float64
) -> Bool {
    dt :: Float64 := vmath.clamp(delta, 0.0, 0.05);
    position :: vraylib.Vector3 := player.camera.position();
    height :: Float64 := 1.8;
    if crouch {
        height = 0.9;
    }
    adjusted :: vraylib.Vector3 := vraylib.vec3(
        position.x,
        position.y + height - player.height(),
        position.z
    );
    if height < player.height() || !se_body_blocked(scene, cache, adjusted, height, character) {
        player.state[0] = height;
        player.camera.set_position(adjusted);
    } else {
        height = player.height();
    }
    speed :: Float64 := 9.0;
    if height < 1.0 {
        speed = 3.0;
    }
    if sprint {
        speed = speed * 2.5;
    }
    forward :: Float64 := input_forward;
    right :: Float64 := input_right;
    if forward != 0.0 && right != 0.0 {
        forward = forward * 0.70710678;
        right = right * 0.70710678;
    }
    steps :: Int64 := int64(vmath.ceil((vmath.abs(forward) + vmath.abs(right)) * speed * dt / 0.25));
    step :: Int64 := 0;
    while step < steps {
        previous :: vraylib.Vector3 := player.camera.position();
        player.camera.move(forward * speed * dt / float64(steps), right * speed * dt / float64(steps));
        candidate :: vraylib.Vector3 := player.camera.position();
        candidate.y = previous.y;
        if se_world_motion_blocked(
            scene,
            cache,
            previous,
            vraylib.vec3(candidate.x, previous.y, previous.z),
            height,
            character,
            player.grounded()
        ) {
            candidate.x = previous.x;
        } else {
            candidate.y = scene.world.work[4238];
        }
        if se_world_motion_blocked(
            scene,
            cache,
            vraylib.vec3(candidate.x, candidate.y, previous.z),
            vraylib.vec3(candidate.x, candidate.y, candidate.z),
            height,
            character,
            player.grounded()
        ) {
            candidate.z = previous.z;
        } else {
            candidate.y = scene.world.work[4238];
        }
        player.camera.set_position(candidate);
        step = step + 1;
    }
    position = player.camera.position();
    floor :: Float64 := 0.0;
    if se_terrain_contains(scene.world, position.x, position.z) {
        floor = se_terrain_height(scene.world, position.x, position.z);
    }
    world_data :: Array<Float64> := scene.world.data;
    world_count :: Int64 := int64(world_data[0]);
    wi :: Int64 := 0;
    while wi < world_count {
        p :: Int64 := 16 + wi * 5;
        r :: Float64 := world_data[p + 3] * 0.5;
        if vmath.abs(position.x - world_data[p]) < r
            +0.4
            && vmath.abs(position.z - world_data[p + 2]) < r
            +0.4
            && world_data[p + 1]
            +r <= position.y - height
            +0.15 {
            floor = vmath.max(floor, world_data[p + 1] + r);
        }
        wi = wi + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if se_collidable(scene, slot, character) {
            box :: vraylib.BoundingBox := se_box(cache, slot);
            if vmge_over_cube(position, box) && box.max.y <= position.y - height + 0.15 {
                floor = vmath.max(floor, box.max.y);
            }
        }
        slot = slot + 1;
    }
    if position.y - height > floor + 0.15 {
        player.state[2] = 0.0;
    }
    if jump && player.grounded() {
        player.state[1] = 21.0;
        player.state[2] = 0.0;
    }
    if player.grounded() {
        player.camera.set_height(floor + height);
    } else {
        velocity :: Float64 := float64(player.state[1]) - 43.2 * dt;
        eye :: Float64 := position.y + velocity * dt;
        landing :: Float64 := height;
        if se_terrain_contains(scene.world, position.x, position.z) {
            landing = se_terrain_height(scene.world, position.x, position.z) + height;
        }
        wi = 0;
        while wi < world_count {
            p :: Int64 := 16 + wi * 5;
            r :: Float64 := world_data[p + 3] * 0.5;
            if vmath.abs(position.x - world_data[p]) < r
                +0.4
                && vmath.abs(position.z - world_data[p + 2]) < r
                +0.4 {
                top :: Float64 := world_data[p + 1] + r;
                bottom :: Float64 := world_data[p + 1] - r;
                if velocity <= 0.0 && position.y - height >= top - 0.01 && eye - height <= top {
                    landing = vmath.max(landing, top + height);
                }
                if velocity > 0.0 && position.y <= bottom && eye >= bottom {
                    eye = bottom;
                    velocity = 0.0;
                }
            }
            wi = wi + 1;
        }
        slot = 0;
        while slot < 128 {
            if se_collidable(scene, slot, character) {
                box :: vraylib.BoundingBox := se_box(cache, slot);
                if vmge_over_cube(position, box) {
                    if velocity <= 0.0 {
                        if position.y - height >= box.max.y - 0.01 && eye - height <= box.max.y {
                            landing = vmath.max(landing, box.max.y + height);
                        }
                    } else {
                        if position.y <= box.min.y && eye >= box.min.y {
                            eye = box.min.y;
                            velocity = 0.0;
                        }
                    }
                }
            }
            slot = slot + 1;
        }
        if velocity <= 0.0 && eye <= landing {
            eye = landing;
            velocity = 0.0;
            player.state[2] = 1.0;
        }
        player.state[1] = velocity;
        player.camera.set_height(eye);
    }
    return true;
}
