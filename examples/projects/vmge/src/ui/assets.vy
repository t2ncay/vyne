use "widgets.vy";

# Recognize supported image file extensions for imports and thumbnail previews.
#
# Parameters:
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The Boolean result of ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp".
fn se_image_asset(path :: String) -> Bool {
    ext :: String := vraylib.text_to_lower(vfs.extension(path));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
}

# Read an asset as a checked binary byte payload, enforcing the expected size and read completeness.
#
# Parameters:
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     expected (Int64): Checked asset file size in bytes.
#
# Returns:
#     Array containing the resulting retained state or payload.
#
# Errors:
#     Binary import read is limited to 8 MiB.
#     Could not read the complete binary asset.
fn se_import_read_bytes(path :: String, expected :: Int64) -> Array {
    if expected <= 0 || expected > 8388608 {
        throw "Binary import read is limited to 8 MiB";
    }
    bytes :: Array := [];
    i :: Int64 := 0;
    # Boxed reserve is not emitted by the compiler. Finish all growth with zero
    # placeholders before filling real bytes; no later resize/reallocation occurs.
    while i < expected {
        bytes.push(0);
        i = i + 1;
    }
    count :: Int64 := vraylib.buffer("int", 1);
    data :: Int64 := 0;
    try {
        # vfs.read_bytes grows boxed storage internally. Fill fixed-size storage
        # from a native binary buffer so arena reuse cannot discard earlier bytes.
        data = vraylib.load_file_data(path, count);
        if data == 0 || int64(vraylib.buffer_read(count, 0)) != expected {
            throw "Could not read the complete binary asset";
        }
        i = 0;
        while i < expected {
            bytes[i] = int64(vraylib.buffer_read(data, i));
            i = i + 1;
        }
    } finally {
        if data != 0 {
            vraylib.unload_file_data(data);
        }
        vraylib.release(count);
    }
    return bytes;
}

# Read a little-endian unsigned 32-bit field from checked binary asset bytes.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     offset (Int64): Zero-based offset into the supplied backing storage.
#
# Returns:
#     The computed Int64 value: int64(bytes[offset]) + int64(bytes[offset + 1]) * 256 +
#     int64(bytes[offset + 2]) * 65536 + int64(bytes[offset + 3]) * 16777216.
#
# Errors:
#     Truncated GLB container.
fn se_import_u32(bytes :: Array, offset :: Int64) -> Int64 {
    if offset < 0 || offset + 4 > bytes.size() {
        throw "Truncated GLB container";
    }
    return int64(bytes[offset])
        +int64(bytes[offset + 1]) * 256
        +int64(bytes[offset + 2]) * 65536
        +int64(bytes[offset + 3]) * 16777216;
}

# Bounded streaming JSON validation: no boxed document tree, and escaped keys
# are decoded before checking URIs. State is operation-local, never retained.

# Return the next JSON byte at the parser cursor without advancing it.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#
# Returns:
#     The Int64 value selected by the conditions described above.
fn se_import_json_peek(bytes :: Array, state :: Array<Int64>) -> Int64 {
    if state[0] >= state[1] {
        return -1;
    }
    return int64(bytes[state[0]]);
}

# Advance the JSON parser past permitted whitespace.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#
# Returns:
#     True when the operation completes without throwing.
fn se_import_json_ws(bytes :: Array, state :: Array<Int64>) -> Bool {
    code :: Int64 := se_import_json_peek(bytes, state);
    while code == 32 || code == 9 || code == 10 || code == 13 {
        state[0] = state[0] + 1;
        code = se_import_json_peek(bytes, state);
    }
    return true;
}

# Consume a required JSON punctuation byte or report malformed input.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#     code (Int64): Unicode codepoint or parsed character code.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid GLB JSON syntax.
fn se_import_json_expect(bytes :: Array, state :: Array<Int64>, code :: Int64) -> Bool {
    if se_import_json_peek(bytes, state) != code {
        throw "Invalid GLB JSON syntax";
    }
    state[0] = state[0] + 1;
    return true;
}

# Decode a hexadecimal JSON escape digit from the parser cursor.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#
# Returns:
#     The computed Int64 value: code.
#
# Errors:
#     Invalid GLB JSON Unicode escape.
fn se_import_json_hex(bytes :: Array, state :: Array<Int64>) -> Int64 {
    code :: Int64 := 0;
    i :: Int64 := 0;
    while i < 4 {
        digit :: Int64 := se_import_json_peek(bytes, state);
        value :: Int64 := -1;
        if digit >= 48 && digit <= 57 {
            value = digit - 48;
        }
        if digit >= 65 && digit <= 70 {
            value = digit - 55;
        }
        if digit >= 97 && digit <= 102 {
            value = digit - 87;
        }
        if value < 0 {
            throw "Invalid GLB JSON Unicode escape";
        }
        code = code * 16 + value;
        state[0] = state[0] + 1;
        i = i + 1;
    }
    return code;
}

# Append a validated Unicode codepoint to the JSON string builder.
#
# Parameters:
#     builder (Int64): Temporary string-builder handle used to append serialized output.
#     code (Int64): Unicode codepoint or parsed character code.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     GLB JSON strings cannot contain NUL.
fn se_import_json_codepoint(builder :: Int64, code :: Int64) -> Bool {
    if code == 0 {
        throw "GLB JSON strings cannot contain NUL";
    }
    if code < 128 {
        vcore.sb_append_char(builder, code);
    } else {
        if code < 2048 {
            vcore.sb_append_char(builder, 192 + code / 64);
            vcore.sb_append_char(builder, 128 + code % 64);
        } else {
            if code < 65536 {
                vcore.sb_append_char(builder, 224 + code / 4096);
                vcore.sb_append_char(builder, 128 + (code / 64) % 64);
                vcore.sb_append_char(builder, 128 + code % 64);
            } else {
                vcore.sb_append_char(builder, 240 + code / 262144);
                vcore.sb_append_char(builder, 128 + (code / 4096) % 64);
                vcore.sb_append_char(builder, 128 + (code / 64) % 64);
                vcore.sb_append_char(builder, 128 + code % 64);
            }
        }
    }
    return true;
}

# Decode a JSON string with validated escapes and Unicode handling.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Unterminated GLB JSON string or control character.
#     Invalid GLB JSON surrogate pair.
#     Invalid GLB JSON surrogate.
#     Invalid GLB JSON escape.
fn se_import_json_string(bytes :: Array, state :: Array<Int64>) -> String {
    se_import_json_expect(bytes, state, 34);
    builder :: Int64 := vcore.sb_create();
    while true {
        code :: Int64 := se_import_json_peek(bytes, state);
        state[0] = state[0] + 1;
        if code == 34 {
            return vcore.sb_build(builder);
        }
        if code < 32 {
            throw "Unterminated GLB JSON string or control character";
        }
        if code == 92 {
            code = se_import_json_peek(bytes, state);
            state[0] = state[0] + 1;
            if code == 117 {
                point :: Int64 := se_import_json_hex(bytes, state);
                if point >= 55296 && point <= 56319 {
                    se_import_json_expect(bytes, state, 92);
                    se_import_json_expect(bytes, state, 117);
                    low :: Int64 := se_import_json_hex(bytes, state);
                    if low < 56320 || low > 57343 {
                        throw "Invalid GLB JSON surrogate pair";
                    }
                    point = 65536 + (point - 55296) * 1024 + low - 56320;
                } else {
                    if point >= 56320 && point <= 57343 {
                        throw "Invalid GLB JSON surrogate";
                    }
                }
                se_import_json_codepoint(builder, point);
            } else {
                if code == 98 {
                    code = 8;
                } else {
                    if code == 102 {
                        code = 12;
                    } else {
                        if code == 110 {
                            code = 10;
                        } else {
                            if code == 114 {
                                code = 13;
                            } else {
                                if code == 116 {
                                    code = 9;
                                } else {
                                    if code != 34 && code != 92 && code != 47 {
                                        throw "Invalid GLB JSON escape";
                                    }
                                }
                            }
                        }
                    }
                }
                vcore.sb_append_char(builder, code);
            }
        } else {
            vcore.sb_append_char(builder, code);
        }
    }
    return "";
}

# Parse a JSON numeric token while enforcing its grammar.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Invalid GLB JSON number.
#     Invalid GLB JSON fraction.
#     Invalid GLB JSON exponent.
fn se_import_json_number(bytes :: Array, state :: Array<Int64>) -> String {
    start :: Int64 := state[0];
    code :: Int64 := se_import_json_peek(bytes, state);
    if code == 45 {
        state[0] = state[0] + 1;
        code = se_import_json_peek(bytes, state);
    }
    if code == 48 {
        state[0] = state[0] + 1;
    } else {
        if code < 49 || code > 57 {
            throw "Invalid GLB JSON number";
        }
        while code >= 48 && code <= 57 {
            state[0] = state[0] + 1;
            code = se_import_json_peek(bytes, state);
        }
    }
    code = se_import_json_peek(bytes, state);
    if code == 46 {
        state[0] = state[0] + 1;
        code = se_import_json_peek(bytes, state);
        if code < 48 || code > 57 {
            throw "Invalid GLB JSON fraction";
        }
        while code >= 48 && code <= 57 {
            state[0] = state[0] + 1;
            code = se_import_json_peek(bytes, state);
        }
    }
    if code == 101 || code == 69 {
        state[0] = state[0] + 1;
        code = se_import_json_peek(bytes, state);
        if code == 43 || code == 45 {
            state[0] = state[0] + 1;
            code = se_import_json_peek(bytes, state);
        }
        if code < 48 || code > 57 {
            throw "Invalid GLB JSON exponent";
        }
        while code >= 48 && code <= 57 {
            state[0] = state[0] + 1;
            code = se_import_json_peek(bytes, state);
        }
    }
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := start;
    while i < state[0] {
        vcore.sb_append_char(builder, int64(bytes[i]));
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

# Recursively validate JSON values under a depth budget and reject unsupported external GLB
# dependencies.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     state (Array<Int64>): Mutable numeric state/counters used by this operation.
#     context (Int64): JSON parser context identifying dependency-sensitive GLB fields.
#     depth (Int64): Current dependency/parser recursion depth, checked against the operation limit.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     GLB JSON exceeds 64 levels of nesting.
#     GLB has an external URI (with contextual detail).
#     Import supports GLB asset version 2.0.
#     GLB buffer byteLength is out of range.
#     GLB buffer byteLength must be a nonnegative integer.
#     GLB asset metadata must be an object.
#     GLB buffers must be an array.
#     GLB JSON has a trailing comma.
#     GLB buffer needs byteLength.
#     GLB embedded buffer does not match its BIN chunk.
#     GLB buffer must be an object.
#     Invalid GLB JSON value.
#     Truncated GLB JSON literal.
#     Invalid GLB JSON literal.
fn se_import_json_value(bytes :: Array, state :: Array<Int64>, context :: Int64, depth :: Int64) -> Bool {
    if depth > 64 {
        throw "GLB JSON exceeds 64 levels of nesting";
    }
    se_import_json_ws(bytes, state);
    code :: Int64 := se_import_json_peek(bytes, state);
    if code == 123 {
        state[0] = state[0] + 1;
        se_import_json_ws(bytes, state);
        length :: Int64 := -1;
        uri_present :: Bool := false;
        while se_import_json_peek(bytes, state) != 125 {
            key :: String := se_import_json_string(bytes, state);
            se_import_json_ws(bytes, state);
            se_import_json_expect(bytes, state, 58);
            se_import_json_ws(bytes, state);
            if key == "uri" {
                uri :: String := se_import_json_string(bytes, state);
                uri_present = true;
                if uri.size() < 5 || vraylib.text_subtext(uri, 0, 5) != "data:" {
                    throw "GLB has an external URI: "
                        +uri
                        +". Re-export with embedded buffers and images, then import the self-contained GLB.";
                }
            } else {
                if context == 1 && key == "version" {
                    if se_import_json_string(bytes, state) != "2.0" {
                        throw "Import supports GLB asset version 2.0";
                    }
                    state[2] = 1;
                } else {
                    if context == 3 && key == "byteLength" {
                        start :: Int64 := state[0];
                        number :: String := se_import_json_number(bytes, state);
                        if number.size() > 8 {
                            throw "GLB buffer byteLength is out of range";
                        }
                        i :: Int64 := start;
                        while i < state[0] {
                            if int64(bytes[i]) < 48 || int64(bytes[i]) > 57 {
                                throw "GLB buffer byteLength must be a nonnegative integer";
                            }
                            i = i + 1;
                        }
                        length = int64(number);
                    } else {
                        child :: Int64 := 0;
                        if depth == 0 && key == "asset" {
                            child = 1;
                            if se_import_json_peek(bytes, state) != 123 {
                                throw "GLB asset metadata must be an object";
                            }
                        }
                        if depth == 0 && key == "buffers" {
                            child = 2;
                            if se_import_json_peek(bytes, state) != 91 {
                                throw "GLB buffers must be an array";
                            }
                        }
                        se_import_json_value(bytes, state, child, depth + 1);
                    }
                }
            }
            se_import_json_ws(bytes, state);
            if se_import_json_peek(bytes, state) == 125 {
                break;
            }
            se_import_json_expect(bytes, state, 44);
            se_import_json_ws(bytes, state);
            if se_import_json_peek(bytes, state) == 125 {
                throw "GLB JSON has a trailing comma";
            }
        }
        state[0] = state[0] + 1;
        if context == 3 {
            if length < 0 {
                throw "GLB buffer needs byteLength";
            }
            if !uri_present && (state[4] != 0 || length > state[3] || state[3] - length > 3) {
                throw "GLB embedded buffer does not match its BIN chunk";
            }
        }
        return true;
    }
    if code == 91 {
        state[0] = state[0] + 1;
        se_import_json_ws(bytes, state);
        while se_import_json_peek(bytes, state) != 93 {
            child :: Int64 := 0;
            if context == 2 {
                child = 3;
                if se_import_json_peek(bytes, state) != 123 {
                    throw "GLB buffer must be an object";
                }
            }
            se_import_json_value(bytes, state, child, depth + 1);
            if context == 2 {
                state[4] = state[4] + 1;
            }
            se_import_json_ws(bytes, state);
            if se_import_json_peek(bytes, state) == 93 {
                break;
            }
            se_import_json_expect(bytes, state, 44);
            se_import_json_ws(bytes, state);
            if se_import_json_peek(bytes, state) == 93 {
                throw "GLB JSON has a trailing comma";
            }
        }
        state[0] = state[0] + 1;
        return true;
    }
    if code == 34 {
        se_import_json_string(bytes, state);
        return true;
    }
    if code == 45 || (code >= 48 && code <= 57) {
        se_import_json_number(bytes, state);
        return true;
    }
    literal :: String := "";
    if code == 116 {
        literal = "true";
    }
    if code == 102 {
        literal = "false";
    }
    if code == 110 {
        literal = "null";
    }
    if literal == "" {
        throw "Invalid GLB JSON value";
    }
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 0;
    while i < literal.size() {
        code = se_import_json_peek(bytes, state);
        if code < 0 {
            throw "Truncated GLB JSON literal";
        }
        vcore.sb_append_char(builder, code);
        state[0] = state[0] + 1;
        i = i + 1;
    }
    if vcore.sb_build(builder) != literal {
        throw "Invalid GLB JSON literal";
    }
    return true;
}

# Validate binary glTF headers/chunks and JSON dependencies before importing a self-contained GLB.
#
# Parameters:
#     bytes (Array): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Import requires a complete GLB 2.0 container.
#     GLB must start with an aligned JSON chunk of at most 1 MiB.
#     GLB must contain only JSON and an optional aligned BIN chunk.
#     GLB JSON root must be an object.
#     GLB JSON must contain asset.version 2.0 and no trailing data.
fn se_import_glb(bytes :: Array) -> Bool {
    if bytes.size() < 20
        || se_import_u32(bytes, 0) != 1179937895
        || se_import_u32(bytes, 4) != 2
        || se_import_u32(bytes, 8) != bytes.size() {
        throw "Import requires a complete GLB 2.0 container";
    }
    json_length :: Int64 := se_import_u32(bytes, 12);
    if se_import_u32(bytes, 16) != 1313821514
        || json_length <= 0
        || json_length > 1048576
        || json_length % 4 != 0
        || 20
        +json_length > bytes.size() {
        throw "GLB must start with an aligned JSON chunk of at most 1 MiB";
    }
    bin_length :: Int64 := 0;
    offset :: Int64 := 20 + json_length;
    if offset < bytes.size() {
        bin_length = se_import_u32(bytes, offset);
        if se_import_u32(bytes, offset + 4) != 5130562
            || bin_length % 4 != 0
            || offset
            +8
            +bin_length != bytes.size() {
            throw "GLB must contain only JSON and an optional aligned BIN chunk";
        }
    }
    state :: Array<Int64> := [20, 20 + json_length, 0, bin_length, 0];
    se_import_json_ws(bytes, state);
    if se_import_json_peek(bytes, state) != 123 {
        throw "GLB JSON root must be an object";
    }
    se_import_json_value(bytes, state, 0, 0);
    se_import_json_ws(bytes, state);
    if state[0] != state[1] || state[2] != 1 {
        throw "GLB JSON must contain asset.version 2.0 and no trailing data";
    }
    return true;
}

# Validate and copy a supported binary model/image into collision-safe project-owned asset storage
# under the import budget.
#
# Parameters:
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     source (String): Source payload to read or copy; retained text has a length prefix at element
#         zero.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Import requires an existing project directory and source file.
#     Import supports nonempty files up to 8 MiB; place larger complete assets in the project
#     manually.
#     Could not read the complete import source.
#     Source is not a PNG image.
#     Source is not a JPEG image.
#     Source is not a BMP image.
#     Cannot create project assets/imports directory.
#     Import source needs a filename.
#     Import filename collisions exhausted; rename the source file.
#     Imported path exceeds 255 UTF-8 bytes; shorten the filename or project path.
#     Import destination appeared; retry to choose a new filename.
#     Cannot write the imported asset.
#     Imported bytes failed verification; incomplete copy removed.
fn se_import_asset(root :: String, source :: String) -> String {
    ext :: String := vraylib.text_to_lower(vfs.extension(source));
    if ext != ".glb" && !se_image_asset(source) {
        throw "Project import supports self-contained GLB and PNG/JPEG/BMP images only. Re-export OBJ/glTF/IQM/M3D as an embedded GLB, or copy the complete dependency tree into the project and assign it there.";
    }
    if !vfs.is_dir(root) || !vfs.is_file(source) {
        throw "Import requires an existing project directory and source file";
    }
    length :: Int64 := vfs.size(source);
    if length <= 0 || length > 8388608 {
        throw "Import supports nonempty files up to 8 MiB; place larger complete assets in the project manually";
    }
    bytes :: Array := se_import_read_bytes(source, length);
    if bytes.size() != length {
        throw "Could not read the complete import source";
    }
    if ext == ".glb" {
        se_import_glb(bytes);
    } else {
        if ext == ".png"
            && (bytes.size() < 8 || bytes[0] != 137 || bytes[1] != 80 || bytes[2] != 78 || bytes[3] != 71 || bytes[4] != 13 || bytes[5] != 10 || bytes[6] != 26 || bytes[7] != 10) {
            throw "Source is not a PNG image";
        }
        if (ext == ".jpg" || ext == ".jpeg")
            && (bytes.size() < 3 || bytes[0] != 255 || bytes[1] != 216 || bytes[2] != 255) {
            throw "Source is not a JPEG image";
        }
        if ext == ".bmp" && (bytes.size() < 2 || bytes[0] != 66 || bytes[1] != 77) {
            throw "Source is not a BMP image";
        }
    }
    imports :: String := vfs.join(root, "assets/imports");
    if !vfs.mkdir_p(imports) || !vfs.is_dir(imports) {
        throw "Cannot create project assets/imports directory";
    }
    stem :: String := vfs.stem(source);
    if stem == "" || stem == "." || stem == ".." {
        throw "Import source needs a filename";
    }
    suffix :: Int64 := 0;
    relative :: String := "assets/imports/" + stem + ext;
    while vfs.exists(vfs.join(root, relative)) {
        suffix = suffix + 1;
        if suffix > 9999 {
            throw "Import filename collisions exhausted; rename the source file";
        }
        relative = "assets/imports/" + stem + "-" + string(suffix) + ext;
    }
    destination :: String := vfs.join(root, relative);
    if relative.size() >= 256 || destination.size() >= 256 {
        throw "Imported path exceeds 255 UTF-8 bytes; shorten the filename or project path";
    }
    # vfs.copy uses string writes and is not binary-safe. Keep fully sized
    # byte arrays local; only the resulting path's bytes are retained by UI.
    if vfs.exists(destination) {
        throw "Import destination appeared; retry to choose a new filename";
    }
    if !vfs.write_bytes(destination, bytes) {
        vfs.remove(destination);
        throw "Cannot write the imported asset";
    }
    try {
        copy :: Array := se_import_read_bytes(destination, length);
        matches :: Bool := copy.size() == bytes.size();
        i :: Int64 := 0;
        while matches && i < bytes.size() {
            if copy[i] != bytes[i] {
                matches = false;
            }
            i = i + 1;
        }
        if !matches {
            throw "Imported bytes failed verification; incomplete copy removed";
        }
    } catch (error) {
        vfs.remove(destination);
        throw error;
    }
    return relative;
}

# Decode the retained path associated with a thumbnail slot.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     index (Int64): Zero-based entry index in the relevant store.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_preview_path(ui :: SEUI, index :: Int64) -> String {
    offset :: Int64 := index * 256;
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 1;
    while i <= ui.preview_paths[offset] {
        vcore.sb_append_char(builder, ui.preview_paths[offset + i]);
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

# Find a cached thumbnail by exact retained asset path; return -1 if absent.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     Cached thumbnail slot, or -1 if the path is not present.
fn se_preview_find(ui :: SEUI, path :: String) -> Int64 {
    i :: Int64 := 0;
    while i < 16 {
        if ui.preview_tokens[i] != 0 && se_preview_path(ui, i) == path {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

# Close cached thumbnail textures and clear retained cache slots after asset-directory changes.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#
# Returns:
#     True when the operation completes without throwing.
fn se_preview_invalidate(ui :: SEUI, app :: vraylib.App) -> Bool {
    i :: Int64 := 0;
    while i < 16 {
        if ui.preview_tokens[i] > 0 {
            texture :: vraylib.Texture := vraylib.Texture(ui.preview_tokens[i], app.token, true);
            if texture.valid() {
                texture.close();
            }
        }
        ui.preview_tokens[i] = 0;
        ui.preview_paths[i * 256] = 0;
        i = i + 1;
    }
    ui.browser_state[0] = 0;
    ui.browser_state[2] = 0;
    ui.browser_state[3] = -1;
    ui.external_path[0] = 0;
    return true;
}

# Resolve or load a bounded image thumbnail, evicting an old cache entry when necessary.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The Int64 value selected by the conditions described above.
fn se_preview_request(ui :: SEUI, app :: vraylib.App, path :: String) -> Int64 {
    found :: Int64 := se_preview_find(ui, path);
    if found >= 0 {
        return ui.preview_tokens[found];
    }
    if path.size() >= 256 {
        return -2;
    }
    slot :: Int64 := ui.browser_state[0] % 16;
    if ui.preview_tokens[slot] > 0 {
        old :: vraylib.Texture := vraylib.Texture(ui.preview_tokens[slot], app.token, true);
        old.close();
    }
    ui.preview_tokens[slot] = -2;
    se_text_write(ui.edit_buffer, path);
    i :: Int64 := 0;
    while i <= ui.edit_buffer[0] {
        ui.preview_paths[slot * 256 + i] = ui.edit_buffer[i];
        i = i + 1;
    }
    ui.browser_state[0] = (slot + 1) % 16;
    if !se_image_asset(path) {
        return -2;
    }
    ui.browser_state[1] = ui.browser_state[1] + 1;
    image :: vraylib.Image := vraylib.Image(0, app.token);
    try {
        if !vfs.is_file(path) || vfs.size(path) > 8388608 {
            return -2;
        }
        image = app.load_image(path);
        width :: Int64 := image.width();
        height :: Int64 := image.height();
        if width <= 0 || height <= 0 || width * height > 16777216 {
            return -2;
        }
        scale :: Float64 := vmath.min(1.0, 128.0 / vmath.max(float64(width), float64(height)));
        image.resize(
            int64(vmath.max(1.0, float64(width) * scale)),
            int64(vmath.max(1.0, float64(height) * scale))
        );
        texture :: vraylib.Texture := app.texture(image);
        try {
            vraylib.set_texture_filter(texture.native_handle(), vraylib.TEXTURE_FILTER_BILINEAR);
        } catch (error) {
            texture.close();
            throw error;
        }
        ui.preview_tokens[slot] = texture.token;
    } catch (error) {
        ui.preview_tokens[slot] = -1;
    } finally {
        if image.token != 0 {
            image.close();
        }
    }
    return ui.preview_tokens[slot];
}

# Draw a cached thumbnail scaled/cropped into a rectangle when the requested preview is available.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     rectangle (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit
#         testing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_preview_draw(ui :: SEUI, app :: vraylib.App, path :: String, rectangle :: vraylib.Rectangle) -> Bool {
    vraylib.draw_rectangle_rec(rectangle, vraylib.rgba(17, 23, 33, 255));
    vraylib.draw_rectangle_lines_ex(rectangle, 1.0, vraylib.rgba(58, 70, 89, 255));
    slot :: Int64 := se_preview_find(ui, path);
    if slot >= 0 && ui.preview_tokens[slot] > 0 {
        texture :: vraylib.Texture := vraylib.Texture(ui.preview_tokens[slot], app.token, true);
        width :: Float64 := float64(texture.width());
        height :: Float64 := float64(texture.height());
        scale :: Float64 := vmath.min((rectangle.width - 6.0) / width, (rectangle.height - 6.0) / height);
        destination :: vraylib.Rectangle := se_rect(
            rectangle.x + (rectangle.width - width * scale) * 0.5,
            rectangle.y + (rectangle.height - height * scale) * 0.5,
            width * scale,
            height * scale
        );
        texture.draw_region(
            se_rect(0.0, 0.0, width, height),
            destination,
            vraylib.vec2(0.0, 0.0),
            0.0,
            vraylib.WHITE
        );
        return true;
    }
    ext :: String := vraylib.text_to_lower(vfs.extension(path));
    label :: String := "File";
    if ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".iqm" || ext == ".m3d" {
        label = "Model";
    }
    if se_image_asset(path) {
        label = "No image";
    }
    if slot >= 0 && ui.preview_tokens[slot] == -1 {
        label = "Failed";
    }
    se_draw_text(
        se_fit(label, rectangle.width - 8.0, 12),
        int64(rectangle.x + 4.0),
        int64(rectangle.y + rectangle.height * 0.5 - 6.0),
        12,
        vraylib.GRAY
    );
    return false;
}
