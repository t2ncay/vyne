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

// cli/init.h - `vynec init` entry point.
//
// `argv` is the full process argument vector; argv[1] is "init". The
// function returns the process exit code:
//
//   0 success
//   1 user cancel
//   2 non-interactive environment without flags
//   3 project.toml already exists
//   4 generation or configuration error

int runInit(int argc, char** argv);
