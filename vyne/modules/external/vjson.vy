module vjson;

# IMPORTANT NOTE : THIS LIBRARY INCLUDES
# A SET OF FEATURES THAT VYNE CURRENTLY DOES NOT SUPPORT
# ALL IMPLEMENTED FEATURES WILL BE RELEASED IN THE FUTURE

# ----- Data Structures -----

# Represents a JSON value (can be null, boolean, number, string, array, object)
interface JsonValue {
    type: String;           # "null", "boolean", "number", "string", "array", "object", "error"
    bool_value: Bool;
    num_value: Float64;
    str_value: String;
    array_value: sequence;  # sequence of JsonValue
    obj_value: sequence;    # sequence of JsonPair
}

# Key-value pair for JSON objects
interface JsonPair {
    key: String;
    value: JsonValue;
}

# Internal parser state
interface JsonParser {
    input: String;
    pos: Int64;
    len: Int64;
}

# ----- Factory Functions for JsonValue -----

fn null() -> JsonValue {
    val = JsonValue();
    val.type = "null";
    return val;
}

fn boolean(b: bool) -> JsonValue {
    val = JsonValue();
    val.type = "boolean";
    val.bool_value = b;
    return val;
}

fn number(n: float64) -> JsonValue {
    val = JsonValue();
    val.type = "number";
    val.num_value = n;
    return val;
}

fn string(s: string) -> JsonValue {
    val = JsonValue();
    val.type = "string";
    val.str_value = s;
    return val;
}

fn array(arr: sequence) -> JsonValue {
    # Verify all elements are JsonValue? Not enforced, but we trust caller.
    val = JsonValue();
    val.type = "array";
    val.array_value = arr;
    return val;
}

fn obj(pairs: sequence) -> JsonValue {
    val = JsonValue();
    val.type = "object";
    val.obj_value = pairs;
    return val;
}

fn errorJson(msg: string) -> JsonValue {
    val = JsonValue();
    val.type = "error";
    val.str_value = msg;
    return val;
}

# ----- Character Helpers -----

fn isWhitespace(ch: string) -> bool {
    return ch == " " or ch == "\t" or ch == "\n" or ch == "\r";
}

fn isDigit(ch: string) -> bool {
    return ch >= "0" and ch <= "9";
}

fn isAlphaNum(ch: string) -> bool {
    return (ch >= "a" and ch <= "z") or (ch >= "A" and ch <= "Z") or (ch >= "0" and ch <= "9") or ch == "_";
}

# ----- Parser Methods (callable as parser.method()) -----

fn initParser(parser: JsonParser, input: string) {
    parser.input = input;
    parser.pos = 0;
    parser.len = sizeof(input);
}

fn peek(parser: JsonParser) -> string {
    if parser.pos < parser.len {
        return parser.input[parser.pos];
    }
    return "\0";  # EOF
}

fn next(parser: JsonParser) {
    if parser.pos < parser.len {
        parser.pos++;
    }
}

fn skipWhitespace(parser: JsonParser) {
    while parser.pos < parser.len and isWhitespace(parser.peek()) {
        parser.next();
    }
}

fn matchKeyword(parser: JsonParser, keyword: string) -> bool {
    i = 0;
    while i < sizeof(keyword) {
        if parser.pos + i >= parser.len or parser.input[parser.pos + i] != keyword[i] {
            return false;
        }
        i++;
    }
    # Ensure next char is not part of identifier (to avoid "true" matching "truefalse")
    if parser.pos + i < parser.len and isAlphaNum(parser.input[parser.pos + i]) {
        return false;
    }
    parser.pos += i;
    return true;
}

fn parseString(parser: JsonParser) -> JsonValue {
    # Assumes current char is '"'
    parser.next();  # skip opening quote
    start = parser.pos;
    result = "";
    escape = false;
    while parser.pos < parser.len {
        ch = parser.peek();
        if escape {
            if ch == '"' or ch == '\\' or ch == '/' {
                result += ch;
            } else if ch == 'b' {
                result += '\b';
            } else if ch == 'f' {
                result += '\f';
            } else if ch == 'n' {
                result += '\n';
            } else if ch == 'r' {
                result += '\r';
            } else if ch == 't' {
                result += '\t';
            } else if ch == 'u' {
                # Unicode escape - simplified: just consume 4 hex digits and ignore
                # In real implementation, we'd convert to UTF-8
                result += "?";
                parser.next(); parser.next(); parser.next(); parser.next();
            } else {
                return errorJson("Invalid escape sequence at position " + string(parser.pos));
            }
            escape = false;
            parser.next();
        } else {
            if ch == '"' {
                parser.next();  # skip closing quote
                return string(result);
            } else if ch == '\\' {
                escape = true;
                parser.next();
            } else {
                result += ch;
                parser.next();
            }
        }
    }
    return errorJson("Unterminated string");
}

fn parseNumber(parser: JsonParser) -> JsonValue {
    start = parser.pos;
    # allow leading minus
    if parser.peek() == '-' {
        parser.next();
    }
    # integer part
    if parser.peek() == '0' {
        parser.next();
    } else if isDigit(parser.peek()) {
        while parser.pos < parser.len and isDigit(parser.peek()) {
            parser.next();
        }
    } else {
        return errorJson("Invalid number at position " + string(parser.pos));
    }
    # fractional part
    if parser.pos < parser.len and parser.peek() == '.' {
        parser.next();
        if not isDigit(parser.peek()) {
            return errorJson("Expected digits after decimal point");
        }
        while parser.pos < parser.len and isDigit(parser.peek()) {
            parser.next();
        }
    }
    # exponent
    if parser.pos < parser.len and (parser.peek() == 'e' or parser.peek() == 'E') {
        parser.next();
        if parser.peek() == '+' or parser.peek() == '-' {
            parser.next();
        }
        if not isDigit(parser.peek()) {
            return errorJson("Expected digits after exponent");
        }
        while parser.pos < parser.len and isDigit(parser.peek()) {
            parser.next();
        }
    }
    numStr = parser.input[start .. parser.pos];  # substring (assuming range operator works)
    # Convert to float64
    val = float64(numStr);  # built-in conversion
    return number(val);
}

fn parseArray(parser: JsonParser) -> JsonValue {
    parser.next();  # skip '['
    arr = [];
    while true {
        parser.skipWhitespace();
        if parser.peek() == ']' {
            parser.next();
            break;
        }
        elem = parser.parseValue();
        if elem.type == "error" {
            return elem;
        }
        arr.append(elem);  # sequence append (built-in?)
        parser.skipWhitespace();
        if parser.peek() == ',' {
            parser.next();
            # allow trailing comma? JSON doesn't, but we can be lenient
        } else if parser.peek() != ']' {
            return errorJson("Expected ',' or ']' in array");
        }
    }
    return array(arr);
}

fn parseObject(parser: JsonParser) -> JsonValue {
    parser.next();  # skip '{'
    pairs = [];
    while true {
        parser.skipWhitespace();
        if parser.peek() == '}' {
            parser.next();
            break;
        }
        # parse key (must be string)
        if parser.peek() != '"' {
            return errorJson("Expected string key in object");
        }
        keyVal = parser.parseString();
        if keyVal.type == "error" {
            return keyVal;
        }
        key = keyVal.str_value;
        parser.skipWhitespace();
        if parser.peek() != ':' {
            return errorJson("Expected ':' after key in object");
        }
        parser.next();  # skip ':'
        valueVal = parser.parseValue();
        if valueVal.type == "error" {
            return valueVal;
        }
        pair = JsonPair();
        pair.key = key;
        pair.value = valueVal;
        pairs.append(pair);
        parser.skipWhitespace();
        if parser.peek() == ',' {
            parser.next();
        } else if parser.peek() != '}' {
            return errorJson("Expected ',' or '}' in object");
        }
    }
    return obj(pairs);
}

fn parseValue(parser: JsonParser) -> JsonValue {
    parser.skipWhitespace();
    ch = parser.peek();
    if ch == '{' {
        return parser.parseObject();
    } else if ch == '[' {
        return parser.parseArray();
    } else if ch == '"' {
        return parser.parseString();
    } else if ch == '-' or isDigit(ch) {
        return parser.parseNumber();
    } else if parser.matchKeyword("true") {
        return boolean(true);
    } else if parser.matchKeyword("false") {
        return boolean(false);
    } else if parser.matchKeyword("null") {
        return null();
    } else {
        return errorJson("Unexpected character '" + ch + "' at position " + string(parser.pos));
    }
}

# ----- Public API -----

fn parse(input: string) -> JsonValue {
    parser = JsonParser();
    parser.initParser(input);
    result = parser.parseValue();
    parser.skipWhitespace();
    if parser.pos != parser.len {
        return errorJson("Extra characters after JSON value");
    }
    return result;
}

# ----- Stringify Helpers -----

fn stringifyInternal(value: JsonValue, indentLevel: int64, indentSpaces: int64) -> string {
    if value.type == "null" {
        return "null";
    } else if value.type == "boolean" {
        if value.bool_value {
            return "true";
        } else {
            return "false";
        }
    } else if value.type == "number" {
        # Convert float64 to string; built-in string() should handle
        return string(value.num_value);
    } else if value.type == "string" {
        # Escape special characters
        s = "\"";
        i = 0;
        while i < sizeof(value.str_value) {
            ch = value.str_value[i];
            if ch == '"' {
                s += "\\\"";
            } else if ch == '\\' {
                s += "\\\\";
            } else if ch == '\b' {
                s += "\\b";
            } else if ch == '\f' {
                s += "\\f";
            } else if ch == '\n' {
                s += "\\n";
            } else if ch == '\r' {
                s += "\\r";
            } else if ch == '\t' {
                s += "\\t";
            } else {
                s += ch;
            }
            i++;
        }
        s += "\"";
        return s;
    } else if value.type == "array" {
        if sizeof(value.array_value) == 0 {
            return "[]";
        }
        indent = indentSpaces > 0;
        newline = indent ? "\n" : "";
        spaces = "";
        if indent {
            i = 0;
            while i < indentLevel * indentSpaces {
                spaces += " ";
                i++;
            }
        }
        result = "[" + newline;
        i = 0;
        while i < sizeof(value.array_value) {
            elem = value.array_value[i];
            result += spaces;
            if indent {
                j = 0;
                while j < indentSpaces {
                    result += " ";
                    j++;
                }
            }
            result += stringifyInternal(elem, indentLevel + 1, indentSpaces);
            if i < sizeof(value.array_value) - 1 {
                result += "," + newline;
            }
            i++;
        }
        result += newline + spaces + "]";
        return result;
    } else if value.type == "object" {
        if sizeof(value.obj_value) == 0 {
            return "{}";
        }
        indent = indentSpaces > 0;
        newline = indent ? "\n" : "";
        spaces = "";
        if indent {
            i = 0;
            while i < indentLevel * indentSpaces {
                spaces += " ";
                i++;
            }
        }
        result = "{" + newline;
        i = 0;
        while i < sizeof(value.obj_value) {
            pair = value.obj_value[i];
            result += spaces;
            if indent {
                j = 0;
                while j < indentSpaces {
                    result += " ";
                    j++;
                }
            }
            result += "\"" + pair.key + "\":";
            if indent {
                result += " ";
            }
            result += stringifyInternal(pair.value, indentLevel + 1, indentSpaces);
            if i < sizeof(value.obj_value) - 1 {
                result += "," + newline;
            }
            i++;
        }
        result += newline + spaces + "}";
        return result;
    } else if value.type == "error" {
        return "<error: " + value.str_value + ">";
    }
    return "<unknown>";
}

fn stringify(value: JsonValue, indent: int64 = 0) -> string {
    return stringifyInternal(value, 0, indent);
}

# Example usage (commented out)
# input = '{"name": "John", "age": 30, "cars": ["Ford", "BMW"]}';
# val = parse(input);
# if val.type != "error" {
#     out(val);
#     out(stringify(val, 2));
# }