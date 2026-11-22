use "document.vy";

# Bounded include-expansion scratch with canonical dependency paths, content hashes, recursion
# tracking, and source-ID bookkeeping.
#
# Fields:
#     paths: Preallocated retained resource-path buffers.
#     hashes: Content hashes parallel to canonical include paths.
#     stack: Active include-source IDs used for recursion/cycle checks.
#     state: Mutable numeric state/counters used by this operation.
interface SSPreprocessor {
    paths :: Array,
    hashes :: Array<Int64>,
    stack :: Array<Int64>,
    state :: Array<Int64>
}

# Create include-preprocessing buffers and counters for a bounded compilation operation.
#
# Returns:
#     SSPreprocessor containing the resulting retained state or payload.
fn ss_preprocessor() -> SSPreprocessor {
    paths :: Array := [];
    i :: Int64 := 0;
    while i < 32 {
        paths.push(ss_text(2048));
        i = i + 1;
    }
    return SSPreprocessor(paths, ss_ints(32), ss_ints(16), ss_ints(3));
}

# Normalize path separators and relative path components for canonical include/resource identity.
#
# Parameters:
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_normal_path(path :: String) -> String {
    normalized :: Int64 := vcore.sb_create();
    i :: Int64 := 0;
    while i < path.size() {
        if path[i] == "\\" {
            vcore.sb_append(normalized, "/");
        } else {
            vcore.sb_append(normalized, path[i]);
        }
        i = i + 1;
    }
    raw :: String := vcore.sb_build(normalized);
    parts :: Array := ss_split(raw, "/");
    kept :: Array<Int64> := [];
    kept.reserve(parts.size());
    i = 0;
    while i < parts.size() {
        part :: String := parts[i];
        if part == ".." {
            if kept.size() == 0 || parts[kept[kept.size() - 1]] == ".." {
                kept.push(i);
            } else {
                kept.pop();
            }
        } else {
            if part != "." && part != "" {
                kept.push(i);
            }
        }
        i = i + 1;
    }
    b :: Int64 := vcore.sb_create();
    if raw.size() > 0 && raw[0] == "/" {
        vcore.sb_append(b, "/");
    }
    i = 0;
    while i < kept.size() {
        if i > 0 {
            vcore.sb_append(b, "/");
        }
        vcore.sb_append(b, parts[kept[i]]);
        i = i + 1;
    }
    return vcore.sb_build(b);
}

# Resolve or register an include path and hash, returning the numeric source ID used by GLSL line
# directives.
#
# Parameters:
#     p (SSPreprocessor): Bounded include-expansion scratch containing canonical paths, hashes, and
#         recursion/source-ID state.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     The Int64 value selected by the conditions described above.
#
# Errors:
#     Include changed during build.
#     Include dependency capacity (32).
fn ss_include_id(p :: SSPreprocessor, path :: String, text :: String) -> Int64 {
    tmp :: Array<Int64> := ss_text(65536);
    se_text_write(tmp, text);
    h :: Int64 := ss_hash(tmp);
    i :: Int64 := 0;
    while i < p.state[0] {
        if se_text_read(p.paths[i]) == path {
            if p.hashes[i] != h {
                throw "Include changed during build";
            }
            return i + 1;
        }
        i = i + 1;
    }
    if i >= 32 {
        throw "Include dependency capacity (32)";
    }
    se_text_write(p.paths[i], path);
    p.hashes[i] = h;
    p.state[0] = i + 1;
    return i + 1;
}

# Expand quoted relative GLSL includes with dependency/depth/source budgets and numeric line
# directives, keeping the root version first.
#
# Parameters:
#     p (SSPreprocessor): Bounded include-expansion scratch containing canonical paths, hashes, and
#         recursion/source-ID state.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     source_id (Int64): Numeric GLSL source ID used by line directives; zero identifies root source.
#     depth (Int64): Current dependency/parser recursion depth, checked against the operation limit.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Include depth/source budget exceeded.
#     Include cycle (with contextual detail).
#     Include syntax requires #include \"relative-path\".
#     Include path must be quoted.
#     Include path must be relative.
#     Missing include (with contextual detail).
#     Expanded shader budget (256 KiB).
#     #version must be the first root line and cannot appear in includes.
fn ss_expand(
    p :: SSPreprocessor,
    text :: String,
    path :: String,
    source_id :: Int64,
    depth :: Int64
) -> String {
    if depth >= 16 || text.size() > 65536 {
        throw "Include depth/source budget exceeded";
    }
    i :: Int64 := 0;
    while i < depth {
        if source_id != 0 && p.stack[i] == source_id {
            throw "Include cycle: " + path;
        }
        i = i + 1;
    }
    p.stack[depth] = source_id;
    lines :: Array := ss_split(text, "\n");
    b :: Int64 := vcore.sb_create();
    row :: Int64 := 0;
    while row < lines.size() {
        line :: String := lines[row];
        trimmed :: String := ss_trim(line);
        fields :: Array := ss_split(trimmed, " ");
        if trimmed.size() >= 8 && fields[0] == "#include" {
            if fields.size() != 2 {
                throw "Include syntax requires #include \"relative-path\"";
            }
            quoted :: String := fields[1];
            if quoted.size() < 3 || quoted[0] != "\"" || quoted[quoted.size() - 1] != "\"" {
                throw "Include path must be quoted";
            }
            name_builder :: Int64 := vcore.sb_create();
            name_end :: Int64 := quoted.size() - 1;
            i = 1;
            while i < name_end {
                vcore.sb_append(name_builder, quoted[i]);
                i = i + 1;
            }
            name :: String := vcore.sb_build(name_builder);
            if name[0] == "/" || name[0] == "\\" {
                throw "Include path must be relative";
            }
            resolved :: String := ss_normal_path(ss_parent(path) + name);
            raw := vfs.read(resolved);
            if raw == null {
                throw "Missing include: " + resolved + " (requested " + name + ")";
            }
            id :: Int64 := ss_include_id(p, resolved, raw);
            prefix :: String := "#line 1 " + string(id) + "\n";
            suffix :: String := "#line " + string(row + 2) + " " + string(source_id) + "\n";
            p.state[1] = p.state[1] + prefix.size() + suffix.size();
            if p.state[1] > 262144 {
                throw "Expanded shader budget (256 KiB)";
            }
            vcore.sb_append(b, prefix);
            vcore.sb_append(b, ss_expand(p, raw, resolved, id, depth + 1));
            vcore.sb_append(b, suffix);
        } else {
            if fields[0] == "#version" && (depth != 0 || row != 0) {
                throw "#version must be the first root line and cannot appear in includes";
            }
            p.state[1] = p.state[1] + line.size() + 1;
            if p.state[1] > 262144 {
                throw "Expanded shader budget (256 KiB)";
            }
            vcore.sb_append(b, line + "\n");
        }
        row = row + 1;
    }
    return vcore.sb_build(b);
}

# Poll the successful build's include hashes and advance only its build revision when dependency
# contents change.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The Boolean result of changed.
fn ss_document_dependencies_changed(d :: SSDocument) -> Bool {
    changed :: Bool := false;
    @bounded<16777216> region shader_dependencies {
        i :: Int64 := 0;
        tmp :: Array<Int64> := ss_text(65536);
        while i < d.state[10] {
            raw := vfs.read(se_text_read(d.dependencies[i]));
            h :: Int64 := -1;
            if raw != null {
                se_text_write(tmp, raw);
                h = ss_hash(tmp);
            }
            if h != d.dependency_hashes[i] {
                d.dependency_hashes[i] = h;
                changed = true;
            }
            i = i + 1;
        }
    };
    if changed {
        d.state[11] = d.state[11] + 1;
    }
    return changed;
}
