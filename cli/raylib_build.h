// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
#pragma once
#include <string>

// Builds pinned native sources in a compiler/source-specific user cache and
// returns linker inputs for generated desktop C. No compiler host dependency.
std::string raylibLinkFlags(const std::string& exeDir);
