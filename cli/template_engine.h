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

// cli/template_engine.h - the tiny template language `vynec init` uses.
//
// Supported syntax:
//
//   {{name}}                     scalar substitution
//   {{#if name}}...{{/if}}       render the body when `name` is truthy
//   {{#each list}}...{{/each}}   repeat the body for each item
//   {{.}}                        current item inside #each
//   {{@index}}                   zero-based item index inside #each
//
// There is deliberately no partial include, no expression evaluation,
// and no escaping. Templates are data; every decision is made in C++
// before the engine runs. Unknown variables render as the empty string.
//
// Truthiness: a non-empty scalar other than "0" or "false"
// (case-insensitive), or a non-empty list.

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace Vyne::Template {

struct Context {
    std::unordered_map<std::string, std::string> vars;
    std::unordered_map<std::string, std::vector<std::string>> lists;

    bool truthy(const std::string& name) const;
};

inline std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(
        std::find_if(s.rbegin(), s.rend(), notSpace).base(),
        s.end()
    );
    return s;
}

inline bool Context::truthy(const std::string& name) const {
    auto list = lists.find(name);
    if (list != lists.end())
        return !list->second.empty();

    auto var = vars.find(name);
    if (var == vars.end())
        return false;
    if (var->second.empty())
        return false;

    std::string lower = var->second;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return lower != "0" && lower != "false";
}

class Engine {
public:
    // Render `source`. On failure `error` describes the first malformed
    // block and `out` is left in an unspecified (but valid) state.
    static bool render(
        const std::string& source,
        const Context& ctx,
        std::string& out,
        std::string& error
    ) {
        out.clear();
        size_t pos = 0;
        Frame frame;
        return renderRange(source, pos, "", ctx, frame, out, error);
    }

    // Exercises scalar substitution, if/each, nesting, and missing
    // variables. Returns true when every case matches.
    static bool selfTest(std::string& error) {
        Context ctx;
        ctx.vars["name"] = "Vyne";
        ctx.vars["flag"] = "1";
        ctx.vars["off"] = "0";
        ctx.vars["empty"] = "";
        ctx.lists["libs"] = {"a", "b"};

        auto expect = [&](const std::string& src, const std::string& want) {
            std::string got;
            std::string err;
            if (!render(src, ctx, got, err)) {
                error = "render failed for '" + src + "': " + err;
                return false;
            }
            if (got != want) {
                error = "template '" + src + "' produced '" + got +
                        "', expected '" + want + "'";
                return false;
            }
            return true;
        };

        return expect("Hello {{name}}", "Hello Vyne") &&
               expect("x{{missing}}y", "xy") &&
               expect("{{#if flag}}yes{{/if}}", "yes") &&
               expect("{{#if off}}yes{{/if}}", "") &&
               expect("{{#if missing}}yes{{/if}}", "") &&
               expect("{{#if empty}}yes{{/if}}", "") &&
               expect(
                   "{{#if flag}}A{{#if off}}B{{/if}}C{{/if}}",
                   "AC"
               ) &&
               expect(
                   "{{#each libs}}[{{.}}]{{/each}}",
                   "[a][b]"
               ) &&
               expect(
                   "{{#each libs}}{{@index}}={{.}};{{/each}}",
                   "0=a;1=b;"
               ) &&
               expect(
                   "{{#each libs}}{{#if flag}}{{.}}{{/if}}{{/each}}",
                   "ab"
               );
    }

private:
    struct Frame {
        std::string item;
        std::string index;
        bool hasItem = false;
    };

    static std::string lookup(
        const std::string& name,
        const Context& ctx,
        const Frame& frame
    ) {
        if (name == "." && frame.hasItem)
            return frame.item;
        if (name == "@index" && frame.hasItem)
            return frame.index;

        auto it = ctx.vars.find(name);
        return it == ctx.vars.end() ? std::string() : it->second;
    }

    // Locate the matching close for an already-consumed opening tag.
    // `bodyStart` is just after the opening tag; on success `bodyEnd`
    // is the start of the close tag and `afterEnd` is just past it.
    static bool findBlock(
        const std::string& src,
        size_t bodyStart,
        const std::string& kind,
        size_t& bodyEnd,
        size_t& afterEnd,
        std::string& error
    ) {
        int depth = 1;
        size_t pos = bodyStart;

        while (pos < src.size()) {
            size_t open = src.find("{{", pos);
            if (open == std::string::npos) {
                error = "missing {{/" + kind + "}}";
                return false;
            }
            size_t close = src.find("}}", open + 2);
            if (close == std::string::npos) {
                error = "unterminated tag while looking for {{/" + kind + "}}";
                return false;
            }

            std::string tag = trim(src.substr(open + 2, close - open - 2));
            if (!tag.empty() && tag[0] == '#') {
                depth++;
            } else if (!tag.empty() && tag[0] == '/') {
                depth--;
                if (depth == 0) {
                    std::string closeName = trim(tag.substr(1));
                    if (closeName != kind) {
                        error = "mismatched closing tag: expected {{/" +
                                kind + "}}, got {{" + tag + "}}";
                        return false;
                    }
                    bodyEnd = open;
                    afterEnd = close + 2;
                    return true;
                }
            }

            pos = close + 2;
        }

        error = "missing {{/" + kind + "}}";
        return false;
    }

    static bool renderRange(
        const std::string& src,
        size_t& pos,
        const std::string& stopTag,
        const Context& ctx,
        const Frame& frame,
        std::string& out,
        std::string& error
    ) {
        while (pos < src.size()) {
            size_t open = src.find("{{", pos);
            if (open == std::string::npos) {
                out.append(src, pos, std::string::npos);
                pos = src.size();
                break;
            }

            out.append(src, pos, open - pos);

            size_t close = src.find("}}", open + 2);
            if (close == std::string::npos) {
                error = "unterminated '{{' at byte " + std::to_string(open);
                return false;
            }

            std::string tag = trim(src.substr(open + 2, close - open - 2));
            pos = close + 2;

            if (tag.empty()) {
                error = "empty '{{}}' tag";
                return false;
            }

            if (tag[0] == '/') {
                if (!stopTag.empty() && tag == stopTag)
                    return true;
                error = "unexpected closing tag '{{" + tag + "}}'";
                return false;
            }

            if (tag[0] != '#') {
                out += lookup(tag, ctx, frame);
                continue;
            }

            std::string rest = trim(tag.substr(1));
            size_t space = rest.find_first_of(" \t");
            std::string kind = rest.substr(0, space);
            std::string arg =
                space == std::string::npos ? "" : trim(rest.substr(space + 1));

            if (kind != "if" && kind != "each") {
                error = "unknown block '#" + kind + "'";
                return false;
            }
            if (arg.empty()) {
                error = "block '#" + kind + "' needs a name";
                return false;
            }

            size_t bodyEnd = 0;
            size_t afterEnd = 0;
            if (!findBlock(src, pos, kind, bodyEnd, afterEnd, error))
                return false;

            std::string body = src.substr(pos, bodyEnd - pos);
            pos = afterEnd;

            if (kind == "if") {
                if (ctx.truthy(arg)) {
                    size_t inner = 0;
                    if (!renderRange(body, inner, "", ctx, frame, out, error))
                        return false;
                }
                continue;
            }

            auto list = ctx.lists.find(arg);
            if (list != ctx.lists.end()) {
                for (size_t i = 0; i < list->second.size(); ++i) {
                    Frame child = frame;
                    child.item = list->second[i];
                    child.index = std::to_string(i);
                    child.hasItem = true;

                    size_t inner = 0;
                    if (!renderRange(body, inner, "", ctx, child, out, error))
                        return false;
                }
            }
        }

        if (!stopTag.empty()) {
            error = "missing closing tag '{{" + stopTag + "}}'";
            return false;
        }
        return true;
    }
};

} // namespace Vyne::Template
