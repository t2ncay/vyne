# Fixed-capacity storage is allocated before managed frames. Edits only replace
# numeric elements and static literals. Original numeric spellings are retained
# at load time so saving a legacy map does not lose Float64 precision.
use native vfs;
use native vcore;
use native vmath;
ruleset {
    dynamic_casting
};

# Fixed-capacity legacy cube map storing five numeric fields per record and optional original numeric
# spellings. Edits reuse existing rows; append preserves duplicates and place rejects occupied
# centers.
#
# Fields:
#     data: Five numeric values per reserved cube row: center X/Y/Z, side length, texture slot.
#     source: Original numeric spellings per cube field, populated before frames for exact legacy
#         roundtrips.
#     state: Shared live-record count at index zero.
#     capacity: Maximum reserved cube rows, from 1 through 65,536.
interface VMGEMap {
    data :: Array,
    source :: Array,
    state :: Array,
    capacity :: Int64,

    # Return the number of live cube records.
    #
    # Returns:
    #     The live entry count.
    count() -> Int64 {
        return int64(self.state[0]);
    }

    # Read one numeric cube field: 0 X, 1 Y, 2 Z, 3 side length, or 4 texture slot.
    #
    # Parameters:
    #     index (Int64): Zero-based entry index in the relevant store.
    #     field (Int64): Cube field: 0 center X, 1 center Y, 2 center Z, 3 side length, 4 texture slot.
    #
    # Returns:
    #     The computed Float64 value: float64(self.data[index * 5 + field]).
    #
    # Errors:
    #     Map index out of range.
    value(index :: Int64, field :: Int64) -> Float64 {
        if index < 0 || index >= self.count() || field < 0 || field > 4 {
            throw "Map index out of range";
        }
        return float64(self.data[index * 5 + field]);
    }

    # Validate and append a cube record, preserving duplicate centers and clearing imported spellings for
    # the new row.
    #
    # Parameters:
    #     x (Float64): World-space X coordinate.
    #     y (Float64): World-space Y coordinate.
    #     z (Float64): World-space Z coordinate.
    #     size (Float64): Positive cube side length in world units, at most 1,000,000.
    #     slot (Int64): Zero-based texture palette slot, from 0 through 7.
    #
    # Returns:
    #     True when the operation completes without throwing.
    #
    # Errors:
    #     Map capacity reached.
    append(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
        vmge_validate_cube(x, y, z, size, slot);
        index :: Int64 := self.count();
        if index >= self.capacity {
            throw "Map capacity reached";
        }
        self.data[index * 5] = x;
        self.data[index * 5 + 1] = y;
        self.data[index * 5 + 2] = z;
        self.data[index * 5 + 3] = size;
        self.data[index * 5 + 4] = float64(slot);
        field :: Int64 := 0;
        while field < 5 {
            self.source[index * 5 + field] = "";
            field = field + 1;
        }
        self.state[0] = index + 1;
        return true;
    }

    # Report whether any cube record has the exact supplied center coordinates.
    #
    # Parameters:
    #     x (Float64): World-space X coordinate.
    #     y (Float64): World-space Y coordinate.
    #     z (Float64): World-space Z coordinate.
    #
    # Returns:
    #     True if any live cube has the exact requested center; false otherwise.
    occupied(x :: Float64, y :: Float64, z :: Float64) -> Bool {
        i :: Int64 := 0;
        while i < self.count() {
            if self.value(i, 0) == x && self.value(i, 1) == y && self.value(i, 2) == z {
                return true;
            }
            i = i + 1;
        }
        return false;
    }

    # Append a cube only when its center is unoccupied; return false for an existing center.
    #
    # Parameters:
    #     x (Float64): World-space X coordinate.
    #     y (Float64): World-space Y coordinate.
    #     z (Float64): World-space Z coordinate.
    #     size (Float64): Positive cube side length in world units, at most 1,000,000.
    #     slot (Int64): Zero-based texture palette slot, from 0 through 7.
    #
    # Returns:
    #     True after a new cube is appended; false when the center is occupied.
    place(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
        if self.occupied(x, y, z) {
            return false;
        }
        return self.append(x, y, z, size, slot);
    }

    # Remove all records at the exact supplied center, compacting survivors while preserving their order
    # and spellings.
    #
    # Parameters:
    #     x (Float64): World-space X coordinate.
    #     y (Float64): World-space Y coordinate.
    #     z (Float64): World-space Z coordinate.
    #
    # Returns:
    #     Number of removed records; zero when the center was absent.
    remove(x :: Float64, y :: Float64, z :: Float64) -> Int64 {
        kept :: Int64 := 0;
        original :: Int64 := self.count();
        i :: Int64 := 0;
        while i < original {
            if !(self.value(i, 0) == x && self.value(i, 1) == y && self.value(i, 2) == z) {
                field :: Int64 := 0;
                while field < 5 {
                    self.data[kept * 5 + field] = self.data[i * 5 + field];
                    self.source[kept * 5 + field] = self.source[i * 5 + field];
                    field = field + 1;
                }
                kept = kept + 1;
            }
            i = i + 1;
        }
        self.state[0] = kept;
        return original - kept;
    }
}

# Validate finite cube coordinates, a positive bounded side length, and a texture slot from 0 through
# 7.
#
# Parameters:
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#     size (Float64): Positive cube side length in world units, at most 1,000,000.
#     slot (Int64): Zero-based texture palette slot, from 0 through 7.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Map cube requires finite coordinates and a positive size (maximum 1000000).
#     Map texture slot must be an integer from 0 to 7.
fn vmge_validate_cube(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
    if !(vmath.abs(x) <= 1000000.0 && vmath.abs(y) <= 1000000.0 && vmath.abs(z) <= 1000000.0 && size > 0.0 && size <= 1000000.0) {
        throw "Map cube requires finite coordinates and a positive size (maximum 1000000)";
    }
    if slot < 0 || slot >= 8 {
        throw "Map texture slot must be an integer from 0 to 7";
    }
    return true;
}

# Allocate the legacy map's fixed-capacity numeric rows, spelling rows, and shared count before
# frames.
#
# Parameters:
#     capacity (Int64): Maximum cube records, from 1 through 65,536.
#
# Returns:
#     VMGEMap containing the resulting retained state or payload.
#
# Errors:
#     Map capacity must be between 1 and 65536.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn vmge_map_create(capacity :: Int64) -> VMGEMap {
    if capacity < 1 || capacity > 65536 {
        throw "Map capacity must be between 1 and 65536";
    }
    data :: Array := [];
    source :: Array := [];
    i :: Int64 := 0;
    while i < capacity * 5 {
        data.push(0.0);
        source.push("");
        i = i + 1;
    }
    return VMGEMap(data, source, [0], capacity);
}

# Classify spaces, tabs, carriage returns, and newlines as map-record whitespace.
#
# Parameters:
#     ch (String): Single character to classify.
#
# Returns:
#     The Boolean result of ch == " " || ch == "\t" || ch == "\r" || ch == "\n".
fn vmge_space(ch :: String) -> Bool {
    return ch == " " || ch == "\t" || ch == "\r" || ch == "\n";
}

# Remove surrounding map whitespace while retaining the interior text.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn vmge_trim(text :: String) -> String {
    start :: Int64 := 0;
    end :: Int64 := text.size();
    while start < end && vmge_space(text[start]) {
        start = start + 1;
    }
    while end > start && vmge_space(text[end - 1]) {
        end = end - 1;
    }
    result :: String := "";
    while start < end {
        result = result + text[start];
        start = start + 1;
    }
    return result;
}

# Unlike the historical split helper, retain empty fields for validation.

# Split text while preserving empty fields so malformed map records can be diagnosed.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     separator (String): Delimiter used to split text.
#
# Returns:
#     Array containing the resulting retained state or payload.
fn vmge_split(text :: String, separator :: String) -> Array {
    parts :: Array := [];
    part :: String := "";
    i :: Int64 := 0;
    while i < text.size() {
        if text[i] == separator {
            parts.push(part);
            part = "";
        } else {
            part = part + text[i];
        }
        i = i + 1;
    }
    parts.push(part);
    return parts;
}

# Report whether a character is an ASCII decimal digit.
#
# Parameters:
#     ch (String): Single character to classify.
#
# Returns:
#     The Boolean result of ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4" || ch == "5"
#     || ch == "6" || ch == "7" || ch == "8" || ch == "9".
fn vmge_digit(ch :: String) -> Bool {
    return ch == "0"
        || ch == "1"
        || ch == "2"
        || ch == "3"
        || ch == "4"
        || ch == "5"
        || ch == "6"
        || ch == "7"
        || ch == "8"
        || ch == "9";
}

# Parse a finite decimal/scientific map number, rejecting malformed numeric syntax.
#
# Parameters:
#     raw (String): Persisted decimal/scientific numeric text to parse.
#
# Returns:
#     The computed Float64 value: value.
#
# Errors:
#     Empty numeric map field.
#     Invalid numeric map field (with contextual detail).
#     Invalid numeric map exponent (with contextual detail).
#     Map numeric field is not finite or exceeds 1000000.
fn vmge_number(raw :: String) -> Float64 {
    text :: String := vmge_trim(raw);
    i :: Int64 := 0;
    digits :: Int64 := 0;
    if text.size() == 0 {
        throw "Empty numeric map field";
    }
    if text[i] == "+" || text[i] == "-" {
        i = i + 1;
    }
    while i < text.size() && vmge_digit(text[i]) {
        digits = digits + 1;
        i = i + 1;
    }
    if i < text.size() && text[i] == "." {
        i = i + 1;
        while i < text.size() && vmge_digit(text[i]) {
            digits = digits + 1;
            i = i + 1;
        }
    }
    if digits == 0 {
        throw "Invalid numeric map field: " + text;
    }
    if i < text.size() && (text[i] == "e" || text[i] == "E") {
        i = i + 1;
        if i < text.size() && (text[i] == "+" || text[i] == "-") {
            i = i + 1;
        }
        exponent_digits :: Int64 := 0;
        while i < text.size() && vmge_digit(text[i]) {
            exponent_digits = exponent_digits + 1;
            i = i + 1;
        }
        if exponent_digits == 0 {
            throw "Invalid numeric map exponent: " + text;
        }
    }
    if i != text.size() {
        throw "Invalid numeric map field: " + text;
    }
    value :: Float64 := float64(text);
    if !(vmath.abs(value) <= 1000000.0) {
        throw "Map numeric field is not finite or exceeds 1000000";
    }
    return value;
}

# Parse semicolon-separated legacy cube records, validating every field and retaining imported numeric
# spellings.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     capacity (Int64): Maximum number of retained entries or UTF-8 bytes for this constructor/parser.
#
# Returns:
#     VMGEMap containing the resulting retained state or payload.
#
# Errors:
#     Expected x,y,z,size,slot.
#     Map texture slot must be an integer from 0 to 7.
#     Map record.
fn vmge_map_parse(text :: String, capacity :: Int64) -> VMGEMap {
    world :: VMGEMap := vmge_map_create(capacity);
    rows :: Array := vmge_split(text, ";");
    row_number :: Int64 := 0;
    through raw :: rows -> loop {
        row_number = row_number + 1;
        row :: String := vmge_trim(raw);
        if row != "" {
            try {
                fields :: Array := vmge_split(row, ",");
                if fields.size() != 5 {
                    throw "Expected x,y,z,size,slot";
                }
                x :: Float64 := vmge_number(fields[0]);
                y :: Float64 := vmge_number(fields[1]);
                z :: Float64 := vmge_number(fields[2]);
                size :: Float64 := vmge_number(fields[3]);
                slot :: Float64 := vmge_number(fields[4]);
                if slot < 0.0 || slot > 7.0 || slot != vmath.floor(slot) {
                    throw "Map texture slot must be an integer from 0 to 7";
                }
                world.append(x, y, z, size, int64(slot));
                field :: Int64 := 0;
                while field < 5 {
                    world.source[(world.count() - 1) * 5 + field] = vmge_trim(fields[field]);
                    field = field + 1;
                }
            } catch (error) {
                throw "Map record " + string(row_number) + ": " + string(error);
            }
        }
    };
    return world;
}

# Load a legacy DAT map, optionally allowing a missing file to represent an empty editor map.
#
# Parameters:
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     capacity (Int64): Maximum number of retained entries or UTF-8 bytes for this constructor/parser.
#     allow_missing (Bool): Whether a missing DAT file may produce an empty map instead of an error.
#
# Returns:
#     VMGEMap containing the resulting retained state or payload.
#
# Errors:
#     Map file not found (with contextual detail).
#     Could not read map (with contextual detail).
fn vmge_map_load(path :: String, capacity :: Int64, allow_missing :: Bool) -> VMGEMap {
    if !vfs.exists(path) {
        if allow_missing {
            return vmge_map_create(capacity);
        }
        throw "Map file not found: " + path;
    }
    text := vfs.read(path);
    if text == null {
        throw "Could not read map: " + path;
    }
    return vmge_map_parse(text, capacity);
}

# Serialize live map records, retaining imported numeric spellings and normalizing record whitespace.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn vmge_map_text(world :: VMGEMap) -> String {
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 0;
    while i < world.count() {
        field :: Int64 := 0;
        while field < 5 {
            if field != 0 {
                vcore.sb_append(builder, ",");
            }
            spelling :: String := world.source[i * 5 + field];
            if spelling != "" {
                vcore.sb_append(builder, spelling);
            } else {
                if field == 4 {
                    vcore.sb_append(builder, string(int64(world.value(i, field))));
                } else {
                    vcore.sb_append(builder, string(world.value(i, field)));
                }
            }
            field = field + 1;
        }
        vcore.sb_append(builder, ";\n");
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

# Write the legacy DAT map and report save failure without discarding the in-memory map.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Could not save map (with contextual detail).
fn vmge_map_save(world :: VMGEMap, path :: String) -> Bool {
    if !vfs.write(path, vmge_map_text(world)) {
        throw "Could not save map: " + path;
    }
    return true;
}
