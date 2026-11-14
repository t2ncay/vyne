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

#define FTXUI_IMPLEMENTATION
#include "ftxui_all.hpp"

#include "tui.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <sys/ioctl.h>
#    include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace ftxui;

namespace {

const Color kBrand = Color::RGB(124, 58, 237);
const Color kPanel = Color::RGB(26, 26, 26);
const Color kMuted = Color::RGB(140, 140, 140);
const Color kText = Color::RGB(230, 230, 230);
const Color kGreen = Color::RGB(74, 222, 128);
const Color kRed = Color::RGB(248, 113, 113);
const Color kYellow = Color::RGB(250, 204, 21);

struct KindOption {
    const char* value;
    const char* label;
    const char* desc;
};

const KindOption kKinds[] = {
    {"ml", "ML training", "vml + vlin + vstat + vtermplot"},
    {"embedded", "Embedded", "arduino + @bounded"},
    {"scientific", "Scientific", "vlin + vfft + vstat"},
    {"sr", "Image / Video", "vimage + vconv"},
    {"mixed", "Mixed", "all of the above"},
};
constexpr int kKindCount = 5;

struct TargetOption {
    const char* value;
    const char* label;
};

const TargetOption kTargets[] = {
    {"desktop", "Desktop (C backend)"},
    {"arduino-uno", "Arduino Uno R3"},
    {"coral", "Coral Edge TPU"},
    {"jetson", "Jetson Nano"},
    {"esp32", "ESP32 family: generic ESP32 Dev Module"},
    {"esp32s2", "ESP32 family: generic ESP32-S2"},
    {"esp32s3", "ESP32 family: generic ESP32-S3"},
    {"esp32c3", "ESP32 family: generic ESP32-C3"},
    {"esp32c6", "ESP32 family: generic ESP32-C6"},
    {"hw950", "ESP32 HW-950: measured ST7796 240x320 + XPT2046"},
};
constexpr int kTargetCount = 10;

struct LibOption {
    const char* value;
    const char* label;
    const char* desc;
};

const LibOption kLibs[] = {
    {"vlin", "vlin", "Linear algebra"},
    {"vml", "vml", "Neural network primitives"},
    {"vstat", "vstat", "Statistics and metrics"},
    {"vtermplot", "vtermplot", "Terminal plotting"},
    {"vplot", "vplot", "Universal colorful charts and live plots"},
    {"vfft", "vfft", "FFT and spectral"},
    {"vcomplex", "vcomplex", "Complex numbers"},
    {"vcsv", "vcsv", "CSV loading"},
    {"vimage", "vimage", "Image I/O"},
    {"vconv", "vconv", "Convolution"},
};
constexpr int kLibCount = 10;

const char* const kLicenses[] = {
    "AGPL-3.0-only",
    "MIT",
    "proprietary",
};
const char* const kLicenseLabels[] = {
    "AGPL-3.0-only   (Vyne compiler default)",
    "MIT",
    "Proprietary     (no LICENSE; writes LICENSE_HEADER.txt)",
};
constexpr int kLicenseCount = 3;

bool terminalSize(int& cols, int& rows) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(handle, &info))
        return false;
    cols = info.srWindow.Right - info.srWindow.Left + 1;
    rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return cols > 0 && rows > 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
        return false;
    cols = ws.ws_col;
    rows = ws.ws_row;
    return cols > 0 && rows > 0;
#endif
}

bool hasNvcc() {
#ifdef _WIN32
    return std::system("nvcc --version > NUL 2>&1") == 0;
#else
    return std::system("nvcc --version > /dev/null 2>&1") == 0;
#endif
}

std::string join(const std::vector<std::string>& values, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            out += sep;
        out += values[i];
    }
    return out;
}

const KindOption& kindByValue(const std::string& value) {
    for (const auto& kind : kKinds) {
        if (value == kind.value)
            return kind;
    }
    return kKinds[0];
}

std::string kindLabel(const std::string& value) {
    return kindByValue(value).label;
}

const char* targetValue(int index) {
    return kTargets[index].value;
}

const char* libValue(int index) {
    return kLibs[index].value;
}

std::vector<std::string> defaultTargets(const std::string& kind) {
    if (kind == "ml")
        return {"desktop", "coral"};
    if (kind == "embedded")
        return {"arduino-uno"};
    if (kind == "scientific")
        return {"desktop"};
    if (kind == "sr")
        return {"desktop", "jetson"};
    if (kind == "mixed")
        return {"desktop", "arduino-uno", "coral", "jetson"};
    return {"desktop"};
}

std::vector<std::string> defaultLibraries(const std::string& kind) {
    if (kind == "ml")
        return {"vlin", "vml", "vstat", "vtermplot"};
    if (kind == "embedded")
        return {};
    if (kind == "scientific")
        return {"vlin", "vfft", "vstat", "vcomplex", "vtermplot"};
    if (kind == "sr")
        return {"vimage", "vconv", "vfft"};
    if (kind == "mixed")
        return {"vlin", "vml", "vstat", "vtermplot", "vfft",
                "vcomplex", "vcsv", "vimage", "vconv"};
    return {};
}

class Questionnaire {
public:
    Questionnaire(
        InitChoices& choices,
        fs::path shareDir,
        std::string projectName,
        std::set<std::string> locked,
        ScreenInteractive& screen
    )
        : choices_(choices),
          shareDir_(std::move(shareDir)),
          projectName_(std::move(projectName)),
          locked_(std::move(locked)),
          screen_(screen),
          targetSel_(kTargetCount, false),
          libSel_(kLibCount, false) {
        for (int i = 0; i < kKindCount; ++i) {
            if (choices_.kind == kKinds[i].value)
                kindIndex_ = i;
        }
        licenseIndex_ = indexOfLicense(choices_.license);
        cudaIndex_ = choices_.cuda ? 0 : 1;

        if (locked_.count("targets")) {
            for (const auto& value : choices_.targets) {
                for (int i = 0; i < kTargetCount; ++i)
                    if (value == kTargets[i].value)
                        targetSel_[i] = true;
            }
        } else if (!choices_.targets.empty()) {
            for (const auto& value : choices_.targets) {
                for (int i = 0; i < kTargetCount; ++i)
                    if (value == kTargets[i].value)
                        targetSel_[i] = true;
            }
        } else {
            applyTargetDefaults(kKinds[kindIndex_].value);
        }

        if (locked_.count("libraries")) {
            for (const auto& value : choices_.libraries) {
                for (int i = 0; i < kLibCount; ++i)
                    if (value == kLibs[i].value)
                        libSel_[i] = true;
            }
        } else if (!choices_.libraries.empty()) {
            for (const auto& value : choices_.libraries) {
                for (int i = 0; i < kLibCount; ++i)
                    if (value == kLibs[i].value)
                        libSel_[i] = true;
            }
        } else {
            applyLibraryDefaults(kKinds[kindIndex_].value);
        }

        step_ = firstVisible(0);
        cursor_ = cursorForStep(step_);
        syncChoices();
    }

    Element render() {
        if ((screen_.dimx() > 0 && screen_.dimx() < 80) ||
            (screen_.dimy() > 0 && screen_.dimy() < 24)) {
            tooSmall_ = true;
            return vbox({
                       text("Terminal too small") | bold | color(kRed),
                       text("vynec init needs at least 80x24.") | color(kMuted),
                       text("Please enlarge the terminal and try again.") | color(kMuted),
                   }) |
                   center;
        }
        tooSmall_ = false;

        Element body = renderBody();
        Element panel = vbox({
                            renderHeader(),
                            separator(),
                            body,
                            separator(),
                            renderFooter(),
                        }) |
                        bgcolor(kPanel) | color(kText) | borderRounded | center;

        return panel;
    }

    bool onEvent(Event event) {
        if (tooSmall_) {
            error_ = "terminal too small; minimum is 80x24";
            cancelled_ = true;
            screen_.Exit();
            return true;
        }

        if (event == Event::CtrlC) {
            cancelled_ = true;
            screen_.Exit();
            return true;
        }

        if (event == Event::Escape) {
            if (step_ == 0) {
                cancelled_ = true;
                screen_.Exit();
                return true;
            }
            if (step_ == 5) {
                int prev = prevVisible(5);
                if (prev == 0 && !stepVisible(0)) {
                    cancelled_ = true;
                    screen_.Exit();
                    return true;
                }
                step_ = prev;
                cursor_ = cursorForStep(step_);
                return true;
            }
            int prev = prevVisible(step_);
            if (prev == 0 && !stepVisible(0)) {
                cancelled_ = true;
                screen_.Exit();
                return true;
            }
            step_ = prev;
            cursor_ = cursorForStep(step_);
            error_.clear();
            return true;
        }

        if (step_ == 5) {
            if (event == Event::ArrowLeft || event == Event::ArrowUp) {
                cursor_ = (cursor_ + 2) % 3;
                return true;
            }
            if (event == Event::ArrowRight || event == Event::ArrowDown) {
                cursor_ = (cursor_ + 1) % 3;
                return true;
            }
            if (event == Event::Return || event == Event::Character(' ')) {
                if (cursor_ == 0) {
                    confirmed_ = true;
                    screen_.Exit();
                } else if (cursor_ == 1) {
                    step_ = prevVisible(5);
                    cursor_ = cursorForStep(step_);
                } else {
                    cancelled_ = true;
                    screen_.Exit();
                }
                return true;
            }
            return false;
        }

        int count = optionCount(step_);
        if (event == Event::ArrowUp || event == Event::ArrowLeft) {
            cursor_ = (cursor_ + count - 1) % count;
            error_.clear();
            return true;
        }
        if (event == Event::ArrowDown || event == Event::ArrowRight) {
            cursor_ = (cursor_ + 1) % count;
            error_.clear();
            return true;
        }

        if (event == Event::Character(' ')) {
            if (step_ == 1) {
                targetSel_[cursor_] = !targetSel_[cursor_];
                error_.clear();
                return true;
            }
            if (step_ == 2) {
                libSel_[cursor_] = !libSel_[cursor_];
                error_.clear();
                return true;
            }
            return confirmStep();
        }

        if (event == Event::Return)
            return confirmStep();

        return false;
    }

    bool cancelled() const { return cancelled_; }
    bool confirmed() const { return confirmed_; }
    const std::string& error() const { return error_; }

private:
    InitChoices& choices_;
    fs::path shareDir_;
    std::string projectName_;
    std::set<std::string> locked_;
    ScreenInteractive& screen_;

    std::vector<bool> targetSel_;
    std::vector<bool> libSel_;
    int kindIndex_ = 0;
    int cudaIndex_ = 0;
    int licenseIndex_ = 0;
    int cursor_ = 0;
    int step_ = 0;
    std::string error_;
    bool cancelled_ = false;
    bool confirmed_ = false;
    bool tooSmall_ = false;
    mutable bool nvccChecked_ = false;
    mutable bool nvccFound_ = false;

    static int indexOfLicense(const std::string& value) {
        for (int i = 0; i < kLicenseCount; ++i) {
            if (value == kLicenses[i])
                return i;
        }
        return 0;
    }

    std::string kindValue() const { return kKinds[kindIndex_].value; }

    bool hasDesktop() const { return targetSel_[0]; }

    bool cudaApplicable() const {
        return hasDesktop() && kindValue() != "embedded";
    }

    bool nvccAvailable() const {
        if (!nvccChecked_) {
            nvccFound_ = hasNvcc();
            nvccChecked_ = true;
        }
        return nvccFound_;
    }

    bool stepVisible(int step) const {
        switch (step) {
            case 0: return !locked_.count("kind");
            case 1: return !locked_.count("targets");
            case 2: return !locked_.count("libraries");
            case 3: return !locked_.count("cuda") && cudaApplicable();
            case 4: return !locked_.count("license");
            case 5: return true;
            default: return false;
        }
    }

    int firstVisible(int start) const {
        int s = start;
        while (s < 5 && !stepVisible(s))
            ++s;
        return s;
    }

    int nextVisible(int step) const {
        int s = step + 1;
        while (s < 5 && !stepVisible(s))
            ++s;
        return s > 5 ? 5 : s;
    }

    int prevVisible(int step) const {
        int s = step - 1;
        while (s >= 0 && !stepVisible(s))
            --s;
        return s < 0 ? 0 : s;
    }

    int optionCount(int step) const {
        if (step == 0) return kKindCount;
        if (step == 1) return kTargetCount;
        if (step == 2) return kLibCount;
        if (step == 3) return 2;
        if (step == 4) return kLicenseCount;
        return 3;
    }

    int cursorForStep(int step) const {
        if (step == 0) return kindIndex_;
        if (step == 3) return cudaIndex_;
        if (step == 4) return licenseIndex_;
        if (step == 5) return 0;
        return 0;
    }

    void applyTargetDefaults(const std::string& kind) {
        if (locked_.count("targets"))
            return;
        targetSel_.assign(kTargetCount, false);
        for (const auto& value : defaultTargets(kind)) {
            for (int i = 0; i < kTargetCount; ++i)
                if (value == kTargets[i].value)
                    targetSel_[i] = true;
        }
    }

    void applyLibraryDefaults(const std::string& kind) {
        if (locked_.count("libraries"))
            return;
        libSel_.assign(kLibCount, false);
        for (const auto& value : defaultLibraries(kind)) {
            for (int i = 0; i < kLibCount; ++i)
                if (value == kLibs[i].value)
                    libSel_[i] = true;
        }
    }

    void syncChoices() {
        choices_.kind = kindValue();
        choices_.targets.clear();
        for (int i = 0; i < kTargetCount; ++i)
            if (targetSel_[i])
                choices_.targets.push_back(kTargets[i].value);
        choices_.libraries.clear();
        for (int i = 0; i < kLibCount; ++i)
            if (libSel_[i])
                choices_.libraries.push_back(kLibs[i].value);
        choices_.cuda = cudaApplicable() && cudaIndex_ == 0;
        choices_.license = kLicenses[licenseIndex_];
    }

    bool confirmStep() {
        error_.clear();

        if (step_ == 0) {
            kindIndex_ = cursor_;
            applyTargetDefaults(kindValue());
            applyLibraryDefaults(kindValue());
        } else if (step_ == 1) {
            bool any = false;
            for (bool selected : targetSel_)
                any = any || selected;
            if (!any) {
                error_ = "Select at least one target.";
                return true;
            }
        } else if (step_ == 3) {
            cudaIndex_ = cursor_;
        } else if (step_ == 4) {
            licenseIndex_ = cursor_;
        }

        syncChoices();
        step_ = nextVisible(step_);
        cursor_ = cursorForStep(step_);
        return true;
    }

    std::string breadcrumb() const {
        std::string out = "project";
        if (step_ > 0 || locked_.count("kind"))
            out += "  \u2192  " + std::string(kKinds[kindIndex_].label);

        std::vector<std::string> targets;
        for (int i = 0; i < kTargetCount; ++i)
            if (targetSel_[i])
                targets.push_back(kTargets[i].value);
        if ((step_ > 1 || locked_.count("targets")) && !targets.empty()) {
            out += "  \u2192  ";
            if (targets.size() == 1)
                out += targets[0];
            else
                out += targets[0] + "+" + std::to_string(targets.size() - 1);
        }

        if ((step_ > 2 || locked_.count("libraries"))) {
            int selected = 0;
            int first = -1;
            for (int i = 0; i < kLibCount; ++i) {
                if (libSel_[i]) {
                    ++selected;
                    if (first < 0)
                        first = i;
                }
            }
            if (selected > 0) {
                out += "  \u2192  " + std::string(kLibs[first].value);
                if (selected > 1)
                    out += "+" + std::to_string(selected - 1);
            } else {
                out += "  \u2192  no libs";
            }
        }

        if ((step_ > 3 || locked_.count("cuda")) && cudaApplicable())
            out += cudaIndex_ == 0 ? "  \u2192  cuda" : "  \u2192  cpu";

        if (step_ > 4 || locked_.count("license"))
            out += "  \u2192  " + std::string(kLicenses[licenseIndex_]);

        return out;
    }

    std::string progressText() const {
        if (step_ == 5)
            return "confirm";
        int shown = step_ + 1;
        if (step_ == 4 && !cudaApplicable())
            shown = 4;
        return "[" + std::to_string(shown) + "/5]";
    }

    std::string footerText() const {
        if (step_ == 0)
            return "← → select   Enter confirm   Esc cancel";
        if (step_ == 1 || step_ == 2)
            return "↑↓ navigate   Space toggle   Enter confirm   Esc back";
        if (step_ == 5)
            return "← → choose   Enter activate   Esc back";
        return "← → select   Enter confirm   Esc back";
    }

    Element renderHeader() const {
        Element left = hbox({
            text("vynec init") | bold | color(kBrand),
            text("   " + breadcrumb()) | color(kMuted),
        });
        return hbox({left, filler(), text(progressText()) | color(kMuted)});
    }

    Element renderFooter() const {
        return text(footerText()) | color(kMuted);
    }

    Element questionHeader(const std::string& title, const std::string& help) const {
        return vbox({
            text(title) | bold,
            text(help) | color(kMuted),
            text(""),
        });
    }

    Element errorLine() const {
        if (error_.empty())
            return text("");
        return text("✗ " + error_) | color(kRed);
    }

    Element singleOptions(
        const std::string& title,
        const std::string& help,
        const std::vector<std::string>& labels,
        const std::vector<std::string>* descs,
        int selected
    ) const {
        std::vector<Element> rows;
        for (size_t i = 0; i < labels.size(); ++i) {
            bool current = ((int)i == cursor_);
            Element marker = text(i == (size_t)selected ? "● " : "○ ") | color(kBrand);
            Element label = text(labels[i]) | bold;
            Element row;
            if (descs && !(*descs)[i].empty()) {
                row = hbox({
                    marker,
                    label,
                    filler(),
                    text((*descs)[i] + "  ") | color(kMuted),
                });
            } else {
                row = hbox({marker, label});
            }
            if (current)
                row = row | bgcolor(kBrand) | color(Color::White);
            rows.push_back(row);
        }

        Element body = vbox({
            questionHeader(title, help),
            vbox(rows),
            errorLine(),
        });
        if (!error_.empty())
            body = body | borderStyled(ROUNDED, kRed);
        return body;
    }

    Element multiOptions(
        const std::string& title,
        const std::string& help,
        const std::vector<std::string>& labels,
        const std::vector<std::string>& descs,
        const std::vector<bool>& selected
    ) const {
        std::vector<Element> rows;
        for (size_t i = 0; i < labels.size(); ++i) {
            bool current = ((int)i == cursor_);
            Element marker = text(selected[i] ? "[×] " : "[ ] ") |
                             color(selected[i] ? kBrand : kMuted);
            Element label = text(labels[i]) | bold;
            Element row = hbox({
                marker,
                label,
                filler(),
                text(descs[i] + "  ") | color(kMuted),
            });
            if (current)
                row = row | bgcolor(kBrand) | color(Color::White);
            rows.push_back(row);
        }

        Element body = vbox({
            questionHeader(title, help),
            vbox(rows),
            errorLine(),
        });
        if (!error_.empty())
            body = body | borderStyled(ROUNDED, kRed);
        return body;
    }

    Element renderKind() const {
        std::vector<std::string> labels;
        std::vector<std::string> descs;
        for (const auto& kind : kKinds) {
            labels.push_back(kind.label);
            descs.push_back(kind.desc);
        }
        return singleOptions(
            "What kind of project?",
            "This determines which libraries and template you get.",
            labels,
            &descs,
            kindIndex_
        );
    }

    Element renderTargets() const {
        std::vector<std::string> labels;
        std::vector<std::string> descs;
        for (const auto& target : kTargets) {
            labels.push_back(target.label);
            descs.push_back("");
        }
        return multiOptions(
            "Which targets?",
            "Your project can build for multiple targets.",
            labels,
            descs,
            targetSel_
        );
    }

    Element renderLibraries() const {
        std::vector<std::string> labels;
        std::vector<std::string> descs;
        for (const auto& lib : kLibs) {
            labels.push_back(lib.label);
            descs.push_back(lib.desc);
        }
        return multiOptions(
            "Which libraries?",
            "Recommended libraries are pre-selected.",
            labels,
            descs,
            libSel_
        );
    }

    Element renderCuda() const {
        std::vector<std::string> labels = {
            "Yes - detect nvcc, add [device.cuda]",
            "No",
        };
        std::vector<std::string> descs = {"", ""};
        Element body = singleOptions(
            "Need CUDA for GPU acceleration?",
            "Required for @device regions. Detects nvcc automatically.",
            labels,
            &descs,
            cudaIndex_
        );
        if (cudaIndex_ == 0 && !nvccAvailable()) {
            body = vbox({
                body,
                text("⚠ nvcc not found - CUDA will be configured but not buildable") |
                    color(kYellow),
            });
        }
        return body;
    }

    Element renderLicense() const {
        std::vector<std::string> labels;
        for (const char* label : kLicenseLabels)
            labels.push_back(label);
        return singleOptions(
            "Which license?",
            "AGPL-3.0-only is the default for compiler-linked code.",
            labels,
            nullptr,
            licenseIndex_
        );
    }

    std::vector<std::string> expectedFiles() const {
        std::string kind = kindValue();
        if (kind == "mixed")
            kind = "ml";

        std::vector<std::string> files;
        files.push_back("project.toml");

        fs::path templateDir = shareDir_ / "templates" / kind;
        std::error_code ec;
        if (fs::is_directory(templateDir, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(templateDir, ec)) {
                if (ec)
                    break;
                if (!entry.is_regular_file())
                    continue;
                files.push_back(fs::relative(entry.path(), templateDir).generic_string());
            }
        }

        files.push_back(kLicenses[licenseIndex_] == std::string("proprietary")
                            ? "LICENSE_HEADER.txt"
                            : "LICENSE");
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
        return files;
    }

    Element renderFileList() const {
        std::vector<std::string> files = expectedFiles();
        size_t half = (files.size() + 1) / 2;

        std::vector<Element> left;
        std::vector<Element> right;
        for (size_t i = 0; i < files.size(); ++i) {
            Element line = text("  " + files[i]) | color(kMuted);
            if (i < half)
                left.push_back(line);
            else
                right.push_back(line);
        }
        while (left.size() < right.size())
            left.push_back(text(""));
        while (right.size() < left.size())
            right.push_back(text(""));

        return hbox({
            vbox(left) | flex,
            vbox(right) | flex,
        });
    }

    Element renderConfirm() const {
        std::vector<std::string> targets;
        for (int i = 0; i < kTargetCount; ++i)
            if (targetSel_[i])
                targets.push_back(kTargets[i].value);

        std::vector<std::string> libs;
        for (int i = 0; i < kLibCount; ++i)
            if (libSel_[i])
                libs.push_back(kLibs[i].value);

        std::string cuda = cudaApplicable() ? (cudaIndex_ == 0 ? "Yes" : "No") : "n/a";

        auto line = [](const std::string& key, const std::string& value) {
            return hbox({
                text(key) | color(kMuted),
                text(value),
            });
        };

        std::vector<Element> buttons;
        const char* buttonLabels[] = {
            "  [ Create project ]  ",
            "  [ Go back ]  ",
            "  [ Cancel ]  ",
        };
        for (int i = 0; i < 3; ++i) {
            Element button = text(buttonLabels[i]) | color(kText);
            if (cursor_ == i)
                button = button | bgcolor(kBrand) | color(Color::White) | bold;
            buttons.push_back(button);
        }

        Element body = vbox({
            text("Create project") | bold,
            text(""),
            line("Project:    ", projectName_),
            line("Kind:       ", kKinds[kindIndex_].label),
            line("Targets:    ", join(targets, ", ")),
            line("Libraries:  ", libs.empty() ? "none" : join(libs, ", ")),
            line("CUDA:       ", cuda),
            line("License:    ", kLicenses[licenseIndex_]),
            text(""),
            text("Files to create:") | bold,
            renderFileList(),
            text(""),
            hbox({buttons[0], text("  "), buttons[1], text("  "), buttons[2]}) | center,
        });

        return body;
    }

    Element renderBody() const {
        switch (step_) {
            case 0: return renderKind();
            case 1: return renderTargets();
            case 2: return renderLibraries();
            case 3: return renderCuda();
            case 4: return renderLicense();
            case 5: return renderConfirm();
            default: return text("");
        }
    }
};

} // namespace

bool questionnaireSelfTest(std::string& error) {
    auto screen = ScreenInteractive::FixedSize(100, 40);
    const std::set<std::string> locked = {"kind", "libraries", "cuda", "license"};
    for (int index = 4; index < kTargetCount; ++index) {
        InitChoices choices;
        choices.kind = "embedded";
        choices.targets = {"arduino-uno"};
        Questionnaire q(choices, {}, "hardware_test", locked, screen);
        q.onEvent(Event::ArrowDown); // Uno
        q.onEvent(Event::Character(' ')); // deselect the default
        for (int i = 1; i < index; ++i) q.onEvent(Event::ArrowDown);
        q.onEvent(Event::Character(' '));
        q.onEvent(Event::Return);
        if (choices.targets != std::vector<std::string>{kTargets[index].value}) {
            error = "interactive board selection failed"; return false;
        }
        q.onEvent(Event::Escape); // back from confirmation
        q.onEvent(Event::Return);
        q.onEvent(Event::Return); // confirm
        if (!q.confirmed()) { error = "interactive confirmation failed"; return false; }
    }
    InitChoices empty;
    empty.kind = "embedded";
    empty.targets = {"arduino-uno"};
    Questionnaire q(empty, {}, "empty_test", locked, screen);
    q.onEvent(Event::ArrowDown);
    q.onEvent(Event::Character(' '));
    q.onEvent(Event::Return);
    if (q.error().empty()) { error = "empty interactive target selection accepted"; return false; }
    q.onEvent(Event::CtrlC);
    if (!q.cancelled()) { error = "interactive cancellation failed"; return false; }
    return true;
}

bool runQuestionnaire(
    InitChoices& choices,
    const std::filesystem::path& shareDir,
    const std::string& projectName,
    const std::set<std::string>& locked,
    std::string& error,
    bool& cancelled
) {
    int cols = 0;
    int rows = 0;
    if (!terminalSize(cols, rows)) {
        error = "could not determine terminal size";
        cancelled = true;
        return false;
    }
    if (cols < 80 || rows < 24) {
        error = "terminal too small; vynec init needs at least 80x24";
        cancelled = true;
        return false;
    }

    auto screen = ScreenInteractive::Fullscreen();
    Questionnaire questionnaire(choices, shareDir, projectName, locked, screen);

    auto component = Renderer([&] { return questionnaire.render(); }) |
                     CatchEvent([&](Event event) {
                         return questionnaire.onEvent(event);
                     });

    screen.Loop(component);

    error = questionnaire.error();
    cancelled = questionnaire.cancelled();
    return questionnaire.confirmed();
}
