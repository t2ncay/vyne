use "scene.vy";
use "../engine/world_numbers.vy";

# Parse and validate an integer-valued persisted scene field.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     The computed Int64 value: int64(value).
#
# Errors:
#     Scene field requires an integer.
fn se_integer(text :: String) -> Int64 {
    value :: Float64 := vmge_number(text);
    if value != vmath.floor(value) {
        throw "Scene field requires an integer";
    }
    return int64(value);
}

# Parse and validate scene text into a staging owner, checking nodes, hierarchy, world, materials, and
# extensions by format version.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Scene text exceeds 16 MiB.
#     Scene record exceeds 2048 characters.
#     Unsupported scene header/version.
#     Expected 21 scene fields.
#     Duplicate or invalid node slot.
#     Invalid scene metadata.
#     Environment nodes require scene v4.
#     Scene line.
#
# Lifecycle:
#     Mutates the supplied staging owner. Use se_import_text or se_load with distinct
#     destination/staging owners for failure-isolated publication.
fn se_parse(scene :: SEScene, text :: String) -> Bool {
    if text.size() > 16777216 {
        throw "Scene text exceeds 16 MiB";
    }
    column :: Int64 := 0;
    character :: Int64 := 0;
    while character < text.size() {
        if text[character] == "\n" {
            column = 0;
        } else {
            column = column + 1;
            if column > 2048 {
                throw "Scene record exceeds 2048 characters";
            }
        }
        character = character + 1;
    }
    lines :: Array := vmge_split(text, "\n");
    if lines.size() < 1
        || (vmge_trim(lines[0]) != "VMGE_SCENE|1" && vmge_trim(lines[0]) != "VMGE_SCENE|2" && vmge_trim(lines[0]) != "VMGE_SCENE|3" && vmge_trim(lines[0]) != "VMGE_SCENE|4") {
        throw "Unsupported scene header/version";
    }
    se_clear(scene);
    row :: Int64 := 1;
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != ""
            && line[0] != "W"
            && line[0] != "C"
            && line[0] != "H"
            && line[0] != "M"
            && line[0] != "P"
            && line[0] != "R"
            && line[0] != "E"
            && line[0] != "A" {
            try {
                fields :: Array := vmge_split(line, "|");
                if fields.size() != 21 {
                    throw "Expected 21 scene fields";
                }
                slot :: Int64 := se_integer(fields[0]);
                parent :: Int64 := se_integer(fields[1]);
                kind :: Int64 := se_integer(fields[2]);
                visible :: Int64 := se_integer(fields[3]);
                texture :: Int64 := se_integer(fields[4]);
                if slot < 0 || slot >= 128 || scene.meta[slot * 8] != 0 {
                    throw "Duplicate or invalid node slot";
                }
                if parent < -1
                    || parent >= 128
                    || !se_node_kind_valid(kind)
                    || visible < 0
                    || visible > 1
                    || texture < 0
                    || texture > 7 {
                    throw "Invalid scene metadata";
                }
                if kind >= 5 && vmge_trim(lines[0]) != "VMGE_SCENE|4" {
                    throw "Environment nodes require scene v4";
                }
                scene.meta[slot * 8] = 1;
                scene.meta[slot * 8 + 1] = 1;
                scene.meta[slot * 8 + 2] = -1;
                if parent != -1 {
                    scene.meta[slot * 8 + 2] = 128 + parent;
                }
                scene.meta[slot * 8 + 3] = kind;
                scene.meta[slot * 8 + 4] = visible;
                scene.meta[slot * 8 + 5] = texture;
                scene.meta[slot * 8 + 6] = -1;
                scene.meta[slot * 8 + 7] = 0;
                if scene.clocks[slot] < 1 {
                    scene.clocks[slot] = 1;
                }
                scene.state[0] = scene.state[0] + 1;
                se_node_extra_reset(scene, slot);
                field :: Int64 := 0;
                while field < 14 {
                    scene.property(scene.handle(slot), field, vmge_number(fields[field + 5]));
                    field = field + 1;
                }
                se_text_unhex(scene.names[scene.offset + slot], fields[19]);
                se_text_unhex(scene.paths[scene.offset + slot], fields[20]);
            } catch (error) {
                throw "Scene line " + string(row + 1) + ": " + string(error);
            }
        }
        row = row + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            scene.reparent(scene.handle(slot), scene.meta[slot * 8 + 2]);
        }
        slot = slot + 1;
    }
    se_parse_materials(scene, lines);
    se_parse_extensions(scene, lines);
    se_parse_world(scene.world, lines, vmge_trim(lines[0]) != "VMGE_SCENE|1");
    return true;
}

# Validate and load W/C/H world records into staging, including complete and unique terrain samples.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     lines (Array): Parsed scene text lines to validate for the selected record section.
#     version2 (Bool): Whether extended world records are permitted by the scene header.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     World records require scene version 2.
#     Invalid or duplicate world header.
#     Invalid terrain size.
#     World record before header.
#     Invalid world cell/capacity.
#     Invalid terrain vertex.
#     Terrain vertex out of bounds.
#     Duplicate terrain vertex.
#     Missing world header.
#     Incomplete terrain surface (with contextual detail).
fn se_parse_world(world :: SEWorld, lines :: Array, version2 :: Bool) -> Bool {
    seen :: Bool := false;
    heights :: Int64 := 0;
    row :: Int64 := 1;
    i :: Int64 := 0;
    while i < 4225 {
        world.work[i] = 0.0;
        i = i + 1;
    }
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != "" && (line[0] == "W" || line[0] == "C" || line[0] == "H") {
            if !version2 {
                throw "World records require scene version 2";
            }
            fields :: Array := vmge_split(line, "|");
            if fields[0] == "W" {
                if seen || (fields.size() != 8 && fields.size() != 9) {
                    throw "Invalid or duplicate world header";
                }
                side :: Int64 := se_integer(fields[1]);
                spacing :: Float64 := vmge_number(fields[2]);
                seed :: Int64 := se_integer(fields[6]);
                preset :: Int64 := se_integer(fields[7]);
                if side < 0 {
                    throw "Invalid terrain size";
                }
                amplitude :: Float64 := 0.0;
                if fields.size() == 9 {
                    amplitude = vmge_number(fields[8]);
                }
                se_terrain_validate(int64(vmath.max(1.0, float64(side))), spacing, amplitude, seed, preset);
                world.data[1] = float64(side);
                world.data[2] = spacing;
                world.data[3] = vmge_number(fields[3]);
                world.data[4] = vmge_number(fields[4]);
                world.data[5] = vmge_number(fields[5]);
                world.data[6] = float64(seed);
                world.data[7] = float64(preset);
                world.data[8] = amplitude;
                seen = true;
            } else {
                if !seen {
                    throw "World record before header";
                }
                if fields[0] == "C" {
                    if fields.size() != 6 || world.count() >= 4096 {
                        throw "Invalid world cell/capacity";
                    }
                    x :: Float64 := vmge_number(fields[1]);
                    y :: Float64 := vmge_number(fields[2]);
                    z :: Float64 := vmge_number(fields[3]);
                    size :: Float64 := vmge_number(fields[4]);
                    palette :: Int64 := se_integer(fields[5]);
                    vmge_validate_cube(x, y, z, size, palette);
                    p :: Int64 := 16 + world.count() * 5;
                    world.data[p] = x;
                    world.data[p + 1] = y;
                    world.data[p + 2] = z;
                    world.data[p + 3] = size;
                    world.data[p + 4] = float64(palette);
                    world.data[0] = float64(world.count() + 1);
                } else {
                    if fields[0] != "H" || fields.size() != 4 || world.side() == 0 {
                        throw "Invalid terrain vertex";
                    }
                    x :: Int64 := se_integer(fields[1]);
                    z :: Int64 := se_integer(fields[2]);
                    h :: Float64 := vmge_number(fields[3]);
                    if x < 0 || x > world.side() || z < 0 || z > world.side() || !(vmath.abs(h) <= 1024.0) {
                        throw "Terrain vertex out of bounds";
                    }
                    p :: Int64 := z * 65 + x;
                    if world.work[p] != 0.0 {
                        throw "Duplicate terrain vertex";
                    }
                    world.work[p] = 1.0;
                    world.data[20496 + p] = h;
                    heights = heights + 1;
                }
            }
        }
        row = row + 1;
    }
    if version2 && !seen {
        throw "Missing world header";
    }
    final_side :: Int64 := world.side();
    if final_side > 0 && heights != (final_side + 1) * (final_side + 1) {
        throw "Incomplete terrain surface: " + string(heights) + " vertices for side " + string(final_side);
    }
    return true;
}

# Serialize scene nodes and optional material/environment records using compatible versions and
# roundtrip-safe numeric text.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_scene_text(scene :: SEScene) -> String {
    builder :: Int64 := vcore.sb_create();
    if se_scene_has_extensions(scene) {
        vcore.sb_append(builder, "VMGE_SCENE|4\n");
    } else {
        if se_scene_has_materials(scene) {
            vcore.sb_append(builder, "VMGE_SCENE|3\n");
        } else {
            if scene.world.count() == 0 && scene.world.side() == 0 {
                vcore.sb_append(builder, "VMGE_SCENE|1\n");
            } else {
                vcore.sb_append(builder, "VMGE_SCENE|2\n");
            }
        }
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            parent :: Int64 := scene.meta[slot * 8 + 2];
            if parent != -1 {
                parent = parent % 128;
            }
            vcore.sb_append(
                builder,
                string(slot)
                    +"|"
                    +string(parent)
                    +"|"
                    +string(scene.meta[slot * 8 + 3])
                    +"|"
                    +string(scene.meta[slot * 8 + 4])
                    +"|"
                    +string(scene.meta[slot * 8 + 5])
            );
            field :: Int64 := 0;
            while field < 14 {
                vcore.sb_append(
                    builder,
                    "|" + se_world_decimal(scene.world, scene.values[slot * 16 + field])
                );
                field = field + 1;
            }
            vcore.sb_append(
                builder,
                "|"
                    +se_text_hex(scene.names[scene.offset + slot])
                    +"|"
                    +se_text_hex(scene.paths[scene.offset + slot])
                    +"\n"
            );
        }
        slot = slot + 1;
    }
    vcore.sb_append(builder, se_scene_material_text(scene));
    vcore.sb_append(builder, se_scene_extension_text(scene));
    if scene.world.count() > 0
        || scene.world.side() > 0
        || se_scene_has_materials(scene)
        || se_scene_has_extensions(scene) {
        world :: SEWorld := scene.world;
        vcore.sb_append(builder, "W");
        field :: Int64 := 1;
        while field < 9 {
            vcore.sb_append(builder, "|" + string(world.data[field]));
            field = field + 1;
        }
        vcore.sb_append(builder, "\n");
        i :: Int64 := 0;
        while i < world.count() {
            vcore.sb_append(builder, "C");
            field = 0;
            while field < 5 {
                vcore.sb_append(builder, "|" + string(world.data[16 + i * 5 + field]));
                field = field + 1;
            }
            vcore.sb_append(builder, "\n");
            i = i + 1;
        }
        if world.side() > 0 {
            z :: Int64 := 0;
            while z <= world.side() {
                x :: Int64 := 0;
                while x <= world.side() {
                    vcore.sb_append(
                        builder,
                        "H|"
                            +string(x)
                            +"|"
                            +string(z)
                            +"|"
                            +string(world.data[20496 + z * 65 + x])
                            +"\n"
                    );
                    x = x + 1;
                }
                z = z + 1;
            }
        }
    }
    return vcore.sb_build(builder);
}

# Parse scene text in distinct staging storage, then replace the destination with fresh generation-
# safe handles.
#
# Parameters:
#     destination (SEScene): Existing destination owner to receive the copied/validated payload.
#     staging (SEScene): Separate validation/staging owner; publish its payload only after validation
#         succeeds.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
fn se_import_text(destination :: SEScene, staging :: SEScene, text :: String) -> Bool {
    se_parse(staging, text);
    se_replace(destination, staging);
    return true;
}

# Report whether scene material assignments require material/resource persistence records.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     True when scene material assignments require material/resource persistence records; false
#     otherwise.
fn se_scene_has_materials(scene :: SEScene) -> Bool {
    if scene.materials.resource_state[0] != 0 {
        return true;
    }
    i :: Int64 := 0;
    while i < 128 {
        p :: Array<Int64> := scene.materials.paths[i];
        if scene.meta[i * 8] != 0 && p[0] != 0 {
            return true;
        }
        i = i + 1;
    }
    return false;
}

# Serialize per-node shader references, numeric overrides, sampler bindings, and persistent texture
# resources.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Shader references require mesh nodes.
fn se_scene_material_text(scene :: SEScene) -> String {
    resources :: Int64 := 0;
    b :: Int64 := vcore.sb_create();
    slot :: Int64 := 0;
    while slot < 128 {
        path :: Array<Int64> := scene.materials.paths[slot];
        if scene.meta[slot * 8] != 0 && path[0] != 0 {
            if scene.meta[slot * 8 + 3] != 1 {
                throw "Shader references require mesh nodes";
            }
            m :: MaterialInstance := scene.materials.instances[slot];
            se_material_limit(m);
            vcore.sb_append(
                b,
                "M|"
                    +string(slot)
                    +"|"
                    +string(m.state[0])
                    +"|"
                    +string(m.state[1])
                    +"|"
                    +se_text_hex(path)
                    +"\n"
            );
            rows :: Array := ss_split(ss_material_text(m), "\n");
            i :: Int64 := 1;
            while i < rows.size() {
                if rows[i] != "" {
                    vcore.sb_append(b, "P|" + string(slot) + "|" + rows[i] + "\n");
                }
                i = i + 1;
            }
        }
        slot = slot + 1;
    }
    while resources < scene.materials.resource_state[0] {
        vcore.sb_append(
            b,
            "R|" + string(resources + 100) + "|" + se_text_hex(scene.materials.resources[resources]) + "\n"
        );
        resources = resources + 1;
    }
    return vcore.sb_build(b);
}

# Validate material/resource records against live mesh nodes and reconstruct retained per-instance
# values and references.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     lines (Array): Parsed scene text lines to validate for the selected record section.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Material records require scene v3/v4.
#     Invalid scene material record.
#     Invalid texture resource table.
#     Material requires a live mesh node.
#     Duplicate/invalid material reference.
#     Empty shader resource reference.
#     Material value before reference.
#     Material instance must match node slot.
#     Material references missing texture resource.
fn se_parse_materials(scene :: SEScene, lines :: Array) -> Bool {
    builders :: Array<Int64> := ss_ints(128);
    seen :: Array<Int64> := ss_ints(128);
    row :: Int64 := 1;
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != "" && (line[0] == "M" || line[0] == "P" || line[0] == "R") {
            if vmge_trim(lines[0]) != "VMGE_SCENE|3" && vmge_trim(lines[0]) != "VMGE_SCENE|4" {
                throw "Material records require scene v3/v4";
            }
            f :: Array := ss_split(line, "|");
            if f.size() < 3 {
                throw "Invalid scene material record";
            }
            if f[0] == "R" {
                if f.size() != 3
                    || ss_integer(f[1]) != scene.materials.resource_state[0]
                    +100
                    || scene.materials.resource_state[0] == 64 {
                    throw "Invalid texture resource table";
                }
                se_text_unhex(scene.materials.resources[scene.materials.resource_state[0]], f[2]);
                scene.materials.resource_state[0] = scene.materials.resource_state[0] + 1;
            } else {
                owner :: Int64 := ss_integer(f[1]);
                if owner < 0 || owner >= 128 || scene.meta[owner * 8] == 0 || scene.meta[owner * 8 + 3] != 1 {
                    throw "Material requires a live mesh node";
                }
                if f[0] == "M" {
                    if seen[owner] != 0 || f.size() != 5 {
                        throw "Duplicate/invalid material reference";
                    }
                    se_text_unhex(scene.materials.paths[owner], f[4]);
                    path :: Array<Int64> := scene.materials.paths[owner];
                    if path[0] == 0 {
                        throw "Empty shader resource reference";
                    }
                    builders[owner] = vcore.sb_create();
                    vcore.sb_append(builders[owner], "VMGE_MATERIAL|1|" + f[2] + "|" + f[3] + "\n");
                    seen[owner] = 1;
                } else {
                    if !seen[owner] {
                        throw "Material value before reference";
                    }
                    i :: Int64 := 2;
                    while i < f.size() {
                        if i != 2 {
                            vcore.sb_append(builders[owner], "|");
                        }
                        vcore.sb_append(builders[owner], f[i]);
                        i = i + 1;
                    }
                    vcore.sb_append(builders[owner], "\n");
                }
            }
        }
        row = row + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if seen[slot] != 0 {
            m :: MaterialInstance := scene.materials.instances[slot];
            ss_material_parse(m, vcore.sb_build(builders[slot]));
            se_material_limit(m);
            if m.state[1] != slot + 1 {
                throw "Material instance must match node slot";
            }
            i :: Int64 := 0;
            while i < m.binding_state[0] {
                id :: Int64 := m.bindings[i * 4 + 1] - 100;
                if id < 0 || id >= scene.materials.resource_state[0] {
                    throw "Material references missing texture resource";
                }
                i = i + 1;
            }
        }
        slot = slot + 1;
    }
    return true;
}

# Read scene text, validate it in a bounded staging region, then replace the destination owner.
#
# Parameters:
#     destination (SEScene): Existing destination owner to receive the copied/validated payload.
#     staging (SEScene): Separate validation/staging owner; publish its payload only after validation
#         succeeds.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Could not read scene (with contextual detail).
fn se_load(destination :: SEScene, staging :: SEScene, path :: String) -> Bool {
    @bounded<67108864> region import_scene {
        text := vfs.read(path);
        if text == null {
            throw "Could not read scene: " + path;
        }
        se_parse(staging, text);
    };
    se_replace(destination, staging);
    return true;
}

# Report whether environment nodes or nondefault active-camera settings require scene version four.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     True when environment nodes or nondefault active-camera settings require scene version four;
#     false otherwise.
fn se_scene_has_extensions(scene :: SEScene) -> Bool {
    if scene.settings[0] >= 0 || scene.settings[1] >= 0 {
        return true;
    }
    i :: Int64 := 0;
    while i < 128 {
        if scene.meta[i * 8] != 0 {
            kind :: Int64 := scene.meta[i * 8 + 3];
            path :: Array<Int64> := scene.paths[scene.offset + i];
            if kind >= 5 || (kind == 2 && (path[0] != 0 || scene.extra[i * 16] != 1.0)) {
                return true;
            }
        }
        i = i + 1;
    }
    return false;
}

# Serialize active camera/sky selections and typed environment sidecars for version-four scenes.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_scene_extension_text(scene :: SEScene) -> String {
    if !se_scene_has_extensions(scene) {
        return "";
    }
    b :: Int64 := vcore.sb_create();
    vcore.sb_append(b, "A|" + string(scene.settings[0]) + "|" + string(scene.settings[1]) + "\n");
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 && (scene.meta[slot * 8 + 3] == 2 || scene.meta[slot * 8 + 3] >= 5) {
            vcore.sb_append(b, "E|" + string(slot));
            f :: Int64 := 0;
            while f < 16 {
                vcore.sb_append(b, "|" + se_world_decimal(scene.world, scene.extra[slot * 16 + f]));
                f = f + 1;
            }
            vcore.sb_append(b, "\n");
        }
        slot = slot + 1;
    }
    return vcore.sb_build(b);
}

# Validate and restore active selections and typed environment records into scene staging.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     lines (Array): Parsed scene text lines to validate for the selected record section.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Scene extensions require v4.
#     Duplicate/invalid active node record.
#     Invalid active node slot.
#     Active node type mismatch.
#     Invalid node extension record.
#     Invalid extension owner.
#     Duplicate/invalid extension owner.
#     Nonzero reserved extension field.
#     Missing node extension record.
#     Sky sun must reference a directional light.
#     Missing active node record.
fn se_parse_extensions(scene :: SEScene, lines :: Array) -> Bool {
    seen :: Array<Int64> := ss_ints(128);
    active_seen :: Bool := false;
    row :: Int64 := 1;
    v4 :: Bool := vmge_trim(lines[0]) == "VMGE_SCENE|4";
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != "" && (line[0] == "E" || line[0] == "A") {
            if !v4 {
                throw "Scene extensions require v4";
            }
            f :: Array := vmge_split(line, "|");
            if f[0] == "A" {
                if active_seen || f.size() != 3 {
                    throw "Duplicate/invalid active node record";
                }
                i :: Int64 := 0;
                while i < 2 {
                    slot :: Int64 := se_integer(f[i + 1]);
                    kind :: Int64 := 2;
                    if i == 1 {
                        kind = 5;
                    }
                    if slot < -1 || slot >= 128 {
                        throw "Invalid active node slot";
                    }
                    if slot >= 0 {
                        if scene.meta[slot * 8] == 0 || scene.meta[slot * 8 + 3] != kind {
                            throw "Active node type mismatch";
                        }
                    }
                    scene.settings[i] = slot;
                    i = i + 1;
                }
                active_seen = true;
            } else {
                if f[0] != "E" || f.size() != 18 {
                    throw "Invalid node extension record";
                }
                slot :: Int64 := se_integer(f[1]);
                if slot < 0 || slot >= 128 {
                    throw "Invalid extension owner";
                }
                kind :: Int64 := scene.meta[slot * 8 + 3];
                if scene.meta[slot * 8] == 0 || seen[slot] != 0 || (kind != 2 && kind < 5) {
                    throw "Duplicate/invalid extension owner";
                }
                count :: Int64 := 3;
                if kind == 2 {
                    count = 1;
                }
                if kind == 5 {
                    count = 11;
                }
                i :: Int64 := 0;
                while i < 16 {
                    value :: Float64 := vmge_number(f[i + 2]);
                    if i < count {
                        se_extra_set(scene, scene.handle(slot), i, value);
                    } else {
                        if value != 0.0 {
                            throw "Nonzero reserved extension field";
                        }
                    }
                    i = i + 1;
                }
                seen[slot] = 1;
            }
        }
        row = row + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            kind :: Int64 := scene.meta[slot * 8 + 3];
            if v4 && (kind == 2 || kind >= 5) && seen[slot] == 0 {
                throw "Missing node extension record";
            }
            if kind == 5 {
                linked :: Int64 := int64(scene.extra[slot * 16 + 9]);
                if linked >= 0 {
                    if scene.meta[linked * 8] == 0 || scene.meta[linked * 8 + 3] != 6 {
                        throw "Sky sun must reference a directional light";
                    }
                }
            }
        }
        slot = slot + 1;
    }
    if v4 && !active_seen {
        throw "Missing active node record";
    }
    return true;
}

# Atomically write compatible scene text, preserving an existing file if serialization or replacement
# fails.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Could not save scene (with contextual detail).
fn se_save(scene :: SEScene, path :: String) -> Bool {
    @bounded<67108864> region save_scene {
        if !vfs.write_atomic(path, se_scene_text(scene)) {
            throw "Could not save scene: " + path;
        }
    };
    return true;
}

# Populate the authored five-node starter scene with ground, crate, character, and camera hierarchy.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     True when the operation completes without throwing.
fn se_demo(scene :: SEScene) -> Bool {
    se_clear(scene);
    root :: Int64 := scene.spawn(0, "Demo", -1);
    floor :: Int64 := scene.spawn(1, "Ground", root);
    scene.property(floor, 1, -0.25);
    scene.property(floor, 6, 24.0);
    scene.property(floor, 7, 0.5);
    scene.property(floor, 8, 24.0);
    scene.meta[(floor % 128) * 8 + 5] = 4;
    cube :: Int64 := scene.spawn(1, "Crate", root);
    scene.property(cube, 0, 2.0);
    scene.property(cube, 1, 1.0);
    scene.property(cube, 2, 4.0);
    scene.property(cube, 6, 2.0);
    scene.property(cube, 7, 2.0);
    scene.property(cube, 8, 2.0);
    scene.meta[(cube % 128) * 8 + 5] = 0;
    actor :: Int64 := scene.spawn(4, "Player", root);
    camera :: Int64 := scene.spawn(2, "Camera", actor);
    scene.property(camera, 1, 1.8);
    return true;
}

# Populate the hybrid-world island demo with deterministic terrain and 1,024 map-resource cells.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#
# Returns:
#     True when the operation completes without throwing.
fn se_resource_demo(scene :: SEScene) -> Bool {
    se_demo(scene);
    scene.meta[12] = 0;
    scene.world.data[3] = -16.0;
    scene.world.data[5] = -16.0;
    se_terrain_generate(scene.world, 32, 1.0, 6.0, 42, 1);
    se_world_rectangle(scene.world, 0, -16, -16, 15, 15, 1.0, -0.5, 6, false);
    scene.property(scene.handle(3), 0, -10.0);
    scene.property(scene.handle(3), 1, 2.0);
    scene.property(scene.handle(3), 2, -10.0);
    return true;
}
