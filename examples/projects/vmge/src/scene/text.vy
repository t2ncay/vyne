# Retained Int64 UTF-8 slots: element zero is byte length, remaining elements
# are bytes. No String or frame-created boxed header is retained by an edit.
use external "vraylib/vraylib.vy";
use native vcore;
ruleset {
    dynamic_casting
};

# Return the UTF-8 byte width needed to encode a Unicode codepoint.
#
# Parameters:
#     code (Int64): Unicode codepoint or parsed character code.
#
# Returns:
#     The Int64 value selected by the conditions described above.
#
# Errors:
#     Invalid Unicode codepoint.
fn se_utf8_length(code :: Int64) -> Int64 {
    if code < 0 || code > 1114111 || (code >= 55296 && code <= 57343) {
        throw "Invalid Unicode codepoint";
    }
    if code < 128 {
        return 1;
    }
    if code < 2048 {
        return 2;
    }
    if code < 65536 {
        return 3;
    }
    return 4;
}

# Encode a Unicode codepoint into the supplied byte buffer at an offset and return the number of bytes
# written.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     offset (Int64): Zero-based offset into the supplied backing storage.
#     code (Int64): Unicode codepoint or parsed character code.
#
# Returns:
#     The computed Int64 value: width.
#
# Errors:
#     Text capacity exceeded or embedded NUL.
fn se_utf8_put(bytes :: Array<Int64>, offset :: Int64, code :: Int64) -> Int64 {
    width :: Int64 := se_utf8_length(code);
    if offset < 1 || offset + width > bytes.size() || code == 0 {
        throw "Text capacity exceeded or embedded NUL";
    }
    if width == 1 {
        bytes[offset] = code;
    }
    if width == 2 {
        bytes[offset] = 192 + code / 64;
        bytes[offset + 1] = 128 + (code % 64);
    }
    if width == 3 {
        bytes[offset] = 224 + code / 4096;
        bytes[offset + 1] = 128 + ((code / 64) % 64);
        bytes[offset + 2] = 128 + (code % 64);
    }
    if width == 4 {
        bytes[offset] = 240 + code / 262144;
        bytes[offset + 1] = 128 + ((code / 4096) % 64);
        bytes[offset + 2] = 128 + ((code / 64) % 64);
        bytes[offset + 3] = 128 + (code % 64);
    }
    return width;
}

# Validate and copy text into retained UTF-8 storage whose element zero contains its byte length.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Text capacity exceeded.
fn se_text_write(bytes :: Array<Int64>, text :: String) -> Bool {
    count :: Int64 := vraylib.buffer("int", 1);
    points :: Int64 := 0;
    try {
        points = vraylib.load_codepoints(text, count);
        length :: Int64 := int64(vraylib.buffer_read(count, 0));
        required :: Int64 := 0;
        i :: Int64 := 0;
        while i < length {
            required = required + se_utf8_length(int64(vraylib.buffer_read(points, i)));
            i = i + 1;
        }
        if required >= bytes.size() {
            throw "Text capacity exceeded";
        }
        offset :: Int64 := 1;
        i = 0;
        while i < length {
            offset = offset + se_utf8_put(bytes, offset, int64(vraylib.buffer_read(points, i)));
            i = i + 1;
        }
        bytes[0] = required;
    } finally {
        if points != 0 {
            vraylib.unload_codepoints(points);
        }
        vraylib.release(count);
    }
    return true;
}

# Decode the length-prefixed retained byte payload into a temporary string.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_text_read(bytes :: Array<Int64>) -> String {
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 1;
    while i <= bytes[0] {
        vcore.sb_append_char(builder, bytes[i]);
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

# Copy retained UTF-8 bytes into an existing destination with capacity validation.
#
# Parameters:
#     destination (Array<Int64>): Destination retained UTF-8 byte buffer.
#     source (Array<Int64>): Source retained UTF-8 byte buffer.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Text capacity exceeded.
fn se_text_copy(destination :: Array<Int64>, source :: Array<Int64>) -> Bool {
    if source[0] >= destination.size() {
        throw "Text capacity exceeded";
    }
    i :: Int64 := 0;
    while i <= source[0] {
        destination[i] = source[i];
        i = i + 1;
    }
    return true;
}

# Remove the final complete UTF-8 codepoint from a retained text buffer.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#
# Returns:
#     false when length == 0. Otherwise returns the completed action/predicate result.
fn se_text_backspace(bytes :: Array<Int64>) -> Bool {
    length :: Int64 := bytes[0];
    if length == 0 {
        return false;
    }
    while length > 1 && bytes[length] >= 128 && bytes[length] < 192 {
        length = length - 1;
    }
    bytes[0] = length - 1;
    return true;
}

# Serialize retained UTF-8 bytes as hexadecimal text for delimiter-safe persistence.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_text_hex(bytes :: Array<Int64>) -> String {
    alphabet :: String := "0123456789abcdef";
    builder :: Int64 := vcore.sb_create();
    i :: Int64 := 1;
    while i <= bytes[0] {
        vcore.sb_append(builder, alphabet[bytes[i] / 16] + alphabet[bytes[i] % 16]);
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

# Convert one hexadecimal character to its numeric nibble, rejecting invalid digits.
#
# Parameters:
#     ch (String): Single character to classify.
#
# Returns:
#     The computed Int64 value: i.
#
# Errors:
#     Invalid scene text hex digit.
fn se_hex_digit(ch :: String) -> Int64 {
    alphabet :: String := "0123456789abcdef";
    i :: Int64 := 0;
    while i < 16 {
        if alphabet[i] == ch {
            return i;
        }
        i = i + 1;
    }
    throw "Invalid scene text hex digit";
}

# Decode hexadecimal persisted text into a retained length-prefixed UTF-8 buffer.
#
# Parameters:
#     bytes (Array<Int64>): UTF-8 byte buffer; retained text uses a length prefix at element zero.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid scene text length.
#     Scene text contains NUL.
fn se_text_unhex(bytes :: Array<Int64>, text :: String) -> Bool {
    length :: Int64 := text.size();
    count :: Int64 := length / 2;
    if length % 2 != 0 || count >= bytes.size() {
        throw "Invalid scene text length";
    }
    i :: Int64 := 0;
    while i < count {
        value :: Int64 := se_hex_digit(text[i * 2]) * 16 + se_hex_digit(text[i * 2 + 1]);
        if value == 0 {
            throw "Scene text contains NUL";
        }
        bytes[i + 1] = value;
        i = i + 1;
    }
    bytes[0] = count;
    return true;
}
