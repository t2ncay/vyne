use "world.vy";

# Map a decimal digit value to its textual glyph for exact numeric serialization.
#
# Parameters:
#     digit (Int64): Decimal digit value, from 0 through 9.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_world_decimal_digit(digit :: Int64) -> String {
    if digit == 0 {
        return "0";
    }
    if digit == 1 {
        return "1";
    }
    if digit == 2 {
        return "2";
    }
    if digit == 3 {
        return "3";
    }
    if digit == 4 {
        return "4";
    }
    if digit == 5 {
        return "5";
    }
    if digit == 6 {
        return "6";
    }
    if digit == 7 {
        return "7";
    }
    if digit == 8 {
        return "8";
    }
    return "9";
}

# Runtime string(Float64) is %g (six digits). Expand the exact binary rational
# with base-10^7 integer limbs in preallocated world scratch, then round to 17
# significant decimal digits (ties to even). This roundtrips every finite bounded
# Float64, including subnormals; no native binding or per-number array is needed.

# Format a Float64 with roundtrip-safe decimal precision using the world's reusable scratch storage.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_world_decimal(world :: SEWorld, value :: Float64) -> String {
    return se_world_decimal_work(world.work, value);
}

# Format a Float64 with roundtrip-safe decimal precision using caller-provided numeric scratch.
#
# Parameters:
#     buffer (Array): Caller-provided fixed numeric buffer for matrix or serialization scratch.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     World export requires finite numbers within 2000000.
#     World decimal scratch exceeded.
fn se_world_decimal_work(buffer :: Array, value :: Float64) -> String {
    # Valid cube corners and terrain pad coordinates can extend past a 1e6 center.
    if !(vmath.abs(value) <= 2000000.0) {
        throw "World export requires finite numbers within 2000000";
    }
    if value == vmath.floor(value) {
        return string(int64(value)) + ".0";
    }
    negative :: Bool := value < 0.0;
    normalized :: Float64 := vmath.abs(value);
    exponent :: Int64 := 0;
    while normalized < 1.0 {
        normalized = normalized * 2.0;
        exponent = exponent - 1;
    }
    while normalized >= 2.0 {
        normalized = normalized * 0.5;
        exponent = exponent + 1;
    }
    mantissa :: Int64 := int64(normalized * 4503599627370496.0);
    power :: Int64 := exponent - 52;
    work :: Array<Float64> := buffer;
    length :: Int64 := 0;
    while mantissa > 0 {
        work[length] = float64(mantissa % 10000000);
        mantissa = mantissa / 10000000;
        length = length + 1;
    }
    factor :: Int64 := 2;
    iterations :: Int64 := power;
    scale :: Int64 := 0;
    if power < 0 {
        factor = 5;
        iterations = -power;
        scale = iterations;
    }
    iteration :: Int64 := 0;
    while iteration < iterations {
        carry :: Int64 := 0;
        limb :: Int64 := 0;
        while limb < length {
            product :: Int64 := int64(work[limb]) * factor + carry;
            work[limb] = float64(product % 10000000);
            carry = product / 10000000;
            limb = limb + 1;
        }
        if carry > 0 {
            if length == 160 {
                throw "World decimal scratch exceeded";
            }
            work[length] = float64(carry);
            length = length + 1;
        }
        iteration = iteration + 1;
    }
    builder :: Int64 := vcore.sb_create();
    vcore.sb_append(builder, string(int64(work[length - 1])));
    limb :: Int64 := length - 2;
    while limb >= 0 {
        divisor :: Int64 := 1000000;
        integer :: Int64 := int64(work[limb]);
        while divisor >= 1 {
            vcore.sb_append(builder, se_world_decimal_digit((integer / divisor) % 10));
            divisor = divisor / 10;
        }
        limb = limb - 1;
    }
    exact :: String := vcore.sb_build(builder);
    decimal_exponent :: Int64 := exact.size() - 1 - scale;
    significand :: Int64 := 0;
    digit :: Int64 := 0;
    while digit < 17 {
        significand = significand * 10;
        if digit < exact.size() {
            significand = significand + int64(exact[digit]);
        }
        digit = digit + 1;
    }
    if exact.size() > 17 {
        next_digit :: Int64 := int64(exact[17]);
        tail :: Bool := false;
        digit = 18;
        while digit < exact.size() {
            if exact[digit] != "0" {
                tail = true;
            }
            digit = digit + 1;
        }
        if next_digit > 5 || (next_digit == 5 && (tail || significand % 2 == 1)) {
            significand = significand + 1;
        }
    }
    if significand == 100000000000000000 {
        significand = 10000000000000000;
        decimal_exponent = decimal_exponent + 1;
    }
    digits :: String := string(significand);
    builder = vcore.sb_create();
    if negative {
        vcore.sb_append(builder, "-");
    }
    vcore.sb_append(builder, digits[0]);
    vcore.sb_append(builder, ".");
    digit = 1;
    while digit < 17 {
        vcore.sb_append(builder, digits[digit]);
        digit = digit + 1;
    }
    vcore.sb_append(builder, "e" + string(decimal_exponent));
    return vcore.sb_build(builder);
}

# Parse a finite bounded world coordinate from persisted decimal/scientific text.
#
# Parameters:
#     raw (String): Persisted decimal/scientific numeric text to parse.
#
# Returns:
#     The computed Float64 value: value.
#
# Errors:
#     Empty world coordinate.
#     Invalid world coordinate.
#     Invalid world coordinate exponent.
#     Invalid world coordinate suffix.
#     World coordinate exceeds 2000000 or is not finite.
fn se_world_coordinate(raw :: String) -> Float64 {
    text :: String := vmge_trim(raw);
    i :: Int64 := 0;
    digits :: Int64 := 0;
    if text.size() == 0 {
        throw "Empty world coordinate";
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
        throw "Invalid world coordinate";
    }
    if i < text.size() && (text[i] == "e" || text[i] == "E") {
        i = i + 1;
        if i < text.size() && (text[i] == "+" || text[i] == "-") {
            i = i + 1;
        }
        digits = 0;
        while i < text.size() && vmge_digit(text[i]) {
            digits = digits + 1;
            i = i + 1;
        }
        if digits == 0 {
            throw "Invalid world coordinate exponent";
        }
    }
    if i != text.size() {
        throw "Invalid world coordinate suffix";
    }
    value :: Float64 := float64(text);
    if !(vmath.abs(value) <= 2000000.0) {
        throw "World coordinate exceeds 2000000 or is not finite";
    }
    return value;
}
