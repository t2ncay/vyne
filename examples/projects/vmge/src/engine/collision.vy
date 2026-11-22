use "render.vy";

# Compute the world-space axis-aligned bounds of a cube record.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     index (Int64): Zero-based entry index in the relevant store.
#
# Returns:
#     A temporary bounding box containing world-space minimum and maximum coordinates.
fn vmge_cube_box(world :: VMGEMap, index :: Int64) -> vraylib.BoundingBox {
    return vraylib.cube_box(
        world.value(index, 0),
        world.value(index, 1),
        world.value(index, 2),
        world.value(index, 3)
    );
}

# Construct the player's axis-aligned body bounds from eye position and standing/crouched height.
#
# Parameters:
#     position (vraylib.Vector3): Position or UTF-8 byte offset, according to the declared type.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     A temporary bounding box containing world-space minimum and maximum coordinates.
fn vmge_player_box(position :: vraylib.Vector3, height :: Float64) -> vraylib.BoundingBox {
    box :: vraylib.BoundingBox := vraylib.player_box(position, 0.8, height);
    # Raylib includes touching faces. A small vertical inset permits standing
    # on a cube without treating that support surface as a horizontal wall.
    box.min.y = box.min.y + 0.01;
    box.max.y = box.max.y - 0.01;
    return box;
}

# Test whether the player body intersects any cube in the legacy map.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     position (vraylib.Vector3): Position or UTF-8 byte offset, according to the declared type.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     True if the player body intersects a map cube; false otherwise.
fn vmge_blocked(world :: VMGEMap, position :: vraylib.Vector3, height :: Float64) -> Bool {
    player :: vraylib.BoundingBox := vmge_player_box(position, height);
    i :: Int64 := 0;
    while i < world.count() {
        if vraylib.check_collision_boxes(player, vmge_cube_box(world, i)) {
            return true;
        }
        i = i + 1;
    }
    return false;
}

# Resolve each horizontal axis separately, with bounded substeps to prevent
# tunneling at sprint speed. Test Z against the accepted X position.

# Move the player horizontally with bounded axis-wise substeps, blocking colliding axes to allow
# sliding.
#
# Parameters:
#     camera (vraylib.Camera): Live managed camera used for viewing, movement, or screen projection.
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     forward (Float64): Requested forward displacement in world units for this update.
#     right (Float64): Requested lateral displacement in world units for this update.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     True when the operation completes without throwing.
fn vmge_slide(
    camera :: vraylib.Camera,
    world :: VMGEMap,
    forward :: Float64,
    right :: Float64,
    height :: Float64
) -> Bool {
    steps :: Int64 := int64(vmath.ceil((vmath.abs(forward) + vmath.abs(right)) / 0.25));
    if steps < 1 {
        return true;
    }
    step :: Int64 := 0;
    while step < steps {
        previous :: vraylib.Vector3 := camera.position();
        camera.move(forward / float64(steps), right / float64(steps));
        candidate :: vraylib.Vector3 := camera.position();
        probe :: vraylib.Vector3 := vraylib.vec3(candidate.x, previous.y, previous.z);
        if vmge_blocked(world, probe, height) {
            candidate.x = previous.x;
        }
        probe = vraylib.vec3(candidate.x, previous.y, candidate.z);
        if vmge_blocked(world, probe, height) {
            candidate.z = previous.z;
        }
        camera.set_position(candidate);
        step = step + 1;
    }
    return true;
}

# Test horizontal overlap between the player footprint and a cube's axis-aligned bounds.
#
# Parameters:
#     position (vraylib.Vector3): Position or UTF-8 byte offset, according to the declared type.
#     box (vraylib.BoundingBox): World-space axis-aligned minimum/maximum bounds used by collision or
#         geometry queries.
#
# Returns:
#     True when the player footprint overlaps the cube in X/Z; false otherwise.
fn vmge_over_cube(position :: vraylib.Vector3, box :: vraylib.BoundingBox) -> Bool {
    return position.x
        +0.4 > box.min.x
        && position.x - 0.4 < box.max.x
        && position.z
        +0.4 > box.min.z
        && position.z - 0.4 < box.max.z;
}

# Find the highest supporting cube top below the player's feet, with the ground plane as fallback.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     position (vraylib.Vector3): Position or UTF-8 byte offset, according to the declared type.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     The computed Float64 value: floor.
fn vmge_floor(world :: VMGEMap, position :: vraylib.Vector3, height :: Float64) -> Float64 {
    floor :: Float64 := 0.0;
    i :: Int64 := 0;
    while i < world.count() {
        box :: vraylib.BoundingBox := vmge_cube_box(world, i);
        if vmge_over_cube(position, box) && box.max.y <= position.y - height + 0.15 {
            floor = vmath.max(floor, box.max.y);
        }
        i = i + 1;
    }
    return floor;
}

# Retained player controller: a managed camera plus numeric body height, vertical velocity, and
# grounded state.
#
# Fields:
#     camera: Managed first-person player camera, owned by the App.
#     state: Indices 0 eye-to-feet height, 1 vertical velocity, 2 grounded flag.
interface VMGEPlayer {
    camera :: vraylib.Camera,
    state :: Array,

    # Return the player's current eye-to-feet height in world units.
    #
    # Returns:
    #     The computed Float64 value: float64(self.state[0]).
    height() -> Float64 {
        return float64(self.state[0]);
    }

    # Report whether the controller currently has floor support.
    #
    # Returns:
    #     The Boolean result of self.state[2] != 0.0.
    grounded() -> Bool {
        return self.state[2] != 0.0;
    }

    # Reset the camera to the default spawn and restore standing height, zero velocity, and initial
    # grounding.
    #
    # Returns:
    #     True when the operation completes without throwing.
    reset() -> Bool {
        self.camera.set_position(vraylib.vec3(0.0, 1.8, 0.0));
        self.state[0] = 1.8;
        self.state[1] = 0.0;
        self.state[2] = 1.0;
        return true;
    }
}

# Create the managed player camera and retained controller state before entering managed frames.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#
# Returns:
#     VMGEPlayer containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn vmge_player(app :: vraylib.App) -> VMGEPlayer {
    camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 1.8, 0.0), vraylib.vec3(0.0, 1.8, 1.0), 80.0);
    return VMGEPlayer(camera, [1.8, 0.0, 1.0]);
}

# Advance legacy-map walking, sprinting, crouching, jumping, gravity, ceiling blocking, and landing.
#
# Parameters:
#     player (VMGEPlayer): Managed player camera plus retained height, vertical velocity, and grounded
#         flag.
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
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
fn vmge_player_update(
    player :: VMGEPlayer,
    world :: VMGEMap,
    input_forward :: Float64,
    input_right :: Float64,
    sprint :: Bool,
    crouch :: Bool,
    jump :: Bool,
    delta :: Float64
) -> Bool {
    dt :: Float64 := vmath.clamp(delta, 0.0, 0.05);
    forward :: Float64 := input_forward;
    right :: Float64 := input_right;
    position :: vraylib.Vector3 := player.camera.position();
    old_height :: Float64 := player.height();
    height :: Float64 := 1.8;
    if crouch {
        height = 0.9;
    }
    adjusted :: vraylib.Vector3 := vraylib.vec3(position.x, position.y + height - old_height, position.z);
    if height < old_height || !vmge_blocked(world, adjusted, height) {
        player.state[0] = height;
        player.camera.set_position(adjusted);
    } else {
        height = old_height;
    }
    speed :: Float64 := 9.0;
    if height < 1.0 {
        speed = 3.0;
    }
    if sprint {
        speed = speed * 2.5;
    }
    if forward != 0.0 && right != 0.0 {
        forward = forward * 0.70710678;
        right = right * 0.70710678;
    }
    vmge_slide(player.camera, world, forward * speed * dt, right * speed * dt, height);
    position = player.camera.position();
    floor :: Float64 := vmge_floor(world, position, height);
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
        if velocity <= 0.0 {
            landing :: Float64 := height;
            i :: Int64 := 0;
            while i < world.count() {
                box :: vraylib.BoundingBox := vmge_cube_box(world, i);
                if vmge_over_cube(position, box)
                    && position.y - height >= box.max.y - 0.01
                    && eye - height <= box.max.y {
                    landing = vmath.max(landing, box.max.y + height);
                }
                i = i + 1;
            }
            if eye <= landing {
                eye = landing;
                velocity = 0.0;
                player.state[2] = 1.0;
            }
        } else {
            i :: Int64 := 0;
            while i < world.count() {
                box :: vraylib.BoundingBox := vmge_cube_box(world, i);
                if vmge_over_cube(position, box) && position.y <= box.min.y && eye >= box.min.y {
                    eye = box.min.y;
                    velocity = 0.0;
                }
                i = i + 1;
            }
        }
        player.state[1] = velocity;
        player.camera.set_height(eye);
    }
    return true;
}
