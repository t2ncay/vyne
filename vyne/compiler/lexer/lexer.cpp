#include "lexer.h"

std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> tokens;
    size_t i = 0;
    int currentLine = 1;

    while (i < input.length()) {
        char character = input[i];

        if (std::isspace(character)) {
            if (character == '\n') {
                currentLine++;
            }
            i++;
            continue;
        }

        if (character == '"') {
            i++;
            std::string strBuffer;
            std::vector<std::pair<std::string, bool>> parts; // bool = is_interpolation
            bool isInterpolated = false;
            bool inInterpolation = false;
            std::string exprBuffer;
            
            while (i < input.length() && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < input.length()) {
                    char next = input[i+1];
                    if (next == 'n') { strBuffer += '\n'; i += 2; continue; }
                    else if (next == 't') { strBuffer += '\t'; i += 2; continue; }
                    else if (next == '\"') { strBuffer += '\"'; i += 2; continue; }
                    else if (next == '{') { strBuffer += '{'; i += 2; continue; }
                    else if (next == '}') { strBuffer += '}'; i += 2; continue; }
                }
                
                if (input[i] == '{' && i + 1 < input.length() && input[i+1] != '{') {
                    // Flush current string part
                    if (!strBuffer.empty()) {
                        parts.emplace_back(strBuffer, false);
                        strBuffer.clear();
                    }
                    isInterpolated = true;
                    inInterpolation = true;
                    i++; // Skip '{'
                    continue;
                }
                
                if (input[i] == '}' && inInterpolation) {
                    // End of interpolation
                    if (!exprBuffer.empty()) {
                        parts.emplace_back(exprBuffer, true);
                        exprBuffer.clear();
                    }
                    inInterpolation = false;
                    i++;
                    continue;
                }
                
                if (inInterpolation) {
                    exprBuffer += input[i];
                } else {
                    strBuffer += input[i];
                }
                i++;
            }
            
            if (!strBuffer.empty()) {
                parts.emplace_back(strBuffer, false);
            }
            
            if (i < input.length()) i++; // Skip closing "
            
            // If not interpolated, just return normal string
            if (!isInterpolated) {
                tokens.emplace_back(VTokenType::String, currentLine, strBuffer, "");
            } else {
                // Create InterpolatedString token with parts
                tokens.emplace_back(VTokenType::InterpolatedString, currentLine, parts, "");
            }
            continue;
        }

        if (std::isdigit(character)) {
            std::string buffer;
            bool isFloatingPoint = false;

            while (i < input.length()) {
                if (std::isdigit(input[i])) {
                    buffer += input[i++];
                } else if (input[i] == '.') {
                    if (i + 1 < input.length() && input[i + 1] == '.') {
                        break;
                    }
                    if (isFloatingPoint) break; 
                    
                    isFloatingPoint = true;
                    buffer += input[i++];
                } else {
                    break;
                }
            }

            if (isFloatingPoint) {
                double val;
                auto [ptr, ec] = std::from_chars(buffer.data(), buffer.data() + buffer.size(), val);
                if (ec == std::errc{}) {
                    tokens.emplace_back(VTokenType::Float64, currentLine, val, "");
                }
            } else {
                long long val;
                auto [ptr, ec] = std::from_chars(buffer.data(), buffer.data() + buffer.size(), val);
                if (ec == std::errc{}) {
                    tokens.emplace_back(VTokenType::Int64, currentLine, static_cast<int64_t>(val), "");
                }
            }
            continue;
        }

        if (std::isalpha(character) || character == '_') {
            size_t start = i;
            while (i < input.length() && (std::isalnum(input[i]) || input[i] == '_')) {
                i++;
            }

            std::string_view buffer = input.substr(start, i - start);

            auto it = keywords.find(buffer);
            if (it != keywords.end()) {
                tokens.emplace_back(it->second, currentLine, 0, buffer);
            } else {
                tokens.emplace_back(VTokenType::Identifier, currentLine, 0, buffer);
            }
            continue;
        }

        switch (character) {
            case '(': tokens.emplace_back(VTokenType::Left_Parenthese, currentLine, 0, "("); break;
            case ')': tokens.emplace_back(VTokenType::Right_Parenthese, currentLine, 0, ")"); break;
            case '{': tokens.emplace_back(VTokenType::Left_CB, currentLine, 0, "{"); break;
            case '}': tokens.emplace_back(VTokenType::Right_CB, currentLine, 0, "}"); break;
            case '[': tokens.emplace_back(VTokenType::Left_Bracket, currentLine, 0, "["); break;
            case ']': tokens.emplace_back(VTokenType::Right_Bracket, currentLine, 0, "]"); break;
            case ',': tokens.emplace_back(VTokenType::Comma, currentLine, 0, ","); break;
            case ';': tokens.emplace_back(VTokenType::Semicolon, currentLine, 0, ";"); break;
            case '%': tokens.emplace_back(VTokenType::Modulo, currentLine, 0, "%"); break;
            case '$': tokens.emplace_back(VTokenType::Addresser, currentLine, 0, "$"); break;
            case '?': tokens.emplace_back(VTokenType::Question, currentLine, 0, "?"); break;
            case '/': {
                if (i + 1 < input.length() && input[i + 1] == '/') {
                    tokens.emplace_back(VTokenType::Floor_Divide, currentLine, 0, "//");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Division, currentLine, 0, "/");
                }
                break;
            }
            case '.': {
                if (i + 1 < input.length() && input[i + 1] == '.') {
                    tokens.emplace_back(VTokenType::Double_Dot, currentLine, 0, "..");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Dot, currentLine, 0, ".");
                }
                break;
            }
            case '*': {
                if (i + 1 < input.length() && input[i + 1] == '*') {
                    tokens.emplace_back(VTokenType::Power, currentLine, 0, "**");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Multiply, currentLine, 0, "*");
                }
                break;
            }
            case '+': {
                if (i + 1 < input.length() && input[i + 1] == '+') {
                    tokens.emplace_back(VTokenType::Double_Increment, currentLine, 0, "++");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Add, currentLine, 0, "+");
                }
                break;
            }
            case '<': {
                if (i + 1 < input.length() && input[i + 1] == '=') {
                    tokens.emplace_back(VTokenType::Smaller_Or_Equal, currentLine, 0, "<=");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Smaller, currentLine, 0, "<");
                }
                break;
            }
            case '>': {
                if (i + 1 < input.length() && input[i + 1] == '=') {
                    tokens.emplace_back(VTokenType::Greater_Or_Equal, currentLine, 0, ">=");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Greater, currentLine, 0, ">");
                }
                break;
            }
            case '=': {
                if (i + 1 < input.length() && input[i + 1] == '=') {
                    tokens.emplace_back(VTokenType::Double_Equals, currentLine, 0, "==");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Equals, currentLine, 0, "=");
                }
                break;
            }
            case '!' : {
                if (i + 1 < input.length() && input[i + 1] == '=') {
                    tokens.emplace_back(VTokenType::Not_Equal, currentLine, 0, "!=");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Exclamatory, currentLine, 0, "!");
                }
                break;
            }
            case '#': {
                while (i < input.length() && input[i] != '\n') {
                    i++;
                }
                i--; 
                break;  
            }
            case ':' : {
                if(i + 1 < input.length() && input[i + 1] == ':'){
                    tokens.emplace_back(VTokenType::Extends, currentLine, 0, "::");
                    i += 2;
                } else {
                    tokens.emplace_back(VTokenType::Colon, currentLine, 0, ":");
                    i++;
                }
                continue;
            }
            case '&' : {
                if(i + 1 < input.length() && input[i + 1] == '&'){
                    tokens.emplace_back(VTokenType::And, currentLine, 0, "&&");
                    i += 2;
                } else {
                    tokens.emplace_back(VTokenType::Referencer, currentLine, 0, "&");
                    i++;
                }
                continue;
            }
            case '|' : {
                if(i + 1 < input.length() && input[i + 1] == '|'){
                    tokens.emplace_back(VTokenType::Or, currentLine, 0, "||");
                    i++;
                } else if(i + 1 < input.length() && input[i + 1] == '>'){
                    tokens.emplace_back(VTokenType::Pipeline, currentLine, 0, "|>");
                    i++;
                }
                break;
            }
            case '-' : {
                if(i + 1 < input.length() && input[i + 1] == '>'){
                    tokens.emplace_back(VTokenType::Arrow, currentLine, 0, "->");
                    i++;
                } else if(i + 1 < input.length() && input[i + 1] == '-'){
                    tokens.emplace_back(VTokenType::Double_Decrement, currentLine, 0, "--");
                    i++;
                } else {
                    tokens.emplace_back(VTokenType::Substract, currentLine, 0, "-");
                }
                break;
            }
            default:
                std::cerr << "Unexpected character: " << character << std::endl;
                break;
        }
        i++;
    }

    tokens.emplace_back(VTokenType::End, currentLine, 0, "");
    return tokens;
}
