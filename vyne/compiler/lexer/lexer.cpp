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

#include "lexer.h"
#include "../../runtime/diagnostics.h"
#include <algorithm>

std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> tokens;
    // Token density is typically one token per 3-5 source bytes. Reserve a
    // conservative slice up front, capped so a multi-megabyte literal does
    // not turn into a huge speculative allocation.
    tokens.reserve(std::min<size_t>(input.size() / 4, 262144) + 16);
    size_t i = 0;
    int currentLine = 1;

    while (i < input.length()) {
        char character = input[i];

        if (std::isspace((unsigned char)character)) {
            if (character == '\n') {
                currentLine++;
            }
            i++;
            continue;
        }

        if (character == '"') {
            i++;
            std::string strBuffer;
            strBuffer.reserve(64);
            std::vector<std::pair<std::string, bool>>
                parts; // bool = is_interpolation
            bool isInterpolated = false;
            bool inInterpolation = false;
            std::string exprBuffer;
            exprBuffer.reserve(32);

            while (i < input.length() && input[i] != '"') {
                // Fast path: copy a whole run of ordinary characters with
                // one append. Most string literals have no escapes and no
                // interpolation, so this replaces a per-character
                // branch+append with a single memcpy per segment.
                if (input[i] != '\\' &&
                    (inInterpolation ? input[i] != '}' : input[i] != '{')) {
                    size_t start = i;
                    if (inInterpolation) {
                        while (i < input.length() && input[i] != '\\' &&
                               input[i] != '}' && input[i] != '"') {
                            ++i;
                        }
                    } else {
                        while (i < input.length() && input[i] != '\\' &&
                               input[i] != '{' && input[i] != '"') {
                            ++i;
                        }
                    }
                    if (i > start) {
                        if (inInterpolation)
                            exprBuffer.append(input.data() + start, i - start);
                        else
                            strBuffer.append(input.data() + start, i - start);
                        continue;
                    }
                }

                if (input[i] == '\\' && i + 1 < input.length()) {
                    char next = input[i + 1];
                    if (next == 'n') {
                        strBuffer += '\n';
                        i += 2;
                        continue;
                    } else if (next == 't') {
                        strBuffer += '\t';
                        i += 2;
                        continue;
                    } else if (next == 'r') {
                        strBuffer += '\r';
                        i += 2;
                        continue;
                    } else if (next == 'e') {
                        strBuffer += '\x1b';
                        i += 2;
                        continue;
                    } else if (next == '\\') {
                        strBuffer += '\\';
                        i += 2;
                        continue;
                    } else if (next == '\"') {
                        strBuffer += '\"';
                        i += 2;
                        continue;
                    } else if (next == '{') {
                        strBuffer += '{';
                        i += 2;
                        continue;
                    } else if (next == '}') {
                        strBuffer += '}';
                        i += 2;
                        continue;
                    } else if (next >= '0' && next <= '7') {
                        int val = 0, digits = 0;
                        size_t j = i + 1;
                        while (j < input.length() && digits < 3 &&
                               input[j] >= '0' && input[j] <= '7') {
                            val = val * 8 + (input[j] - '0');
                            digits++;
                            j++;
                        }
                        strBuffer += (char)val;
                        i = j;
                        continue;
                    }
                }

                if (input[i] == '{' && i + 1 < input.length() &&
                    input[i + 1] != '{') {
                    if (!strBuffer.empty()) {
                        parts.emplace_back(std::move(strBuffer), false);
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
                        parts.emplace_back(std::move(exprBuffer), true);
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

            // Only interpolated strings need a parts vector. A plain literal
            // was previously copied into `parts` too, then thrown away.
            if (isInterpolated && !strBuffer.empty()) {
                parts.emplace_back(std::move(strBuffer), false);
            }

            if (i < input.length())
                i++; // Skip closing "

            // If not interpolated, just return normal string
            if (!isInterpolated) {
                tokens.emplace_back(
                    VTokenType::String,
                    currentLine,
                    std::move(strBuffer),
                    ""
                );
            } else {
                // Create InterpolatedString token with parts
                tokens.emplace_back(
                    VTokenType::InterpolatedString,
                    currentLine,
                    std::move(parts),
                    ""
                );
            }
            continue;
        }

        if (std::isdigit((unsigned char)character)) {
            // Hex literals: `0x` or `0X` followed by one or more hex digits.
            // Emitted as Int64 only. Float64 hex (`0x1.8p3`) is a C99 feature
            // that would require reworking the NumberNode/codegen path; not
            // needed for masks and bit constants, which is the common case.
            if (character == '0' && i + 1 < input.length() &&
                (input[i + 1] == 'x' || input[i + 1] == 'X')) {
                size_t start = i;
                i += 2; // skip 0x
                std::string hexDigits;
                while (i < input.length()) {
                    if (std::isxdigit((unsigned char)input[i])) {
                        hexDigits += input[i++];
                    } else if (
                        input[i] == '_' && !hexDigits.empty() &&
                        i + 1 < input.length() &&
                        std::isxdigit((unsigned char)input[i + 1])
                    ) {
                        ++i; // digit separator
                    } else {
                        break;
                    }
                }
                if (hexDigits.empty()) {
                    emitError(
                        "Hex literal has no digits after '0x'",
                        currentLine,
                        "VNE-006",
                        {"Write 0x followed by at least one hex digit"}
                    );
                    throw std::runtime_error(
                        "Lexing failed at line " + std::to_string(currentLine)
                    );
                }
                // Parse as unsigned 64-bit, then reinterpret as signed.
                // `0xFFFFFFFFFFFFFFFF` wraps to -1, matching C's behaviour
                // for out-of-range signed hex literals in a small way — the
                // common case (`0x7FFFFFFF`, `0xFF`, `0xDEADBEEF`) is
                // unaffected either way.
                uint64_t unsignedVal = 0;
                for (char c : hexDigits) {
                    unsignedVal = unsignedVal * 16 +
                                  (c >= '0' && c <= '9'   ? c - '0'
                                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                                          : c - 'A' + 10);
                }
                tokens.emplace_back(
                    VTokenType::Int64,
                    currentLine,
                    static_cast<int64_t>(unsignedVal),
                    ""
                );
                continue;
            }

            std::string buffer;
            bool isFloatingPoint = false;

            // Phase 1 — mantissa. Integer digits, optional single
            // decimal point, optional fractional digits. The `..`
            // range operator is rejected here so `0..10` does not get
            // misread as the float 0. followed by .10.
            while (i < input.length()) {
                if (std::isdigit((unsigned char)input[i])) {
                    buffer += input[i++];
                } else if (
                    input[i] == '_' && !buffer.empty() &&
                    std::isdigit((unsigned char)buffer.back()) &&
                    i + 1 < input.length() &&
                    std::isdigit((unsigned char)input[i + 1])
                ) {
                    ++i; // digit separator: 64_000_000
                } else if (input[i] == '.') {
                    if (i + 1 < input.length() && input[i + 1] == '.') {
                        break;
                    }
                    if (isFloatingPoint)
                        break;

                    isFloatingPoint = true;
                    buffer += input[i++];
                } else {
                    break;
                }
            }

            // Phase 2 — optional exponent. Commit only when the 'e' or
            // 'E' is followed by a well-formed exponent: an optional
            // sign, then at least one digit. Otherwise, roll back and
            // leave the 'e' for the identifier lexer. This is what
            // makes `2e` lex as Int64(2) then Identifier("e"), and
            // `2ex5` as Int64(2) then Identifier("ex5"), without
            // breaking a variable that happens to be named `e`.
            if (i < input.length() && (input[i] == 'e' || input[i] == 'E')) {
                size_t probe = i + 1;
                if (probe < input.length() &&
                    (input[probe] == '+' || input[probe] == '-')) {
                    probe++;
                }
                if (probe < input.length() && std::isdigit((unsigned char)input[probe])) {
                    buffer += input[i++]; // 'e' or 'E'
                    if (input[i] == '+' || input[i] == '-') {
                        buffer += input[i++]; // exponent sign
                    }
                    while (i < input.length()) {
                        if (std::isdigit((unsigned char)input[i])) {
                            buffer += input[i++];
                        } else if (
                            input[i] == '_' && i + 1 < input.length() &&
                            std::isdigit((unsigned char)input[i + 1])
                        ) {
                            ++i; // digit separator in the exponent
                        } else {
                            break;
                        }
                    }
                    // Any literal written with e-notation is a float,
                    // even when the mantissa is integral. Matches C,
                    // C++, Python, and JS: `1e5` is 100000.0, not the
                    // integer 100000. It also stops literals like
                    // `1e20` from silently overflowing the Int64 path.
                    isFloatingPoint = true;
                }
            }

            // Phase 3 - optional Float32 suffix. `1.0f` / `2f` / `1e3f`
            // lex as a Float32 literal. The suffix is only consumed when it
            // is not followed by an identifier character, so `1.0foo` still
            // becomes a Float64 followed by identifier `foo`.
            bool isFloat32Literal = false;
            if (i < input.length() && (input[i] == 'f' || input[i] == 'F')) {
                size_t next = i + 1;
                bool ident_after = (next < input.length()) &&
                    (std::isalnum((unsigned char)input[next]) || input[next] == '_');
                if (!ident_after) {
                    isFloat32Literal = true;
                    isFloatingPoint = true;
                    i++;
                }
            }

            if (isFloatingPoint) {
                double val;
                auto [ptr, ec] = std::from_chars(
                    buffer.data(),
                    buffer.data() + buffer.size(),
                    val
                );
                if (ec == std::errc{}) {
                    tokens.emplace_back(
                        isFloat32Literal ? VTokenType::Float32
                                         : VTokenType::Float64,
                        currentLine,
                        val,
                        ""
                    );
                } else {
                    // Route through the diagnostic engine instead of
                    // writing straight to stderr. The engine already
                    // knows the current filename (the parser sets it
                    // before calling tokenize), so the report is
                    // "file.vy:12: malformed float literal '…'"
                    // rather than a bare "line 12".
                    emitError(
                        "Malformed float literal '" + buffer + "'",
                        currentLine,
                        "VNE-006",
                        {"Use digits before and after the decimal point",
                         "Scientific notation is supported: 1.5e-3"}
                    );
                    throw std::runtime_error(
                        "Lexing failed at line " + std::to_string(currentLine)
                    );
                }
            } else {
                long long val;
                auto [ptr, ec] = std::from_chars(
                    buffer.data(),
                    buffer.data() + buffer.size(),
                    val
                );
                if (ec == std::errc{}) {
                    tokens.emplace_back(
                        VTokenType::Int64,
                        currentLine,
                        static_cast<int64_t>(val),
                        ""
                    );
                } else {
                    emitError(
                        "Malformed integer literal '" + buffer + "'",
                        currentLine,
                        "VNE-007",
                        {"Integer literals must fit in a signed 64-bit value"}
                    );
                    throw std::runtime_error(
                        "Lexing failed at line " + std::to_string(currentLine)
                    );
                }
            }
            continue;
        }

        if (std::isalpha((unsigned char)character) || character == '_') {
            size_t start = i;
            while (i < input.length() &&
                   (std::isalnum((unsigned char)input[i]) || input[i] == '_')) {
                i++;
            }

            std::string_view buffer = input.substr(start, i - start);

            auto it = keywords.find(buffer);
            if (it != keywords.end()) {
                tokens.emplace_back(it->second, currentLine, 0, buffer);
            } else {
                tokens.emplace_back(
                    VTokenType::Identifier,
                    currentLine,
                    0,
                    buffer
                );
            }
            continue;
        }

        switch (character) {
        case '@':
            tokens.emplace_back(VTokenType::At, currentLine, 0, "@");
            break;
        case '(':
            tokens
                .emplace_back(VTokenType::Left_Parenthese, currentLine, 0, "(");
            break;
        case ')':
            tokens.emplace_back(
                VTokenType::Right_Parenthese,
                currentLine,
                0,
                ")"
            );
            break;
        case '{':
            tokens.emplace_back(VTokenType::Left_CB, currentLine, 0, "{");
            break;
        case '}':
            tokens.emplace_back(VTokenType::Right_CB, currentLine, 0, "}");
            break;
        case '[':
            tokens.emplace_back(VTokenType::Left_Bracket, currentLine, 0, "[");
            break;
        case ']':
            tokens.emplace_back(VTokenType::Right_Bracket, currentLine, 0, "]");
            break;
        case ',':
            tokens.emplace_back(VTokenType::Comma, currentLine, 0, ",");
            break;
        case ';':
            tokens.emplace_back(VTokenType::Semicolon, currentLine, 0, ";");
            break;
        case '%':
            tokens.emplace_back(VTokenType::Modulo, currentLine, 0, "%");
            break;
        case '$':
            tokens.emplace_back(VTokenType::Addresser, currentLine, 0, "$");
            break;
        case '?': {
            if (i + 1 < input.length() && input[i + 1] == '?') {
                if (i + 2 < input.length() && input[i + 2] == '=') {
                    tokens.emplace_back(
                        VTokenType::NullCoalesceAssign,
                        currentLine,
                        0,
                        "??="
                    );
                    i += 3;
                } else {
                    tokens.emplace_back(
                        VTokenType::NullCoalesce,
                        currentLine,
                        0,
                        "??"
                    );
                    i += 2;
                }
            } else {
                tokens.emplace_back(VTokenType::Question, currentLine, 0, "?");
                i++;
            }
            continue;
        }
        case '/': {
            if (i + 1 < input.length() && input[i + 1] == '/') {
                tokens.emplace_back(
                    VTokenType::Floor_Divide,
                    currentLine,
                    0,
                    "//"
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Division, currentLine, 0, "/");
            }
            break;
        }
        case '.': {
            if (i + 1 < input.length() && input[i + 1] == '.') {
                tokens
                    .emplace_back(VTokenType::Double_Dot, currentLine, 0, "..");
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
                tokens.emplace_back(
                    VTokenType::Double_Increment,
                    currentLine,
                    0,
                    "++"
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Add, currentLine, 0, "+");
            }
            break;
        }
        case '<': {
            if (i + 1 < input.length() && input[i + 1] == '=') {
                tokens.emplace_back(
                    VTokenType::Smaller_Or_Equal,
                    currentLine,
                    0,
                    "<="
                );
                i++;
            } else if (i + 1 < input.length() && input[i + 1] == '<') {
                tokens.emplace_back(
                    VTokenType::Bitwise_Sll,
                    currentLine,
                    0,
                    "<<"
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Smaller, currentLine, 0, "<");
            }
            break;
        }
        case '>': {
            if (i + 1 < input.length() && input[i + 1] == '=') {
                tokens.emplace_back(
                    VTokenType::Greater_Or_Equal,
                    currentLine,
                    0,
                    ">="
                );
                i++;
            } else if (i + 1 < input.length() && input[i + 1] == '>') {
                tokens.emplace_back(
                    VTokenType::Bitwise_Srl,
                    currentLine,
                    0,
                    ">>"
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Greater, currentLine, 0, ">");
            }
            break;
        }
        case '~': {
            tokens.emplace_back(VTokenType::Bitwise_Not, currentLine, 0, "~");
            break;
        }
        case '=': {
            if (i + 1 < input.length() && input[i + 1] == '=') {
                tokens.emplace_back(
                    VTokenType::Double_Equals,
                    currentLine,
                    0,
                    "=="
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Equals, currentLine, 0, "=");
            }
            break;
        }
        case '!': {
            if (i + 1 < input.length() && input[i + 1] == '=') {
                tokens
                    .emplace_back(VTokenType::Not_Equal, currentLine, 0, "!=");
                i++;
            } else {
                tokens
                    .emplace_back(VTokenType::Exclamatory, currentLine, 0, "!");
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
        case ':': {
            // `::`  — Extends (type annotation, group target)
            // `:=`  — Declare_Assign (fresh variable declaration)
            // `:`   — Colon (map literals, ternary, slice)
            // Order matters: check the two-character forms first.
            if (i + 1 < input.length() && input[i + 1] == ':') {
                tokens.emplace_back(VTokenType::Extends, currentLine, 0, "::");
                i += 2;
            } else if (i + 1 < input.length() && input[i + 1] == '=') {
                tokens.emplace_back(
                    VTokenType::Declare_Assign,
                    currentLine,
                    0,
                    ":="
                );
                i += 2;
            } else {
                tokens.emplace_back(VTokenType::Colon, currentLine, 0, ":");
                i++;
            }
            continue;
        }
        case '&': {
            if (i + 1 < input.length() && input[i + 1] == '&') {
                tokens.emplace_back(VTokenType::And, currentLine, 0, "&&");
                i += 2;
            } else {
                tokens
                    .emplace_back(VTokenType::Bitwise_And, currentLine, 0, "&");
                i++;
            }
            continue;
        }
        case '|': {
            if (i + 1 < input.length() && input[i + 1] == '|') {
                tokens.emplace_back(VTokenType::Or, currentLine, 0, "||");
                i++;
            } else if (i + 1 < input.length() && input[i + 1] == '>') {
                tokens.emplace_back(VTokenType::Pipeline, currentLine, 0, "|>");
                i++;
            } else {
                tokens
                    .emplace_back(VTokenType::Bitwise_Or, currentLine, 0, "|");
            }
            break;
        }
        case '^': {
            if (i + 1 < input.length() && input[i + 1] == '^') {
                tokens
                    .emplace_back(VTokenType::Referencer, currentLine, 0, "^^");
                i++;
            } else {
                tokens
                    .emplace_back(VTokenType::Bitwise_Xor, currentLine, 0, "^");
            }
            break;
        }
        case '-': {
            if (i + 1 < input.length() && input[i + 1] == '>') {
                tokens.emplace_back(VTokenType::Arrow, currentLine, 0, "->");
                i++;
            } else if (i + 1 < input.length() && input[i + 1] == '-') {
                tokens.emplace_back(
                    VTokenType::Double_Decrement,
                    currentLine,
                    0,
                    "--"
                );
                i++;
            } else {
                tokens.emplace_back(VTokenType::Substract, currentLine, 0, "-");
            }
            break;
        }
        default:
            emitError(
                std::string("Unexpected character '") + character +
                    "' (ASCII " +
                    std::to_string((int)(unsigned char)character) + ")",
                currentLine,
                "VNE-008",
                {"Check the character; it is not part of the Vyne grammar"}
            );
            throw std::runtime_error(
                "Lexing failed at line " + std::to_string(currentLine)
            );
        }
        i++;
    }

    tokens.emplace_back(VTokenType::End, currentLine, 0, "");
    return tokens;
}
