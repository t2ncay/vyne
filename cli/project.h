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

#include "file_handler.h"

#include <string>

// ---------------------------------------------------------------------------
// Project discovery and project.toml parsing
// ---------------------------------------------------------------------------
// `vynec build` / `vynec run` behave like `cargo build` / `cargo run`: when
// no source path is supplied, the driver walks up from the current directory
// until it finds project.toml, then resolves the conventional entry point
// (src/main.vy, then main.vy).
//
// The TOML reader is deliberately small and schema-aware. It handles the
// subset emitted by `vynec init` — sections, string/boolean/integer values,
// comments, and quoted keys — without pulling in a full TOML dependency.
// Unknown sections and keys are ignored for forward compatibility.
// ---------------------------------------------------------------------------

// Walk up from `startDir` looking for `project.toml`. Returns true and sets
// `outPath` to the absolute file path when found.
bool findProjectToml(const std::string& startDir, std::string& outPath);

// Look for `src/main.vy` first, then `main.vy`, relative to `projectDir`.
// Returns true and sets `outEntry` to the absolute entry-point path.
bool findProjectEntry(const std::string& projectDir, std::string& outEntry);

// Result of a full project lookup: discovery + parsing + dependency
// resolution. `found == false` means no project.toml was walked up to;
// `ok == false` means one was found but failed to parse.
struct ProjectLoadResult {
    bool found = false;
    bool ok = false;
    ProjectConfig config;
    std::string error;
};

// Discover project.toml from `startDir`, parse it, and resolve dependencies.
ProjectLoadResult loadProject(const std::string& startDir);

// Parse a project.toml file. On failure returns false and, when `error` is
// non-null, fills it with a `path:line: message` diagnostic.
bool readProjectToml(
    const std::string& path,
    ProjectConfig& out,
    std::string* error = nullptr
);

// Resolve every entry in `cfg.dependencies` against the Vyne installation's
// module roots. The result is stored in `cfg.dependency_paths` (parallel to
// `dependencies`); unresolved entries are empty strings.
void resolveProjectDependencies(ProjectConfig& cfg);

// Fold a parsed project into the build driver's options. A board target
// routes to the Arduino pipeline, while a CUDA device implies host-native
// code generation. Explicit CLI flags are applied after this function, so
// they still win.
void applyProjectConfig(const ProjectConfig& cfg, BuildOptions& opts);
