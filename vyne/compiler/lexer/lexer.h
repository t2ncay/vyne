// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Vyne is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the
// Free Software Foundation, version 3.
//
// Vyne is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Vyne. If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <cctype>
#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

enum class VTokenType {
    // --- LITERALS & IDENTIFIERS ---
    Identifier, // Variable and function names
    Int64,      // 42
    Float64,    // 3.14
    String,     // String
    True,
    False,
    Null, // Keywords as literals

    // --- KEYWORDS: STRUCTURE ---
    Group,     // Class/Structure definition
    Function,  // 'sub' or 'function' keyword
    Extends,   // Inheritance/Injections
    Module,    // Namespace declaration
    Dismiss,   // Unload/Remove module
    Arrow,     // Loop sequencer
    Const,     // Constant declaration
    Use,       // Multiple file importing
    Deploy,    // Module deployment
    As,        // Alias declaration
    External,  // External lib modifier
    Native,    // Native lib modifier
    Interface, // Struct definers
    Region,    // 'region' keyword (A1/A3)
    Scratch,   // 'scratch' — shaped stack array declaration
    Enum,      // Enumeration

    // --- KEYWORDS: CONTROL FLOW ---
    If,
    Else,
    While,
    Through,
    LoopMode,
    Return,
    Break,
    Continue,
    Question,

    // --- OPERATORS: ARITHMETIC ---
    Add,              // +
    Substract,        // -
    Multiply,         // *
    Division,         // /
    Double_Increment, // ++
    Double_Decrement, // --
    Floor_Divide,     //
    Modulo,           // %
    Power,
    Exclamatory, // !
    Addresser,   // $

    // --- OPERATORS : BITWISE ---
    Bitwise_And, // &
    Bitwise_Or,  // |
    Bitwise_Xor, // ^
    Bitwise_Not, // ~
    Bitwise_Sll, // <<
    Bitwise_Srl, // >>

    // --- OPERATORS: LOGIC & RELATIONAL ---
    And,
    Or,               // &&, ||
    Equals,           // = (Assignment)
    Double_Equals,    // == (Comparison)
    Not_Equal,        // !=
    next == VTokenType::NullCoalesceAssign) Greater,          // >
    Smaller,          // <
    Greater_Or_Equal, // >=
    Smaller_Or_Equal, // <=
    Pipeline,         // |>
    Referencer,       // ^^

    // --- DELIMITERS & SYMBOLS ---
    Left_Parenthese,    // (
    Right_Parenthese,   // )
    Left_CB,            // {
    Right_CB,           // }
    Left_Bracket,       // [
    Right_Bracket,      // ]
    Comma,              // ,
    Colon,              // :
    Declare_Assign,     // :=
    Semicolon,          // ;
    Dot,                // .
    Double_Dot,         // ..
    InterpolatedString, // For strings with {expr}
    InterpolationStart, // '{' inside string
    InterpolationEnd,   // '}' inside string
    NullCoalesceAssign,
    NullCoalesce,

    // --- RULESETS ---
    Ruleset, // Ruleset
    Warnings,
    Dynamic_Casting,
    Memory_Limit,
    In,

    // --- KEYWORDS: ERROR HANDLING ---
    Try,     // try block
    Catch,   // catch block
    Throw,   // throw statement
    Finally, // finally block

    // --- SPECIAL ---
    BuiltIn, // Pre-defined functions
    Defer,   // defer
    At,      // @
    End      // End of File (EOF)
};

static const std::unordered_map<std::string_view, VTokenType> keywords = {
    {"out", VTokenType::BuiltIn},
    {"sizeof", VTokenType::BuiltIn},
    {"type", VTokenType::BuiltIn},
    {"string", VTokenType::BuiltIn},
    {"int64", VTokenType::BuiltIn},
    {"float64", VTokenType::BuiltIn},
    {"sequence", VTokenType::BuiltIn},
    {"exit", VTokenType::BuiltIn},
    {"group", VTokenType::Group},
    {"true", VTokenType::True},
    {"false", VTokenType::False},
    {"null", VTokenType::Null},
    {"fn", VTokenType::Function},
    {"return", VTokenType::Return},
    {"while", VTokenType::While},
    {"through", VTokenType::Through},
    {"loop", VTokenType::LoopMode},
    {"collect", VTokenType::LoopMode},
    {"unique", VTokenType::LoopMode},
    {"every", VTokenType::LoopMode},
    {"filter", VTokenType::LoopMode},
    {"break", VTokenType::Break},
    {"continue", VTokenType::Continue},
    {"module", VTokenType::Module},
    {"dismiss", VTokenType::Dismiss},
    {"if", VTokenType::If},
    {"else", VTokenType::Else},
    {"const", VTokenType::Const},
    {"use", VTokenType::Use},
    {"deploy", VTokenType::Deploy},
    {"as", VTokenType::As},
    {"external", VTokenType::External},
    {"native", VTokenType::Native},
    {"interface", VTokenType::Interface},
    {"region", VTokenType::Region},
    {"scratch", VTokenType::Scratch},
    {"ruleset", VTokenType::Ruleset},
    {"warnings", VTokenType::Warnings},
    {"dynamic_casting", VTokenType::Dynamic_Casting},
    {"memory_limit", VTokenType::Memory_Limit},
    {"enum", VTokenType::Enum},
    {"defer", VTokenType::Defer},
    {"free", VTokenType::BuiltIn},
    {"in", VTokenType::In},
    {"try", VTokenType::Try},
    {"catch", VTokenType::Catch},
    {"throw", VTokenType::Throw},
    {"finally", VTokenType::Finally},
    {"map", VTokenType::BuiltIn},
};

// tokens
using TokenData = std::variant<
    std::monostate,
    double,
    int64_t,
    std::string,
    std::vector<std::pair<std::string, bool>>>;

struct Token {
    std::string name;
    TokenData literal;
    VTokenType type;
    int line;

    Token(
        VTokenType t,
        int cl,
        TokenData lit = std::monostate{},
        std::string_view n = ""
    )
        : name(n)
        , literal(std::move(lit))
        , type(t)
        , line(cl) {}

    double value_legacy() const {
        if (std::holds_alternative<double>(literal))
            return std::get<double>(literal);
        if (std::holds_alternative<int64_t>(literal))
            return static_cast<double>(std::get<int64_t>(literal));
        return 0.0;
    }

    Token()
        : name("")
        , literal(std::monostate{})
        , type(VTokenType::End)
        , line(0) {}
};

std::vector<Token> tokenize(std::string_view input);
char advance();

inline std::string VTokenTypeToString(VTokenType type) {
    switch (type) {
    // --- LITERALS & IDENTIFIERS ---
    case VTokenType::Identifier:
        return "Identifier";
    case VTokenType::Int64:
        return "Int64";
    case VTokenType::Float64:
        return "Float64";
    case VTokenType::String:
        return "String";
    case VTokenType::True:
        return "'true'";
    case VTokenType::False:
        return "'false'";
    case VTokenType::Null:
        return "'null'";

    // --- KEYWORDS: STRUCTURE ---
    case VTokenType::Group:
        return "'group'";
    case VTokenType::Function:
        return "'fn'";
    case VTokenType::Extends:
        return "'::'";
    case VTokenType::Module:
        return "'module'";
    case VTokenType::Dismiss:
        return "'dismiss'";
    case VTokenType::Arrow:
        return "'->'";
    case VTokenType::Const:
        return "'const'";
    case VTokenType::Use:
        return "'use'";
    case VTokenType::Deploy:
        return "'deploy'";
    case VTokenType::As:
        return "'as'";
    case VTokenType::External:
        return "'extern'";
    case VTokenType::Interface:
        return "'interface'";
    case VTokenType::Region:
        return "'region'";
    case VTokenType::Scratch:
        return "'scratch'";
    case VTokenType::Ruleset:
        return "'ruleset'";

    // --- KEYWORDS: CONTROL FLOW ---
    case VTokenType::If:
        return "'if'";
    case VTokenType::Else:
        return "'else'";
    case VTokenType::While:
        return "'while'";
    case VTokenType::Through:
        return "'through'";
    case VTokenType::LoopMode:
        return "LoopMode";
    case VTokenType::Return:
        return "'return'";
    case VTokenType::Break:
        return "'break'";
    case VTokenType::Continue:
        return "'continue'";

    // --- OPERATORS: ARITHMETIC ---
    case VTokenType::Add:
        return "'+'";
    case VTokenType::Substract:
        return "'-'";
    case VTokenType::Multiply:
        return "'*'";
    case VTokenType::Division:
        return "'/'";
    case VTokenType::Double_Increment:
        return "'++'";
    case VTokenType::Double_Decrement:
        return "'--'";
    case VTokenType::Floor_Divide:
        return "'//'";
    case VTokenType::Modulo:
        return "'%'";
    case VTokenType::Power:
        return "'**'";
    case VTokenType::Exclamatory:
        return "'!'";
    case VTokenType::Addresser:
        return "'$'";
    case VTokenType::Referencer:
        return "'^^'";

    // --- OPERATORS: BITWISE ---
    case VTokenType::Bitwise_And:
        return "'&'";
    case VTokenType::Bitwise_Or:
        return "'|'";
    case VTokenType::Bitwise_Xor:
        return "'^";
    case VTokenType::Bitwise_Not:
        return "'~'";
    case VTokenType::Bitwise_Sll:
        return "'<<'";
    case VTokenType::Bitwise_Srl:
        return "'>>'";

    // --- OPERATORS: LOGIC & RELATIONAL ---
    case VTokenType::And:
        return "'&&'";
    case VTokenType::Or:
        return "'||'";
    case VTokenType::Equals:
        return "'='";
    case VTokenType::Double_Equals:
        return "'=='";
    case VTokenType::Not_Equal:
        return "'!='";
    case VTokenType::Greater:
        return "'>'";
    case VTokenType::Smaller:
        return "'<'";
    case VTokenType::Greater_Or_Equal:
        return "'>='";
    case VTokenType::Smaller_Or_Equal:
        return "'<='";
    case VTokenType::Pipeline:
        return "'|>'";

    // --- DELIMITERS & SYMBOLS ---
    case VTokenType::Left_Parenthese:
        return "'('";
    case VTokenType::Right_Parenthese:
        return "')'";
    case VTokenType::Left_CB:
        return "'{'";
    case VTokenType::Right_CB:
        return "'}'";
    case VTokenType::Left_Bracket:
        return "'['";
    case VTokenType::Right_Bracket:
        return "']'";
    case VTokenType::Comma:
        return "','";
    case VTokenType::Colon:
        return "':'";
    case VTokenType::Declare_Assign:
        return "':='";
    case VTokenType::Semicolon:
        return "';'";
    case VTokenType::Dot:
        return "'.'";
    case VTokenType::Double_Dot:
        return "'..'";

    // --- SPECIAL ---
    case VTokenType::BuiltIn:
        return "BuiltIn";
    case VTokenType::At:
        return "'@'";
    case VTokenType::End:
        return "EOF";

    default:
        return "Unknown (" + std::to_string(static_cast<int>(type)) + ")";
    }
}
