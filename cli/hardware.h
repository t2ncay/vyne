// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

using HardwareValue = std::variant<std::string, int, bool>;
struct HardwareConfig {
    std::string preset;
    std::string label;
    std::string fqbn;
    std::string family;
    std::string port;
    int baud = 115200;
    std::map<std::string, HardwareValue> settings;
};

// Schema-aware TOML subset: scalar strings, decimal integers and booleans.
// Legacy board metadata, pinout and budget fields remain accepted.
bool readHardwareToml(const std::string& path, HardwareConfig& out, std::string& error);
bool setHardwareValue(HardwareConfig& config, const std::string& assignment, std::string& error);
bool resolveHardware(HardwareConfig& config, const std::string& projectBoard, std::string& error);
std::string hardwareDefines(const HardwareConfig& config);
std::string hardwareReport(const HardwareConfig& config);
std::vector<std::string> hardwareLibraries(const HardwareConfig& config);
int runHardware(int argc, char** argv);
