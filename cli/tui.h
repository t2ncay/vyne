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

// cli/tui.h - the interactive `vynec init` questionnaire.
//
// The TUI owns only the question flow and the summary screen. Generation
// stays in cli/init.cpp. `locked` names fields already supplied on the
// command line; those questions are skipped, but their values still seed
// the confirmation screen.

#include <filesystem>
#include <set>
#include <string>
#include <vector>

struct InitChoices {
    std::string kind = "ml";
    std::vector<std::string> targets;
    std::vector<std::string> libraries;
    bool cuda = false;
    std::string license = "AGPL-3.0-only";
};

// Returns true when the user confirmed a project. `cancelled` is set when
// the user pressed Esc/Ctrl-C. `error` is non-empty on a terminal-size or
// rendering failure. The `locked` set uses the flag names: kind, targets,
// libraries, cuda, license.
bool runQuestionnaire(
    InitChoices& choices,
    const std::filesystem::path& shareDir,
    const std::string& projectName,
    const std::set<std::string>& locked,
    std::string& error,
    bool& cancelled
);
