use "map.vy";
use external "vraylib/vraylib.vy";

# Resource layout (Float64 throughout, no boxed cell records):
# 0 count,1 terrain quads/side,2 spacing,3..5 origin,6 seed,7 preset.
# 16..20495: 4096 x (center xyz,size,palette). 20496..24720: 65x65 heights.
# All owners, including staging and history, are acquired before frames.

# Fixed scene-owned hybrid world. The data buffer holds a 16-number header, 4,096 five-number cube
# records, and a 65-by-65 height grid. Scratch and reusable drawing records are owned before frames.
#
# Fields:
#     data: 24,736 Float64 elements: header, 4,096 five-field cells, and fixed-stride 65-by-65
#         heights.
#     work: 4,256 Float64 elements reused for sculpt snapshots, ray results, serialization, and
#         motion.
#     points: Four preacquired mutable Vector3 records reused by terrain triangle drawing.
#     colors: Eight cube palette colors and two terrain triangle colors.
interface SEWorld {
    data :: Array<Float64>,
    work :: Array<Float64>,
    points :: Array,
    colors :: Array,

    # Return the number of live map-resource cells.
    #
    # Returns:
    #     The live entry count.
    count() -> Int64 {
        return int64(self.data[0]);
    }

    # Return terrain quads per side; zero means no terrain is present.
    #
    # Returns:
    #     Terrain quads per side, or zero when terrain is absent.
    side() -> Int64 {
        return int64(self.data[1]);
    }
}

# Allocate and zero the fixed 24,736-element Float64 world payload used by owners and history
# snapshots.
#
# Returns:
#     A zeroed 24,736-element Float64 payload buffer.
fn se_world_buffer() -> Array<Float64> {
    data :: Array<Float64> := [];
    data.reserve(24736);
    data.resize(24736, 0.0);
    return data;
}

# Create a world owner with fixed data/scratch buffers, four reusable vertices, and palette/terrain
# colors.
#
# Returns:
#     A new fixed-capacity world owner with initialized scratch/vertices/colors.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_world_create() -> SEWorld {
    data :: Array<Float64> := se_world_buffer();
    data[2] = 1.0;
    work :: Array<Float64> := [];
    work.reserve(4256);
    work.resize(4256, 0.0);
    points :: Array := [
        vraylib.vec3(0.0, 0.0, 0.0),
        vraylib.vec3(0.0, 0.0, 0.0),
        vraylib.vec3(0.0, 0.0, 0.0),
        vraylib.vec3(0.0, 0.0, 0.0)
    ];
    colors :: Array := [];
    colors.reserve(10);
    i :: Int64 := 0;
    while i < 8 {
        colors.push(vraylib.rgba(100 + i * 17, 135 + i * 9, 85 + i * 15, 255));
        i = i + 1;
    }
    colors.push(vraylib.rgba(89, 139, 78, 255));
    colors.push(vraylib.rgba(95, 145, 83, 255));
    return SEWorld(data, work, points, colors);
}

# Copy the complete fixed world payload into an existing destination buffer.
#
# Parameters:
#     destination (Array<Float64>): Destination numeric payload buffer.
#     source (Array<Float64>): Source numeric payload buffer.
#
# Returns:
#     True when the operation completes without throwing.
fn se_world_copy_data(destination :: Array<Float64>, source :: Array<Float64>) -> Bool {
    i :: Int64 := 0;
    while i < 24736 {
        destination[i] = source[i];
        i = i + 1;
    }
    return true;
}

# Copy a world's authored numeric payload without replacing its scratch, vertex, or color owners.
#
# Parameters:
#     destination (SEWorld): Existing destination owner to receive the copied/validated payload.
#     source (SEWorld): Source owner whose payload is read or copied by this operation.
#
# Returns:
#     True after the complete numeric world payload is copied.
fn se_world_copy(destination :: SEWorld, source :: SEWorld) -> Bool {
    return se_world_copy_data(destination.data, source.data);
}

# Clear the world header and restore default spacing; old payload rows become inactive without
# reallocating storage.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#
# Returns:
#     True when the operation completes without throwing.
fn se_world_clear(world :: SEWorld) -> Bool {
    i :: Int64 := 0;
    while i < 16 {
        world.data[i] = 0.0;
        i = i + 1;
    }
    world.data[2] = 1.0;
    return true;
}

# Find the first map cell with the exact supplied center; return -1 if absent.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     The first matching zero-based cell index, or -1 when absent.
fn se_world_cell(world :: SEWorld, x :: Float64, y :: Float64, z :: Float64) -> Int64 {
    return se_world_cell_data(world.data, x, y, z);
}

# Find the first exact-center cell in a raw world payload; return -1 if absent.
#
# Parameters:
#     data (Array<Float64>): Numeric backing storage; offsets follow the containing resource layout.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     The first matching zero-based cell index, or -1 when absent.
fn se_world_cell_data(data :: Array<Float64>, x :: Float64, y :: Float64, z :: Float64) -> Int64 {
    count :: Int64 := int64(data[0]);
    i :: Int64 := 0;
    while i < count {
        p :: Int64 := 16 + i * 5;
        if data[p] == x && data[p + 1] == y && data[p + 2] == z {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

# Paint or update an exact-center world cell, returning whether a new cell or changed size/palette was
# written.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#     size (Float64): Positive cube side length in world units, at most 1,000,000.
#     palette (Int64): Zero-based world color slot, from 0 through 7.
#
# Returns:
#     True when a cell was added or its size/palette changed; false for unchanged paint.
fn se_world_paint(
    world :: SEWorld,
    x :: Float64,
    y :: Float64,
    z :: Float64,
    size :: Float64,
    palette :: Int64
) -> Bool {
    return se_world_paint_data(world.data, x, y, z, size, palette);
}

# Validate and paint a cell in raw world storage, appending only when its center is absent.
#
# Parameters:
#     data (Array<Float64>): Numeric backing storage; offsets follow the containing resource layout.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#     size (Float64): Positive cube side length in world units, at most 1,000,000.
#     palette (Int64): Zero-based world color slot, from 0 through 7.
#
# Returns:
#     True when a cell was added or its size/palette changed; false for unchanged paint.
#
# Errors:
#     World map capacity reached (4096 cells).
fn se_world_paint_data(
    data :: Array<Float64>,
    x :: Float64,
    y :: Float64,
    z :: Float64,
    size :: Float64,
    palette :: Int64
) -> Bool {
    vmge_validate_cube(x, y, z, size, palette);
    count :: Int64 := int64(data[0]);
    scan :: Int64 := 0;
    i :: Int64 := -1;
    while scan < count {
        p :: Int64 := 16 + scan * 5;
        if data[p] == x && data[p + 1] == y && data[p + 2] == z {
            i = scan;
            break;
        }
        scan = scan + 1;
    }
    fresh :: Bool := i == -1;
    if fresh {
        if data[0] == 4096.0 {
            throw "World map capacity reached (4096 cells)";
        }
        i = int64(data[0]);
        data[0] = float64(i + 1);
    }
    p :: Int64 := 16 + i * 5;
    changed :: Bool := data[p + 3] != size || data[p + 4] != float64(palette) || fresh;
    data[p] = x;
    data[p + 1] = y;
    data[p + 2] = z;
    data[p + 3] = size;
    data[p + 4] = float64(palette);
    return changed;
}

# Remove an exact-center world cell; return false when no cell matches.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     True when a matching cell was removed; false when absent.
fn se_world_erase(world :: SEWorld, x :: Float64, y :: Float64, z :: Float64) -> Bool {
    return se_world_erase_data(world.data, x, y, z);
}

# Erase a cell in raw world storage by moving the final live record into its slot.
#
# Parameters:
#     data (Array<Float64>): Numeric backing storage; offsets follow the containing resource layout.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     True when a matching cell was removed; false when absent.
fn se_world_erase_data(data :: Array<Float64>, x :: Float64, y :: Float64, z :: Float64) -> Bool {
    count :: Int64 := int64(data[0]);
    scan :: Int64 := 0;
    i :: Int64 := -1;
    while scan < count {
        p :: Int64 := 16 + scan * 5;
        if data[p] == x && data[p + 1] == y && data[p + 2] == z {
            i = scan;
            break;
        }
        scan = scan + 1;
    }
    if i == -1 {
        return false;
    }
    j :: Int64 := 0;
    while j < 5 {
        data[16 + i * 5 + j] = data[16 + (count - 1) * 5 + j];
        j = j + 1;
    }
    data[0] = float64(count - 1);
    return true;
}

# Rectangle is preflighted in full before touching the retained store.
# axis: 0 XZ (Y elevation), 1 XY (Z elevation), 2 YZ (X elevation).

# Apply an axis-aligned construction-plane rectangle after validating the entire operation and
# required capacity.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     axis (Int64): Construction plane: 0 XZ (fixed Y), 1 XY (fixed Z), 2 YZ (fixed X).
#     u0 (Int64): First rectangle corner along the construction plane U grid axis.
#     v0 (Int64): First rectangle corner along the construction plane V grid axis.
#     u1 (Int64): Opposite rectangle corner along the construction plane U grid axis.
#     v1 (Int64): Opposite rectangle corner along the construction plane V grid axis.
#     grid (Float64): Construction-grid spacing in world units, from 0.125 through 1,024.
#     elevation (Float64): World-space coordinate held fixed by the construction plane.
#     palette (Int64): Zero-based world color slot, from 0 through 7.
#     erase (Bool): Whether to remove addressed cells rather than paint them.
#
# Returns:
#     True when at least one addressed cell changed; false for a no-op rectangle.
fn se_world_rectangle(
    world :: SEWorld,
    axis :: Int64,
    u0 :: Int64,
    v0 :: Int64,
    u1 :: Int64,
    v1 :: Int64,
    grid :: Float64,
    elevation :: Float64,
    palette :: Int64,
    erase :: Bool
) -> Bool {
    return se_world_rectangle_data(world.data, axis, u0, v0, u1, v1, grid, elevation, palette, erase);
}

# Preflight then paint/erase a construction-plane rectangle in raw world storage, preserving the
# payload on validation failure.
#
# Parameters:
#     data (Array): Numeric backing storage; offsets follow the containing resource layout.
#     axis (Int64): Construction plane: 0 XZ (fixed Y), 1 XY (fixed Z), 2 YZ (fixed X).
#     u0 (Int64): First rectangle corner along the construction plane U grid axis.
#     v0 (Int64): First rectangle corner along the construction plane V grid axis.
#     u1 (Int64): Opposite rectangle corner along the construction plane U grid axis.
#     v1 (Int64): Opposite rectangle corner along the construction plane V grid axis.
#     grid (Float64): Construction-grid spacing in world units, from 0.125 through 1,024.
#     elevation (Float64): World-space coordinate held fixed by the construction plane.
#     palette (Int64): Zero-based world color slot, from 0 through 7.
#     erase (Bool): Whether to remove addressed cells rather than paint them.
#
# Returns:
#     True when at least one addressed cell changed; false for a no-op rectangle.
#
# Errors:
#     Invalid construction grid/plane.
#     Rectangle exceeds 128 cells per axis.
#     Rectangle exceeds world map capacity.
fn se_world_rectangle_data(
    data :: Array,
    axis :: Int64,
    u0 :: Int64,
    v0 :: Int64,
    u1 :: Int64,
    v1 :: Int64,
    grid :: Float64,
    elevation :: Float64,
    palette :: Int64,
    erase :: Bool
) -> Bool {
    owner :: Array<Float64> := data;
    if axis < 0 || axis > 2 || !(grid >= 0.125 && grid <= 1024.0) {
        throw "Invalid construction grid/plane";
    }
    low_u :: Int64 := int64(vmath.min(float64(u0), float64(u1)));
    high_u :: Int64 := int64(vmath.max(float64(u0), float64(u1)));
    low_v :: Int64 := int64(vmath.min(float64(v0), float64(v1)));
    high_v :: Int64 := int64(vmath.max(float64(v0), float64(v1)));
    if high_u - low_u > 127 || high_v - low_v > 127 {
        throw "Rectangle exceeds 128 cells per axis";
    }
    pass :: Int64 := 0;
    needed :: Int64 := 0;
    changed :: Bool := false;
    while pass < 2 {
        u :: Int64 := low_u;
        while u <= high_u {
            v :: Int64 := low_v;
            while v <= high_v {
                x :: Float64 := float64(u) * grid;
                y :: Float64 := elevation;
                z :: Float64 := float64(v) * grid;
                if axis == 1 {
                    y = float64(v) * grid;
                    z = elevation;
                }
                if axis == 2 {
                    x = elevation;
                    y = float64(u) * grid;
                }
                if pass == 0 {
                    vmge_validate_cube(x, y, z, grid, palette);
                    if se_world_cell_data(owner, x, y, z) == -1 {
                        needed = needed + 1;
                    }
                } else {
                    if erase {
                        if se_world_erase_data(owner, x, y, z) {
                            changed = true;
                        }
                    } else {
                        if se_world_paint_data(owner, x, y, z, grid, palette) {
                            changed = true;
                        }
                    }
                }
                v = v + 1;
            }
            u = u + 1;
        }
        if pass == 0 && !erase && int64(data[0]) + needed > 4096 {
            throw "Rectangle exceeds world map capacity";
        }
        pass = pass + 1;
    }
    return changed;
}

# Validate and copy a legacy map into the resource's cell section, preserving terrain and scene nodes.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     imported (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Map exceeds world capacity (4096 cells).
fn se_world_import(world :: SEWorld, imported :: VMGEMap) -> Bool {
    if imported.count() > 4096 {
        throw "Map exceeds world capacity (4096 cells)";
    }
    i :: Int64 := 0;
    while i < imported.count() {
        vmge_validate_cube(
            imported.value(i, 0),
            imported.value(i, 1),
            imported.value(i, 2),
            imported.value(i, 3),
            int64(imported.value(i, 4))
        );
        i = i + 1;
    }
    i = 0;
    while i < imported.count() {
        j :: Int64 := 0;
        while j < 5 {
            world.data[16 + i * 5 + j] = imported.value(i, j);
            j = j + 1;
        }
        i = i + 1;
    }
    world.data[0] = float64(imported.count());
    return true;
}
