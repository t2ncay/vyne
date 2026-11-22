use "scene.vy";

# Preallocated per-slot world matrices, traversal marks, and six-number axis-aligned bounds cache for
# 128 scene nodes.
#
# Fields:
#     matrices: Sixteen cached world-matrix numbers per scene slot.
#     marks: Traversal state: 0 unseen, 1 visiting, 2 matrix computed.
#     boxes: Six world-space bounds numbers per slot: minimum XYZ and maximum XYZ.
interface SETransforms {
    matrices :: Array<Float64>,
    marks :: Array<Int64>,
    boxes :: Array<Float64>,
}

# Create transform traversal and bounds storage around a caller-provided matrix buffer.
#
# Parameters:
#     buffer (Array): Caller-provided fixed numeric buffer for matrix or serialization scratch.
#
# Returns:
#     SETransforms containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_transforms(buffer :: Array) -> SETransforms {
    marks :: Array<Int64> := [];
    marks.reserve(128);
    marks.resize(128, 0);
    boxes :: Array<Float64> := [];
    boxes.reserve(768);
    boxes.resize(768, 0.0);
    return SETransforms(buffer, marks, boxes);
}

# Copy a raylib Matrix into sixteen numeric elements using the cache's explicit matrix layout.
#
# Parameters:
#     data (Array<Float64>): Numeric backing storage; offsets follow the containing resource layout.
#     offset (Int64): Zero-based offset into the supplied backing storage.
#     value (vraylib.Matrix): Matrix whose sixteen values are copied into numeric storage.
#
# Returns:
#     True when the operation completes without throwing.
fn se_matrix_write(data :: Array<Float64>, offset :: Int64, value :: vraylib.Matrix) -> Bool {
    data[offset] = value.m0;
    data[offset + 1] = value.m4;
    data[offset + 2] = value.m8;
    data[offset + 3] = value.m12;
    data[offset + 4] = value.m1;
    data[offset + 5] = value.m5;
    data[offset + 6] = value.m9;
    data[offset + 7] = value.m13;
    data[offset + 8] = value.m2;
    data[offset + 9] = value.m6;
    data[offset + 10] = value.m10;
    data[offset + 11] = value.m14;
    data[offset + 12] = value.m3;
    data[offset + 13] = value.m7;
    data[offset + 14] = value.m11;
    data[offset + 15] = value.m15;
    return true;
}

# Reconstruct a temporary raylib Matrix from sixteen cached numeric elements.
#
# Parameters:
#     data (Array<Float64>): Numeric backing storage; offsets follow the containing resource layout.
#     offset (Int64): Zero-based offset into the supplied backing storage.
#
# Returns:
#     A temporary matrix containing the computed or reconstructed transform.
fn se_matrix_read(data :: Array<Float64>, offset :: Int64) -> vraylib.Matrix {
    return vraylib.Matrix(
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
        data[offset + 4],
        data[offset + 5],
        data[offset + 6],
        data[offset + 7],
        data[offset + 8],
        data[offset + 9],
        data[offset + 10],
        data[offset + 11],
        data[offset + 12],
        data[offset + 13],
        data[offset + 14],
        data[offset + 15]
    );
}

# Compute and cache a node's parent-composed scale/rotation/translation matrix, rejecting hierarchy
# cycles.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     id (Int64): Generation-checked scene handle; stale handles are rejected by scene.require.
#
# Returns:
#     A temporary matrix containing the computed or reconstructed transform.
#
# Errors:
#     Scene hierarchy cycle.
fn se_world(scene :: SEScene, cache :: SETransforms, id :: Int64) -> vraylib.Matrix {
    slot :: Int64 := scene.require(id);
    if cache.marks[slot] == 2 {
        return se_matrix_read(cache.matrices, slot * 16);
    }
    if cache.marks[slot] == 1 {
        throw "Scene hierarchy cycle";
    }
    cache.marks[slot] = 1;
    data :: Array<Float64> := scene.values;
    p :: Int64 := slot * 16;
    scale :: vraylib.Matrix := vraylib.math_matrix_scale(data[p + 6], data[p + 7], data[p + 8]);
    rotation :: vraylib.Matrix := vraylib.math_matrix_rotate_xyz(vraylib.vec3(
        data[p + 3] * 0.01745329252,
        data[p + 4] * 0.01745329252,
        data[p + 5] * 0.01745329252
    ));
    translation :: vraylib.Matrix := vraylib.math_matrix_translate(data[p], data[p + 1], data[p + 2]);
    transform :: vraylib.Matrix := vraylib.math_matrix_multiply(
        vraylib.math_matrix_multiply(scale, rotation),
        translation
    );
    parent :: Int64 := scene.meta[slot * 8 + 2];
    if parent != -1 {
        transform = vraylib.math_matrix_multiply(transform, se_world(scene, cache, parent));
    }
    se_matrix_write(cache.matrices, p, transform);
    cache.marks[slot] = 2;
    return transform;
}

# Recompute all live world transforms and conservative axis-aligned bounds into existing caches.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#
# Returns:
#     True when the operation completes without throwing.
fn se_update_transforms(scene :: SEScene, cache :: SETransforms) -> Bool {
    i :: Int64 := 0;
    while i < 128 {
        cache.marks[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 128 {
        if scene.meta[i * 8] != 0 {
            se_world(scene, cache, scene.handle(i));
        }
        i = i + 1;
    }
    i = 0;
    while i < 128 {
        if scene.meta[i * 8] != 0 {
            box :: vraylib.BoundingBox := se_compute_box(cache, i);
            cache.boxes[i * 6] = box.min.x;
            cache.boxes[i * 6 + 1] = box.min.y;
            cache.boxes[i * 6 + 2] = box.min.z;
            cache.boxes[i * 6 + 3] = box.max.x;
            cache.boxes[i * 6 + 4] = box.max.y;
            cache.boxes[i * 6 + 5] = box.max.z;
        }
        i = i + 1;
    }
    return true;
}

# Read a node slot's world-space translation from its cached matrix.
#
# Parameters:
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#
# Returns:
#     A temporary vector containing the computed coordinates described above.
fn se_position(cache :: SETransforms, slot :: Int64) -> vraylib.Vector3 {
    return vraylib.vec3(
        cache.matrices[slot * 16 + 3],
        cache.matrices[slot * 16 + 7],
        cache.matrices[slot * 16 + 11]
    );
}

# Report whether a node and every ancestor are visible.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     id (Int64): Generation-checked scene handle; stale handles are rejected by scene.require.
#
# Returns:
#     True when the node and all ancestors are visible; false if any visibility flag is clear.
fn se_visible(scene :: SEScene, id :: Int64) -> Bool {
    current :: Int64 := id;
    while current != -1 {
        slot :: Int64 := scene.require(current);
        if scene.meta[slot * 8 + 4] == 0 {
            return false;
        }
        current = scene.meta[slot * 8 + 2];
    }
    return true;
}

# Transform the eight unit-cube corners and compute conservative world-space axis-aligned bounds.
#
# Parameters:
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#
# Returns:
#     A temporary bounding box containing world-space minimum and maximum coordinates.
fn se_compute_box(cache :: SETransforms, slot :: Int64) -> vraylib.BoundingBox {
    matrix :: vraylib.Matrix := se_matrix_read(cache.matrices, slot * 16);
    minimum :: vraylib.Vector3 := vraylib.vec3(1000000000.0, 1000000000.0, 1000000000.0);
    maximum :: vraylib.Vector3 := vraylib.vec3(-1000000000.0, -1000000000.0, -1000000000.0);
    i :: Int64 := 0;
    while i < 8 {
        x :: Float64 := -0.5;
        y :: Float64 := -0.5;
        z :: Float64 := -0.5;
        if i % 2 == 1 {
            x = 0.5;
        }
        if (i / 2) % 2 == 1 {
            y = 0.5;
        }
        if i >= 4 {
            z = 0.5;
        }
        point :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(x, y, z), matrix);
        minimum.x = vmath.min(minimum.x, point.x);
        minimum.y = vmath.min(minimum.y, point.y);
        minimum.z = vmath.min(minimum.z, point.z);
        maximum.x = vmath.max(maximum.x, point.x);
        maximum.y = vmath.max(maximum.y, point.y);
        maximum.z = vmath.max(maximum.z, point.z);
        i = i + 1;
    }
    return vraylib.BoundingBox(minimum, maximum);
}

# Reconstruct temporary bounding-box records from a slot's six cached bounds values.
#
# Parameters:
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#
# Returns:
#     A temporary bounding box containing world-space minimum and maximum coordinates.
fn se_box(cache :: SETransforms, slot :: Int64) -> vraylib.BoundingBox {
    return vraylib.BoundingBox(
        vraylib.vec3(cache.boxes[slot * 6], cache.boxes[slot * 6 + 1], cache.boxes[slot * 6 + 2]),
        vraylib.vec3(cache.boxes[slot * 6 + 3], cache.boxes[slot * 6 + 4], cache.boxes[slot * 6 + 5])
    );
}
