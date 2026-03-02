# Copyright f3rhd (C) 2026-2026
ruleset warnings;
module vmem;

const SUCCESSFULL_LEX   :: Int64 = 1;
const UNSUCCESSFULL_LEX :: Int64 = -1;

group token_types {
    lp      ::  Int64 = 2;
    rp      ::  Int64 = 4;
    literal ::  Int64 = 8;
    plus    ::  Int64 = 16;
    minus   ::  Int64 = 32;
    slash   ::  Int64 = 64;
    star    ::  Int64 = 128;
};

interface token_ifc {
    kind  :: Int64,
    value :: String
}

fn lexing_helper_str_is_digit(char :: String) -> Int64 {
    if char == "0" { return 1; }
    if char == "1" { return 1; }
    if char == "2" { return 1; }
    if char == "3" { return 1; }
    if char == "4" { return 1; }
    if char == "5" { return 1; }
    if char == "6" { return 1; }
    if char == "7" { return 1; }
    if char == "8" { return 1; }
    if char == "9" { return 1; }
    return -1;
}

fn lexing_lex(source_ptr :: Int64) -> Array {
    source :: String = vmem.peek(source_ptr);  
    tokens :: Array  = [];
    current_index :: Int64 = 0;
    source_length :: Int64 = source.size();

    while current_index < source_length {
        current_char :: String = source[current_index];

        if current_char == " " || current_char == "\n" || current_char == "\t" {
            current_index++;
            continue;
        }

        if current_char == "(" {
            tokens.push(token_ifc(token_types.lp, "("));
            current_index++;
            continue;
        }
        if current_char == ")" {
            tokens.push(token_ifc(token_types.rp, ")"));
            current_index++;
            continue;
        }
        if current_char == "+" {
            tokens.push(token_ifc(token_types.plus, "+"));
            current_index++;
            continue;
        }
        if current_char == "-" {
            tokens.push(token_ifc(token_types.minus, "-"));
            current_index++;
            continue;
        }
        if current_char == "*" {
            tokens.push(token_ifc(token_types.star, "*"));
            current_index++;
            continue;
        }
        if current_char == "/" {
            tokens.push(token_ifc(token_types.slash, "/"));
            current_index++;
            continue;
        }

        if lexing_helper_str_is_digit(current_char) == 1 {
            num_buf :: String = ""; 
            while current_index < source_length && lexing_helper_str_is_digit(source[current_index]) == 1 {
                num_buf = num_buf + source[current_index];
                current_index++;
            }
            tokens.push(token_ifc(token_types.literal, num_buf));
            continue;
        }

        out("Got unexpected character: " + current_char);
        current_index++;
    }
    return tokens;
}

fn main() -> Int64 {
    source :: String = "(2 * 3) + 40";
    tokens :: Array  = lexing_lex($source); 
    
    through t :: tokens -> loop {
        out("Token Value: " + t.value);
    };
    return 1;
}

main();