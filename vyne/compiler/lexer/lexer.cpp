#include "lexer.h"

std::vector<Token> tokenize(const std::string& input) {
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

            while (i < input.length() && input[i] != '"') {
                if (input[i] == '\n') currentLine++;
                if (input[i] == '\\' && i + 1 < input.length()) {
                    if (input[i+1] == 'n') {
                        strBuffer += '\n';
                        i += 2;
                        continue;
                    } else if (input[i+1] == 't') {
                        strBuffer += '\t';
                        i += 2;
                        continue;
                    } else if (input[i+1] == '\"') {
                        strBuffer += '\"';
                        i += 2;
                        continue;
                    }
                }
                strBuffer += input[i];
                i++;
            }

            if (i < input.length()) {
                i++;
            }
            tokens.emplace_back(Token(VTokenType::String, currentLine, 0, strBuffer));
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
                tokens.emplace_back(VTokenType::Float64, currentLine, std::stod(buffer), "");
            } else {
                tokens.emplace_back(VTokenType::Int64, currentLine, static_cast<double>(std::stoll(buffer)), "");
            }
            continue;
        }

        if (std::isalpha(character) || character == '_') {
            std::string buffer;
            while (i < input.length() && (std::isalnum(input[i]) || input[i] == '_')) {
                buffer += input[i++];
            }

            if (buffer == "out") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "sizeof") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "type") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "string") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "int64") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "float64") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "sequence") tokens.emplace_back(VTokenType::BuiltIn, currentLine, 0, buffer);
            else if (buffer == "group") tokens.emplace_back(VTokenType::Group, currentLine, 0, "");
            else if (buffer == "true") tokens.emplace_back(VTokenType::True, currentLine, 1, "");
            else if (buffer == "false") tokens.emplace_back(VTokenType::False, currentLine, 0, "");
            else if (buffer == "null") tokens.emplace_back(VTokenType::Null, currentLine, 0, "");
            else if (buffer == "sub") tokens.emplace_back(VTokenType::Function, currentLine, 0, buffer);
            else if (buffer == "return") tokens.emplace_back(VTokenType::Return, currentLine, 0, buffer);
            else if (buffer == "while") tokens.emplace_back(VTokenType::While, currentLine, 0, buffer);
            else if (buffer == "through") tokens.emplace_back(VTokenType::Through, currentLine, 0, buffer);
            else if (buffer == "loop") tokens.emplace_back(VTokenType::LoopMode, currentLine, 0, buffer);
            else if (buffer == "collect") tokens.emplace_back(VTokenType::LoopMode, currentLine, 0, buffer);
            else if (buffer == "unique") tokens.emplace_back(VTokenType::LoopMode, currentLine, 0, buffer);
            else if (buffer == "every") tokens.emplace_back(VTokenType::LoopMode, currentLine, 0, buffer);
            else if (buffer == "filter") tokens.emplace_back(VTokenType::LoopMode, currentLine, 0, buffer);
            else if (buffer == "break") tokens.emplace_back(VTokenType::Break, currentLine, 0, buffer);
            else if (buffer == "continue") tokens.emplace_back(VTokenType::Continue, currentLine, 0, buffer);
            else if (buffer == "module") tokens.emplace_back(VTokenType::Module, currentLine, 0, buffer);
            else if (buffer == "dismiss") tokens.emplace_back(VTokenType::Dismiss, currentLine, 0, buffer);
            else if (buffer == "if") tokens.emplace_back(VTokenType::If, currentLine, 0, buffer);
            else if (buffer == "else") tokens.emplace_back(VTokenType::Else, currentLine, 0, buffer);
            else if (buffer == "const") tokens.emplace_back(VTokenType::Const, currentLine, 0, buffer);
            else if (buffer == "use") tokens.emplace_back(VTokenType::Use, currentLine, 0, buffer);
            else if (buffer == "deploy") tokens.emplace_back(VTokenType::Deploy, currentLine, 0, buffer);
            else if (buffer == "as") tokens.emplace_back(VTokenType::As, currentLine, 0, buffer);
            else if (buffer == "extern") tokens.emplace_back(VTokenType::Extern, currentLine, 0, buffer);
            else tokens.emplace_back(VTokenType::Identifier, currentLine, 0, buffer);
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
                    i++;
                }
                break;
            }
            case '&' : {
                if(i + 1 < input.length() && input[i + 1] == '&'){
                    tokens.emplace_back(VTokenType::And, currentLine, 0, "&&");
                    i++;
                }
                break;
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
