#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <cctype>

enum class VTokenType {
    // --- LITERALS & IDENTIFIERS ---
    Identifier,         // Variable and function names
    Int64,              // 42
    Float64,            // 3.14
    String,             // String
    True, False, Null,  // Keywords as literals

    // --- KEYWORDS: STRUCTURE ---
    Group,              // Class/Structure definition
    Function,           // 'sub' or 'function' keyword
    Extends,            // Inheritance/Injections
    Module,             // Namespace declaration
    Dismiss,            // Unload/Remove module
    Arrow,              // Loop sequencer
    Const,              // Constant declaration
    Use,                // Multiple file importing
    Deploy,             // Module deployment
    As,                 // Alias declaration
    Extern,             // External lib modifier
    Interface,

    // --- KEYWORDS: CONTROL FLOW ---
    If,
    Else,
    While,
    Through,
    LoopMode,
    Return,
    Break,
    Continue,

    // --- OPERATORS: ARITHMETIC ---
    Add,                // +
    Substract,          // -
    Multiply,           // *
    Division,           // /
    Double_Increment,   // ++
    Double_Decrement,   // --
    Floor_Divide,       //
    Modulo,             // %
    Power,
    Exclamatory,        // !
    Addresser,          // $

    // --- OPERATORS: LOGIC & RELATIONAL ---
    And, Or,            // &&, ||
    Equals,             // = (Assignment)
    Double_Equals,      // == (Comparison)
    Not_Equal,          // !=
    Greater,            // >
    Smaller,            // <
    Greater_Or_Equal,   // >=
    Smaller_Or_Equal,   // <=
    Pipeline,           // |>

    // --- DELIMITERS & SYMBOLS ---
    Left_Parenthese,    // (
    Right_Parenthese,   // )
    Left_CB,            // {
    Right_CB,           // }
    Left_Bracket,       // [
    Right_Bracket,      // ]
    Comma,              // ,
    Semicolon,          // ;
    Dot,                // .
    Double_Dot,         // ..

    // --- SPECIAL ---
    BuiltIn,            // Pre-defined functions
    End                 // End of File (EOF)
};

struct Token {
    std::string name;
    double value; // TODO fix this shit because we implemented Int64 and Float64
    VTokenType type;
    int line;

    Token(VTokenType t, int cl, double v = 0.0, std::string_view n = "")
        : name(n), value(v), type(t), line(cl) {
    }

    Token() : name(""), value(0.0), type(VTokenType::End), line(0) {}
};

std::vector<Token> tokenize(std::string_view input);
char advance();

inline std::string VTokenTypeToString(VTokenType type) {
    switch (type) {
        // --- LITERALS & IDENTIFIERS ---
        case VTokenType::Identifier:       return "Identifier";
        case VTokenType::Int64:            return "Int64";
        case VTokenType::Float64:          return "Float64";
        case VTokenType::String:           return "String";
        case VTokenType::True:             return "'true'";
        case VTokenType::False:            return "'false'";
        case VTokenType::Null:             return "'null'";

        // --- KEYWORDS: STRUCTURE ---
        case VTokenType::Group:            return "'group'";
        case VTokenType::Function:         return "'sub'";
        case VTokenType::Extends:          return "'::'";
        case VTokenType::Module:           return "'module'";
        case VTokenType::Dismiss:          return "'dismiss'";
        case VTokenType::Arrow:            return "'->'";
        case VTokenType::Const:            return "'const'";

        // --- KEYWORDS: CONTROL FLOW ---
        case VTokenType::If:               return "'if'";
        case VTokenType::Else:             return "'else'";
        case VTokenType::While:            return "'while'";
        case VTokenType::Through:          return "'through'";
        case VTokenType::LoopMode:         return "LoopMode Keyword";
        case VTokenType::Return:           return "'return'";
        case VTokenType::Break:            return "'break'";
        case VTokenType::Continue:         return "'continue'";

        // --- OPERATORS: ARITHMETIC ---
        case VTokenType::Add:              return "'+'";
        case VTokenType::Substract:        return "'-'";
        case VTokenType::Multiply:         return "'*'";
        case VTokenType::Division:         return "'/'";
        case VTokenType::Double_Increment: return "'++'";
        case VTokenType::Double_Decrement: return "'--'";
        case VTokenType::Floor_Divide:     return "'//'";
        case VTokenType::Modulo:           return "'%'";
        case VTokenType::Exclamatory:      return "'!'";

        // --- OPERATORS: LOGIC & RELATIONAL ---
        case VTokenType::And:              return "'&&'";
        case VTokenType::Or:               return "'||'";
        case VTokenType::Equals:           return "'='";
        case VTokenType::Double_Equals:    return "'=='";
        case VTokenType::Not_Equal:        return "'!='";
        case VTokenType::Greater:          return "'>'";
        case VTokenType::Smaller:          return "'<'";
        case VTokenType::Greater_Or_Equal: return "'>='";
        case VTokenType::Smaller_Or_Equal: return "'<='";
        case VTokenType::Pipeline:         return "'|>'";

        // --- DELIMITERS & SYMBOLS ---
        case VTokenType::Left_Parenthese:  return "'('";
        case VTokenType::Right_Parenthese: return "')'";
        case VTokenType::Left_CB:          return "'{'";
        case VTokenType::Right_CB:         return "'}'";
        case VTokenType::Left_Bracket:     return "'['";
        case VTokenType::Right_Bracket:    return "']'";
        case VTokenType::Comma:            return "','";
        case VTokenType::Semicolon:        return "';'";
        case VTokenType::Dot:              return "'.'";
        case VTokenType::Double_Dot:       return "'..'";

        // --- SPECIAL ---
        case VTokenType::BuiltIn:          return "Built-in Function";
        case VTokenType::End:              return "End of File";

        default:                           return "Unknown Token (" + std::to_string((int)type) + ")";
    }
}
