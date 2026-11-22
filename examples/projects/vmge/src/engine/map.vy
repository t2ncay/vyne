# Fixed-capacity storage is allocated before managed frames. Edits only replace
# numeric elements and static literals. Original numeric spellings are retained
# at load time so saving a legacy map does not lose Float64 precision.
use native vfs;
use native vcore;
use native vmath;
ruleset { dynamic_casting };

interface VMGEMap {
    data :: Array,
    source :: Array,
    state :: Array,
    capacity :: Int64,
    count() -> Int64 { return int64(self.state[0]); }
    value(index :: Int64, field :: Int64) -> Float64 {
        if index < 0 || index >= self.count() || field < 0 || field > 4 { throw "Map index out of range"; }
        return float64(self.data[index * 5 + field]);
    }
    append(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
        vmge_validate_cube(x, y, z, size, slot);
        index :: Int64 := self.count();
        if index >= self.capacity { throw "Map capacity reached"; }
        self.data[index * 5] = x;
        self.data[index * 5 + 1] = y;
        self.data[index * 5 + 2] = z;
        self.data[index * 5 + 3] = size;
        self.data[index * 5 + 4] = float64(slot);
        field :: Int64 := 0;
        while field < 5 { self.source[index * 5 + field] = ""; field = field + 1; }
        self.state[0] = index + 1;
        return true;
    }
    occupied(x :: Float64, y :: Float64, z :: Float64) -> Bool {
        i :: Int64 := 0;
        while i < self.count() {
            if self.value(i, 0) == x && self.value(i, 1) == y && self.value(i, 2) == z { return true; }
            i = i + 1;
        }
        return false;
    }
    place(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
        if self.occupied(x, y, z) { return false; }
        return self.append(x, y, z, size, slot);
    }
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

fn vmge_validate_cube(x :: Float64, y :: Float64, z :: Float64, size :: Float64, slot :: Int64) -> Bool {
    if !(vmath.abs(x) <= 1000000.0 && vmath.abs(y) <= 1000000.0 && vmath.abs(z) <= 1000000.0 && size > 0.0 && size <= 1000000.0) { throw "Map cube requires finite coordinates and a positive size (maximum 1000000)"; }
    if slot < 0 || slot >= 8 { throw "Map texture slot must be an integer from 0 to 7"; }
    return true;
}

fn vmge_map_create(capacity :: Int64) -> VMGEMap {
    if capacity < 1 || capacity > 65536 { throw "Map capacity must be between 1 and 65536"; }
    data :: Array := [];
    source :: Array := [];
    i :: Int64 := 0;
    while i < capacity * 5 { data.push(0.0); source.push(""); i = i + 1; }
    return VMGEMap(data, source, [0], capacity);
}

fn vmge_space(ch :: String) -> Bool { return ch == " " || ch == "\t" || ch == "\r" || ch == "\n"; }
fn vmge_trim(text :: String) -> String {
    start :: Int64 := 0;
    end :: Int64 := text.size();
    while start < end && vmge_space(text[start]) { start = start + 1; }
    while end > start && vmge_space(text[end - 1]) { end = end - 1; }
    result :: String := "";
    while start < end { result = result + text[start]; start = start + 1; }
    return result;
}

# Unlike the historical split helper, retain empty fields for validation.
fn vmge_split(text :: String, separator :: String) -> Array {
    parts :: Array := [];
    part :: String := "";
    i :: Int64 := 0;
    while i < text.size() {
        if text[i] == separator { parts.push(part); part = ""; }
        else { part = part + text[i]; }
        i = i + 1;
    }
    parts.push(part);
    return parts;
}

fn vmge_digit(ch :: String) -> Bool { return ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4" || ch == "5" || ch == "6" || ch == "7" || ch == "8" || ch == "9"; }
fn vmge_number(raw :: String) -> Float64 {
    text :: String := vmge_trim(raw);
    i :: Int64 := 0;
    digits :: Int64 := 0;
    if text.size() == 0 { throw "Empty numeric map field"; }
    if text[i] == "+" || text[i] == "-" { i = i + 1; }
    while i < text.size() && vmge_digit(text[i]) { digits = digits + 1; i = i + 1; }
    if i < text.size() && text[i] == "." {
        i = i + 1;
        while i < text.size() && vmge_digit(text[i]) { digits = digits + 1; i = i + 1; }
    }
    if digits == 0 { throw "Invalid numeric map field: " + text; }
    if i < text.size() && (text[i] == "e" || text[i] == "E") {
        i = i + 1;
        if i < text.size() && (text[i] == "+" || text[i] == "-") { i = i + 1; }
        exponent_digits :: Int64 := 0;
        while i < text.size() && vmge_digit(text[i]) { exponent_digits = exponent_digits + 1; i = i + 1; }
        if exponent_digits == 0 { throw "Invalid numeric map exponent: " + text; }
    }
    if i != text.size() { throw "Invalid numeric map field: " + text; }
    value :: Float64 := float64(text);
    if !(vmath.abs(value) <= 1000000.0) { throw "Map numeric field is not finite or exceeds 1000000"; }
    return value;
}

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
                if fields.size() != 5 { throw "Expected x,y,z,size,slot"; }
                x :: Float64 := vmge_number(fields[0]);
                y :: Float64 := vmge_number(fields[1]);
                z :: Float64 := vmge_number(fields[2]);
                size :: Float64 := vmge_number(fields[3]);
                slot :: Float64 := vmge_number(fields[4]);
                if slot < 0.0 || slot > 7.0 || slot != vmath.floor(slot) { throw "Map texture slot must be an integer from 0 to 7"; }
                world.append(x, y, z, size, int64(slot));
                field :: Int64 := 0;
                while field < 5 { world.source[(world.count() - 1) * 5 + field] = vmge_trim(fields[field]); field = field + 1; }
            } catch (error) { throw "Map record " + string(row_number) + ": " + string(error); }
        }
    };
    return world;
}

fn vmge_map_load(path :: String, capacity :: Int64, allow_missing :: Bool) -> VMGEMap {
    if !vfs.exists(path) {
        if allow_missing { return vmge_map_create(capacity); }
        throw "Map file not found: " + path;
    }
    text := vfs.read(path);
    if text == null { throw "Could not read map: " + path; }
    return vmge_map_parse(text, capacity);
}

fn vmge_map_text(world :: VMGEMap) -> String {
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 0;
    while i < world.count() {
        field :: Int64 := 0;
        while field < 5 {
            if field != 0 { vcore.sb_append(builder, ","); }
            spelling :: String := world.source[i * 5 + field];
            if spelling != "" { vcore.sb_append(builder, spelling); }
            else {
                if field == 4 { vcore.sb_append(builder, string(int64(world.value(i, field)))); }
                else { vcore.sb_append(builder, string(world.value(i, field))); }
            }
            field = field + 1;
        }
        vcore.sb_append(builder, ";\n");
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

fn vmge_map_save(world :: VMGEMap, path :: String) -> Bool {
    if !vfs.write(path, vmge_map_text(world)) { throw "Could not save map: " + path; }
    return true;
}
