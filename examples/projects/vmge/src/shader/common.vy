use "../scene/text.vy";
use native vfs;
use native vmath;

fn ss_ints(n :: Int64) -> Array<Int64> { a :: Array<Int64> := []; a.reserve(n); a.resize(n,0); return a; }
fn ss_floats(n :: Int64) -> Array<Float64> { a :: Array<Float64> := []; a.reserve(n); a.resize(n,0.0); return a; }
fn ss_text(capacity :: Int64) -> Array<Int64> { return ss_ints(capacity+1); }
fn ss_utf8_validate(bytes :: Array<Int64>) -> Bool {
    i :: Int64 := 1; while i <= bytes[0] { lead :: Int64 := bytes[i]; width :: Int64 := 1; code :: Int64 := lead;
        if lead < 1 || lead > 244 { throw "Invalid shader UTF8 byte"; }
        if lead >= 128 { if lead >= 194 && lead <= 223 { width = 2; code = lead-192; } else { if lead >= 224 && lead <= 239 { width = 3; code = lead-224; } else { if lead >= 240 { width = 4; code = lead-240; } else { throw "Invalid shader UTF8 lead"; } } }
            if i+width-1 > bytes[0] { throw "Truncated shader UTF8"; } j :: Int64 := 1; while j < width { next :: Int64 := bytes[i+j]; if next < 128 || next > 191 { throw "Invalid shader UTF8 continuation"; } code = code*64+next-128; j = j+1; }
            if (width == 2 && code < 128) || (width == 3 && code < 2048) || (width == 4 && code < 65536) || (code >= 55296 && code <= 57343) || code > 1114111 { throw "Invalid/overlong shader UTF8 codepoint"; }
        } i = i+width;
    } return true;
}
fn ss_unhex(bytes :: Array<Int64>,text :: String) -> Bool { se_text_unhex(bytes,ss_trim(text)); return ss_utf8_validate(bytes); }
fn ss_split(text :: String, separator :: String) -> Array {
    parts :: Array := []; builder :: Int64 := vcore.sb_create(); i :: Int64 := 0;
    while i < text.size() { if text[i] == separator { parts.push(vcore.sb_build(builder)); builder = vcore.sb_create(); } else { vcore.sb_append(builder,text[i]); } i = i+1; }
    parts.push(vcore.sb_build(builder)); return parts;
}
fn ss_trim(text :: String) -> String {
    a :: Int64 := 0; b :: Int64 := text.size();
    while a < b && (text[a] == " " || text[a] == "\t" || text[a] == "\r") { a = a+1; }
    while b > a && (text[b-1] == " " || text[b-1] == "\t" || text[b-1] == "\r") { b = b-1; }
    builder :: Int64 := vcore.sb_create(); while a < b { vcore.sb_append(builder,text[a]); a = a+1; } return vcore.sb_build(builder);
}
fn ss_digit(ch :: String) -> Bool { return ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4" || ch == "5" || ch == "6" || ch == "7" || ch == "8" || ch == "9"; }
fn ss_number(text :: String) -> Float64 {
    encoded :: Array := ss_split(text,"@"); if encoded.size() == 2 { mantissa :: Int64 := ss_integer(encoded[0]); power :: Int64 := ss_integer(encoded[1]); if power < -1074 || power > 1024 { throw "Invalid binary exponent"; } value :: Float64 := float64(mantissa); while power < 0 { value = value*0.5; power = power+1; } while power > 0 { value = value*2.0; power = power-1; } if !(vmath.abs(value) <= 2147483648.0) { throw "Shader number must be finite and bounded"; } return value; } if encoded.size() != 1 { throw "Invalid binary number"; }
    i :: Int64 := 0; digits :: Int64 := 0; if text.size() == 0 { throw "Empty shader number"; }
    if text[0] == "-" || text[0] == "+" { i = i+1; }
    while i < text.size() && ss_digit(text[i]) { digits = digits+1; i = i+1; }
    if i < text.size() && text[i] == "." { i = i+1; while i < text.size() && ss_digit(text[i]) { digits = digits+1; i = i+1; } }
    if digits == 0 { throw "Invalid shader number"; }
    if i < text.size() && (text[i] == "e" || text[i] == "E") { i = i+1; if i < text.size() && (text[i] == "-" || text[i] == "+") { i = i+1; } digits = 0; while i < text.size() && ss_digit(text[i]) { digits = digits+1; i = i+1; } if digits == 0 { throw "Invalid exponent"; } }
    if i != text.size() { throw "Invalid shader number suffix"; }
    value :: Float64 := float64(text); if !(vmath.abs(value) <= 2147483648.0) { throw "Shader number must be finite and bounded"; } return value;
}
fn ss_integer(text :: String) -> Int64 {
    if text.size() == 0 { throw "Expected shader integer"; } i :: Int64 := 0; sign :: Int64 := 1; if text[0] == "-" { sign = -1; i = 1; } if i == text.size() { throw "Expected shader integer"; } n :: Int64 := 0;
    while i < text.size() { if !ss_digit(text[i]) || n > 900719925474099 { throw "Invalid/oversized shader integer"; } n = n*10+int64(text[i]); if n > 9007199254740991 { throw "Oversized shader integer"; } i = i+1; } return n*sign;
}
# Exact binary-rational spelling avoids runtime String(Float64)'s six-digit %g.
fn ss_encode(value :: Float64) -> String {
    if !(vmath.abs(value) <= 2147483648.0) { throw "Cannot encode nonfinite/bounded shader number"; } if value == 0.0 { return "0"; }
    n :: Float64 := vmath.abs(value); power :: Int64 := -52; while n < 1.0 { n = n*2.0; power = power-1; } while n >= 2.0 { n = n*0.5; power = power+1; }
    mantissa :: Int64 := int64(n*4503599627370496.0); if value < 0.0 { mantissa = -mantissa; } return string(mantissa)+"@"+string(power);
}
fn ss_glsl_number(value :: Float64) -> String { if value == 0.0 { return "0.0"; } parts :: Array := ss_split(ss_encode(value),"@"); power :: Int64 := ss_integer(parts[1])+52; return "(("+parts[0]+".0/4503599627370496.0)*exp2("+string(power)+".0))"; }
fn ss_hash(bytes :: Array<Int64>) -> Int64 { h :: Int64 := 17; i :: Int64 := 1; while i <= bytes[0] { h = (h*131+bytes[i])%2147483647; i = i+1; } return h; }
fn ss_parent(path :: String) -> String { end :: Int64 := -1; i :: Int64 := 0; while i < path.size() { if path[i] == "/" || path[i] == "\\" { end = i; } i = i+1; } b :: Int64 := vcore.sb_create(); i = 0; while i <= end { vcore.sb_append(b,path[i]); i = i+1; } return vcore.sb_build(b); }
fn ss_components(gltype :: Int64) -> Int64 {
    if gltype == 5126 || gltype == 5124 || gltype == 35670 || gltype == 35678 { return 1; }
    if gltype == 35664 || gltype == 35667 || gltype == 35671 { return 2; }
    if gltype == 35665 || gltype == 35668 || gltype == 35672 { return 3; }
    if gltype == 35666 || gltype == 35669 || gltype == 35673 || gltype == 35674 { return 4; }
    if gltype == 35675 { return 9; } if gltype == 35676 { return 16; } return 0;
}
fn ss_continuous(gltype :: Int64) -> Bool { return gltype == 5126 || (gltype >= 35664 && gltype <= 35666) || (gltype >= 35674 && gltype <= 35676); }
fn ss_upload_supported(gltype :: Int64) -> Bool { return gltype == 5126 || gltype == 5124 || (gltype >= 35664 && gltype <= 35673) || gltype == 35676 || gltype == 35678; }
fn ss_component_validate(gltype :: Int64,value :: Float64) -> Bool {
    if ss_components(gltype) == 0 || !(vmath.abs(value) <= 2147483648.0) { throw "Unknown/nonfinite shader component type/value"; }
    if gltype >= 35670 && gltype <= 35673 { if value != 0.0 && value != 1.0 { throw "Boolean shader component must be exactly 0 or 1"; } return true; }
    if !ss_continuous(gltype) { if value != vmath.floor(value) || value < -2147483648.0 || value > 2147483647.0 { throw "Integer shader component must be exact signed Int32"; } return true; }
    if !(vmath.abs(value) <= 1000000.0) { throw "Continuous shader component exceeds bounded range"; } return true;
}
fn ss_range_validate(gltype :: Int64,low :: Float64,high :: Float64) -> Bool { ss_component_validate(gltype,low); ss_component_validate(gltype,high); if low > high { throw "Invalid shader component range"; } return true; }
fn ss_identifier(name :: String) -> Bool {
    if name.size() == 0 || name.size() > 127 { return false; }
    alphabet :: String := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"; i :: Int64 := 0;
    while i < name.size() { found :: Bool := false; j :: Int64 := 0; while j < alphabet.size() { if name[i] == alphabet[j] { found = true; } j = j+1; } if i > 0 && ss_digit(name[i]) { found = true; } if !found { return false; } i = i+1; } return true;
}
