// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
#include "hardware.h"
#include "project.h"
#include "../vyne/utils/file_utils.h"
#include "../editors/vscode/lsp/backend/include/nlohmann/json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
struct Field { const char* key; const char* define; char type; };
const Field fields[] = {
    {"board.name", "", 's'}, {"board.family", "", 's'},
    {"connection.port", "", 's'}, {"connection.baud", "VYNE_SERIAL_BAUD", 'i'},
    {"pinout.led_builtin", "VYNE_LED_BUILTIN", 'i'},
    {"display.driver", "", 's'}, {"display.width", "TFT_WIDTH", 'i'},
    {"display.height", "TFT_HEIGHT", 'i'}, {"display.rotation", "VYNE_DISPLAY_ROTATION", 'i'},
    {"display.rgb_order", "TFT_RGB_ORDER", 'i'}, {"display.hspi", "USE_HSPI_PORT", 'b'},
    {"display.miso", "TFT_MISO", 'i'}, {"display.mosi", "TFT_MOSI", 'i'},
    {"display.sclk", "TFT_SCLK", 'i'}, {"display.cs", "TFT_CS", 'i'},
    {"display.dc", "TFT_DC", 'i'}, {"display.rst", "TFT_RST", 'i'},
    {"display.backlight", "TFT_BL", 'i'}, {"display.backlight_on", "TFT_BACKLIGHT_ON", 'i'},
    {"display.enable", "VYNE_DISPLAY_ENABLE_PIN", 'i'},
    {"display.mirror_x", "VYNE_DISPLAY_MIRROR_X", 'b'},
    {"display.mirror_y", "VYNE_DISPLAY_MIRROR_Y", 'b'},
    {"display.spi_frequency", "SPI_FREQUENCY", 'i'},
    {"display.spi_read_frequency", "SPI_READ_FREQUENCY", 'i'},
    {"touch.driver", "", 's'}, {"touch.cs", "VYNE_TOUCH_CS", 'i'},
    {"touch.irq", "VYNE_TOUCH_IRQ", 'i'}, {"touch.sclk", "VYNE_TOUCH_SCLK", 'i'},
    {"touch.mosi", "VYNE_TOUCH_MOSI", 'i'}, {"touch.miso", "VYNE_TOUCH_MISO", 'i'},
    {"touch.hspi", "", 'b'},
};
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string scalar(const HardwareValue& v) {
    if (auto s = std::get_if<std::string>(&v)) return *s;
    if (auto i = std::get_if<int>(&v)) return std::to_string(*i);
    return std::get<bool>(v) ? "true" : "false";
}
const Field* field(const std::string& key) {
    for (const auto& f : fields) if (key == f.key) return &f;
    return nullptr;
}
bool assign(HardwareConfig& c, const std::string& key, std::string raw, std::string& error) {
    const auto* f = field(key);
    if (!f && key != "board.preset") {
        // Retain compatibility with the metadata emitted by the original Uno template.
        if (key == "board.mcu" || key == "board.flash_kb" || key == "board.sram_kb" ||
            key == "pinout.adc_0" || key == "budget.flash_kb" || key == "budget.sram_kb") return true;
        error = "unknown hardware setting `" + key + "`"; return false;
    }
    raw = trim(raw);
    const char type = f ? f->type : 's';
    HardwareValue value;
    if (type == 's') {
        if (raw.size() < 2 || (raw.front() != '"' && raw.front() != '\'') || raw.back() != raw.front()) {
            error = "`" + key + "` must be a quoted string"; return false;
        }
        try {
            value = raw.front() == '"' ? json::parse(raw).get<std::string>() : raw.substr(1, raw.size() - 2);
        } catch (...) { error = "invalid string for `" + key + "`"; return false; }
    } else if (type == 'b') {
        if (raw != "true" && raw != "false") { error = "`" + key + "` must be true or false"; return false; }
        value = raw == "true";
    } else {
        int number = 0;
        auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), number);
        if (ec != std::errc{} || end != raw.data() + raw.size()) {
            error = "`" + key + "` must be a decimal integer"; return false;
        }
        value = number;
    }
    if (key == "board.preset") c.preset = std::get<std::string>(value);
    else c.settings[key] = value;
    return true;
}
json presetJson(const std::string& name) {
    if (name.empty()) return json::object();
    // Presets are installation data, never loaded from the application directory.
    if (name != "hw950" && name != "esp32" && name != "esp32s2" &&
        name != "esp32s3" && name != "esp32c3" && name != "esp32c6")
        throw std::runtime_error("unknown preset `" + name + "`; run `vynec hardware list`");
    fs::path exe(FileUtils::getExeDir());
    for (const auto& root : {exe / "share/vyne/hardware", exe.parent_path() / "share/vyne/hardware"}) {
        std::ifstream in(root / (name + ".json"));
        if (in) return json::parse(in);
    }
    throw std::runtime_error("bundled hardware preset missing: " + name + " (reinstall Vyne)");
}
}

bool setHardwareValue(HardwareConfig& c, const std::string& assignment, std::string& error) {
    auto eq = assignment.find('=');
    if (eq == std::string::npos) { error = "expected section.key=value"; return false; }
    return assign(c, trim(assignment.substr(0, eq)), assignment.substr(eq + 1), error);
}

bool readHardwareToml(const std::string& path, HardwareConfig& out, std::string& error) {
    out = {};
    std::ifstream in(path);
    if (!in) { error = "could not open `" + path + "`"; return false; }
    std::string line, section;
    std::set<std::string> seen, tables;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        char quote = 0; bool escaped = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char ch = line[i];
            if (escaped) { escaped = false; continue; }
            if (quote == '"' && ch == '\\') { escaped = true; continue; }
            if (quote) { if (ch == quote) quote = 0; }
            else if (ch == '"' || ch == '\'') quote = ch;
            else if (ch == '#') { line.resize(i); break; }
        }
        line = trim(line);
        if (line.empty()) continue;
        std::string why;
        if (line.front() == '[') {
            if (line.back() != ']' || line.size() < 3) why = "malformed table header";
            else {
                section = trim(line.substr(1, line.size() - 2));
                if (!tables.insert(section).second) why = "duplicate table `" + section + "`";
                else if (section != "board" && section != "connection" && section != "display" &&
                         section != "touch" && section != "pinout" && section != "budget")
                    why = "unknown hardware table `" + section + "`";
            }
        } else {
            const auto eq = line.find('=');
            if (eq == std::string::npos) why = "expected key = value";
            else {
                std::string key = section + "." + trim(line.substr(0, eq));
                if (!seen.insert(key).second) why = "duplicate key `" + key + "`";
                else assign(out, key, line.substr(eq + 1), why);
            }
        }
        if (!why.empty()) { error = path + ":" + std::to_string(number) + ": " + why; return false; }
    }
    return true;
}

bool resolveHardware(HardwareConfig& c, const std::string& projectBoard, std::string& error) {
    try {
        auto overrides = c.settings;
        c.settings.clear();
        auto preset = presetJson(c.preset);
        c.label = preset.value("label", "Custom hardware");
        if (preset.contains("settings")) {
            for (const auto& [key, v] : preset.at("settings").items()) {
                std::string why;
                if (!assign(c, key, v.dump(), why)) throw std::runtime_error("invalid bundled preset: " + why);
            }
        }
        for (const auto& [key, v] : overrides) c.settings[key] = v;
        auto text = [&](const char* key, const std::string& fallback = "") {
            auto it = c.settings.find(key); return it == c.settings.end() ? fallback : scalar(it->second);
        };
        auto integer = [&](const char* key, int fallback) {
            auto it = c.settings.find(key); return it == c.settings.end() ? fallback : std::get<int>(it->second);
        };
        c.fqbn = text("board.name", projectBoard);
        c.family = c.fqbn.starts_with("esp32:") ? "esp32" : c.fqbn.starts_with("arduino:avr:") ? "avr" : "";
        if (c.family.empty()) throw std::runtime_error("hardware requires an ESP32 or Arduino AVR board FQBN");
        if (text("board.family", c.family) != c.family) throw std::runtime_error("board.family is incompatible with board.name");
        if (!c.preset.empty() && c.family != "esp32") throw std::runtime_error("ESP32 preset is incompatible with an AVR target");
        if (c.preset == "hw950" && !c.fqbn.starts_with("esp32:esp32:esp32:" ) && c.fqbn != "esp32:esp32:esp32")
            throw std::runtime_error("HW-950 requires the classic ESP32 target");
        if (c.fqbn.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:,=.-") != std::string::npos)
            throw std::runtime_error("invalid board FQBN");
        c.port = text("connection.port"); c.baud = integer("connection.baud", 115200);
        if (c.port.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./\\:-") != std::string::npos)
            throw std::runtime_error("connection.port must be a serial device name (COM5 or /dev/ttyUSB0)");
        if (c.baud <= 0 || c.baud > 4000000) throw std::runtime_error("connection.baud must be in 1..4000000");
        auto driver = text("display.driver", "none"), touch = text("touch.driver", "none");
        if (driver != "none" && driver != "st7796" && driver != "ili9341" && driver != "ili9488")
            throw std::runtime_error("unsupported display.driver (none, st7796, ili9341, ili9488)");
        if (touch != "none" && touch != "xpt2046") throw std::runtime_error("unsupported touch.driver (none, xpt2046)");
        if (c.family != "esp32" && (driver != "none" || touch != "none")) throw std::runtime_error("display/touch drivers require ESP32");
        if (touch != "none" && driver == "none") throw std::runtime_error("touch requires a configured display");
        for (const auto& [key, value] : c.settings) {
            if (!std::holds_alternative<int>(value)) continue;
            int n = std::get<int>(value);
            const auto* f = field(key);
            std::string def = f ? f->define : "";
            bool pin = def == "VYNE_LED_BUILTIN" || def == "TFT_MISO" || def == "TFT_MOSI" ||
                def == "TFT_SCLK" || def == "TFT_CS" || def == "TFT_DC" || def == "TFT_RST" ||
                def == "TFT_BL" || def == "VYNE_DISPLAY_ENABLE_PIN" || key.starts_with("touch.");
            int maxPin = c.family == "avr" ? 69 : c.fqbn.starts_with("esp32:esp32:esp32s3") ? 48 :
                c.fqbn.starts_with("esp32:esp32:esp32c3") ? 21 : c.fqbn.starts_with("esp32:esp32:esp32c6") ? 30 :
                c.fqbn.starts_with("esp32:esp32:esp32s2") ? 46 : 39;
            if (pin && (n < -1 || n > maxPin)) throw std::runtime_error(key + " is outside the board GPIO range");
            if (pin && maxPin == 39 && (n == 20 || n == 24 || (n >= 28 && n <= 31)))
                throw std::runtime_error(key + " selects a GPIO absent on classic ESP32");
            if (pin && maxPin == 39 && n >= 6 && n <= 11)
                throw std::runtime_error(key + " selects an ESP32 flash-reserved GPIO");
            if (pin && n >= 34 && maxPin == 39 && key != "touch.irq" && !key.ends_with("miso"))
                throw std::runtime_error(key + " selects an input-only ESP32 GPIO");
            if ((key == "display.width" || key == "display.height") && (n < 1 || n > 1024))
                throw std::runtime_error(key + " must be in 1..1024");
            if (key == "display.rotation" && (n < 0 || n > 3)) throw std::runtime_error("display.rotation must be in 0..3");
            if ((key == "display.rgb_order" || key == "display.backlight_on") && (n < 0 || n > 1))
                throw std::runtime_error(key + " must be 0 or 1");
            if (key.ends_with("frequency") && (n < 100000 || n > 80000000)) throw std::runtime_error(key + " must be in 100000..80000000");
        }
        if (driver != "none") {
            for (const auto* key : {"display.width", "display.height", "display.mosi", "display.sclk", "display.cs", "display.dc"})
                if (integer(key, -1) < 0) throw std::runtime_error(std::string("display requires ") + key);
            if (driver == "ili9341" && (integer("display.width", 0) != 240 || integer("display.height", 0) != 320))
                throw std::runtime_error("ILI9341 requires native 240x320 dimensions (use rotation for landscape)");
            if (driver == "ili9488" && (integer("display.width", 0) != 320 || integer("display.height", 0) != 480))
                throw std::runtime_error("ILI9488 requires native 320x480 dimensions");
            if (driver == "st7796" && !((integer("display.width", 0) == 240 && integer("display.height", 0) == 320) ||
                                       (integer("display.width", 0) == 320 && integer("display.height", 0) == 480)))
                throw std::runtime_error("ST7796 supports measured 240x320 or native 320x480 dimensions");
        }
        if (touch != "none") {
            for (const auto* key : {"touch.cs", "touch.sclk", "touch.mosi", "touch.miso"})
                if (integer(key, -1) < 0) throw std::runtime_error(std::string("touch requires ") + key);
            if (integer("touch.cs", -1) == integer("display.cs", -2)) throw std::runtime_error("display and touch chip-select pins must differ");
        }
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}

std::string hardwareDefines(const HardwareConfig& c) {
    std::ostringstream out;
    auto define = [&](const std::string& name, const std::string& value) { out << "-U" << name << "\n-D" << name << "=" << value << "\n"; };
    for (const auto& [key, v] : c.settings) {
        const auto* f = field(key);
        if (!f || !*f->define) continue;
        if (key == "display.hspi" && !std::get<bool>(v)) {
            out << "-UUSE_HSPI_PORT\n";
            continue;
        }
        define(f->define, std::holds_alternative<bool>(v) ? (std::get<bool>(v) ? "1" : "0") : scalar(v));
    }
    auto get = [&](const char* key, const char* fallback) {
        auto it = c.settings.find(key); return it == c.settings.end() ? std::string(fallback) : scalar(it->second);
    };
    if (get("display.driver", "none") != "none") {
        define("VYNE_PLOT_DISPLAY_CONFIGURED", "1");
        define("USER_SETUP_LOADED", "1");
        for (const auto* d : {"ST7789_DRIVER", "ST7796_DRIVER", "ILI9341_DRIVER", "ILI9341_2_DRIVER", "ILI9488_DRIVER"}) out << "-U" << d << "\n";
        const auto driver = get("display.driver", "none");
        define(driver == "st7796" ? "ST7796_DRIVER" : driver == "ili9341" ? "ILI9341_DRIVER" : "ILI9488_DRIVER", "1");
        for (const auto* font : {"LOAD_GLCD", "LOAD_FONT2", "LOAD_FONT4", "LOAD_GFXFF"}) define(font, "1");
    } else define("VYNE_PLOT_DISPLAY_CONFIGURED", "0");
    if (get("touch.driver", "none") == "xpt2046") {
        define("VYNE_TOUCH_XPT2046", "1");
        define("VYNE_TOUCH_SPI_PORT", get("touch.hspi", "false") == "true" ? "HSPI" : "VSPI");
    }
    return out.str();
}

std::string hardwareReport(const HardwareConfig& c) {
    std::ostringstream out;
    out << "# " << c.label << "\n[board]\npreset = " << json(c.preset).dump()
        << "\nname = " << json(c.fqbn).dump() << "\nfamily = " << json(c.family).dump()
        << "\n\n[connection]\nport = " << json(c.port).dump() << "\nbaud = " << c.baud << "\n";
    std::string section;
    for (const auto& [key, v] : c.settings) {
        if (key.starts_with("board.") || key.starts_with("connection.")) continue;
        auto dot = key.find('.'); auto next = key.substr(0, dot);
        if (section != next) { section = next; out << "\n[" << section << "]\n"; }
        out << key.substr(dot + 1) << " = " << (std::holds_alternative<std::string>(v) ? json(scalar(v)).dump() : scalar(v)) << "\n";
    }
    return out.str();
}

std::vector<std::string> hardwareLibraries(const HardwareConfig& c) {
    auto it = c.settings.find("display.driver");
    return it != c.settings.end() && scalar(it->second) != "none" ? std::vector<std::string>{"TFT_eSPI"} : std::vector<std::string>{};
}

int runHardware(int argc, char** argv) {
    std::string action = argc > 2 ? argv[2] : "show";
    if (action == "list") {
        try {
            for (const auto* name : {"esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "hw950"})
                std::cout << name << "  " << presetJson(name).at("label").get<std::string>() << "\n";
            return 0;
        } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 1; }
    }
    if (action != "show" && action != "setup") {
        std::cerr << "Usage: vynec hardware [list|show|setup] [--preset NAME] [--hardware-set section.key=value]\n"; return 1;
    }
    auto project = loadProject(".");
    HardwareConfig config; std::string error;
    if (project.found && !project.ok) { std::cerr << project.error << "\n"; return 1; }
    fs::path path = project.found ? fs::path(project.config.root_dir) / "hardware.toml" : fs::path("hardware.toml");
    if (fs::exists(path) && !readHardwareToml(path.string(), config, error)) { std::cerr << error << "\n"; return 1; }
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--preset" && i + 1 < argc) config.preset = argv[++i];
        else if (arg == "--hardware-set" && i + 1 < argc) {
            if (!setHardwareValue(config, argv[++i], error)) { std::cerr << error << "\n"; return 1; }
        } else { std::cerr << "unknown or incomplete hardware option: " << arg << "\n"; return 1; }
    }
    if (!resolveHardware(config, project.config.board, error)) { std::cerr << error << "\n"; return 1; }
    if (action == "show") { std::cout << hardwareReport(config); return 0; }
    const std::string core = config.family == "esp32" ? "esp32:esp32" : "arduino:avr";
    std::cout << "Installing " << core << " and required hardware libraries...\n";
    std::string index = config.family == "esp32" ? " --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json" : "";
    if (std::system(("arduino-cli core update-index" + index).c_str()) != 0 ||
        std::system(("arduino-cli core install " + core + index).c_str()) != 0) {
        std::cerr << "Core setup failed. Install arduino-cli, check network access, then retry vynec hardware setup.\n"; return 1;
    }
    for (const auto& lib : hardwareLibraries(config))
        if (std::system(("arduino-cli lib install " + lib).c_str()) != 0) { std::cerr << "Library setup failed: " << lib << "\n"; return 1; }
    std::cout << "Hardware ready. Run vynec build, then vynec run.\n";
    return 0;
}
