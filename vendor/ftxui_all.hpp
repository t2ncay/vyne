// Copyright 2026 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// FTXUI Amalgamated Version 7.1.0 (fa2ae5b)
//
// Usage (Header-only version):
// 1. Copy ftxui_all.hpp into your project.
// 2. In EXACTLY ONE C++ file, define FTXUI_IMPLEMENTATION before including:
//    #define FTXUI_IMPLEMENTATION
//    #include "ftxui_all.hpp"
// 3. In other files, just #include "ftxui_all.hpp".
// 4. Ensure you are using C++17 and link against threads (-lpthread).
// 5. On Windows/MSVC, use /utf-8 and define UNICODE / _UNICODE and NOMINMAX.

#ifndef FTXUI_AMALGAMATED_HPP
#define FTXUI_AMALGAMATED_HPP

// On Windows, FTXUI requires UNICODE and NOMINMAX to be defined.
#if defined(_WIN32)
  #if !defined(UNICODE)
    #error "FTXUI requires UNICODE to be defined on Windows. Please add /DUNICODE /D_UNICODE to your compiler flags."
  #endif
  #if !defined(NOMINMAX)
    #error "FTXUI requires NOMINMAX to be defined on Windows. Please add /DNOMINMAX to your compiler flags."
  #endif
#endif

// Marker for detection
#define FTXUI_AMALGAMATED
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__sun) || defined(__illumos__)
#include <sys/termios.h>
#endif

// In an amalgamated build, we don't want any export/import annotations.
#if defined(FTXUI_AMALGAMATED)
#define FTXUI_EXPORT(component)
#define INSIDE_FTXUI_COMPONENT_IMPL(component) 0
#else

// Used to annotate symbols which are exported by the component named
// |component|. Note that this only does the right thing if the corresponding
// component target's sources are compiled with |IS_FTXUI_$component_IMPL|
// defined as 1. For example:
//
//   class FTXUI_EXPORT(FOO) Bar {};
//
// If IS_FTXUI_FOO_IMPL=1 at compile time, then Bar will be annotated using the
// FTXUI_EXPORT_ANNOTATION macro defined below. Otherwise it will be
// annotated using the FTXUI_IMPORT_ANNOTATION macro.
#define FTXUI_EXPORT(component)                         \
  FTXUI_MACRO_CONDITIONAL_(IS_FTXUI_##component##_IMPL, \
                           FTXUI_EXPORT_ANNOTATION, FTXUI_IMPORT_ANNOTATION)

// Indicates whether the current compilation unit is being compiled as part of
// the implementation of the component named |component|. Expands to |1| if
// |IS_FTXUI_$component_IMPL| is defined as |1|; expands to |0| otherwise.
//
// Note in particular that if |IS_FTXUI_$component_IMPL| is not defined at all,
// it is still fine to test INSIDE_FTXUI_COMPONENT_IMPL(component), which
// expands to |0| as expected.
#define INSIDE_FTXUI_COMPONENT_IMPL(component) \
  FTXUI_MACRO_CONDITIONAL_(IS_FTXUI_##component##_IMPL, 1, 0)

#endif

// Compiler-specific macros to annotate for export or import of a symbol. No-op
// in non-component builds. These should not see much if any direct use.
// Instead use the FTXUI_EXPORT macro defined above.
#if defined(COMPONENT_BUILD)
#if defined(WIN32)
#define FTXUI_EXPORT_ANNOTATION __declspec(dllexport)
#define FTXUI_IMPORT_ANNOTATION __declspec(dllimport)
#else  // defined(WIN32)
#define FTXUI_EXPORT_ANNOTATION __attribute__((visibility("default")))
#define FTXUI_IMPORT_ANNOTATION __attribute__((visibility("default")))
#endif  // defined(WIN32)
#else   // defined(COMPONENT_BUILD)
#define FTXUI_EXPORT_ANNOTATION
#define FTXUI_IMPORT_ANNOTATION
#endif  // defined(COMPONENT_BUILD)

// Below this point are several internal utility macros used for the
// implementation of the above macros. Not intended for external use.

#define FTXUI_MACRO_EXPAND(x) x

// Helper for conditional expansion to one of two token strings. If |condition|
// expands to |1| then this macro expands to |consequent|; otherwise it expands
// to |alternate|.
#define FTXUI_MACRO_CONDITIONAL_(condition, consequent, alternate) \
  FTXUI_MACRO_EXPAND(FTXUI_MACRO_SELECT_THIRD_ARGUMENT_(           \
      FTXUI_MACRO_CONDITIONAL_COMMA_(condition), consequent, alternate))

// Expands to a comma (,) iff its first argument expands to |1|. Used in
// conjunction with |FTXUI_MACRO_SELECT_THIRD_ARGUMENT_()|, as the presence
// or absense of an extra comma can be used to conditionally shift subsequent
// argument positions and thus influence which argument is selected.
#define FTXUI_MACRO_CONDITIONAL_COMMA_(...) \
  FTXUI_MACRO_EXPAND(FTXUI_MACRO_CONDITIONAL_COMMA_IMPL_(__VA_ARGS__, dummy))
#define FTXUI_MACRO_CONDITIONAL_COMMA_IMPL_(x, ...) \
  FTXUI_MACRO_CONDITIONAL_COMMA_##x##_
#define FTXUI_MACRO_CONDITIONAL_COMMA_1_ ,

// Helper which simply selects its third argument. Used in conjunction with
// |FTXUI_MACRO_CONDITIONAL_COMMA_()| above to implement conditional macro
// expansion.
#define FTXUI_MACRO_SELECT_THIRD_ARGUMENT_(...) \
  FTXUI_MACRO_EXPAND(                           \
      FTXUI_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(__VA_ARGS__, dummy))
#define FTXUI_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(a, b, c, ...) c


namespace ftxui::animation {
/// @brief RequestAnimationFrame is a function that requests a new frame to be
/// drawn in the next animation cycle.
///
/// @note This function is typically called by components that need to
/// update their state or appearance over time, such as animations or
/// transitions. This is useful when the change doesn't depend depend on the
/// events seen by the terminal, but rather on the passage of time.
///
/// Components who haven't completed their animation can call this function to
/// request a new frame to be drawn later.
///
/// When there is no new events and no animations to complete, no new frame is
/// drawn.
///
/// @ingroup component
FTXUI_EXPORT(COMPONENT) void RequestAnimationFrame();

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<float>;

// Parameter of Component::OnAnimation(param).
class FTXUI_EXPORT(COMPONENT) Params {
 public:
  explicit Params(Duration duration) : duration_(duration) {}

  /// The duration this animation step represents.
  Duration duration() const { return duration_; }

 private:
  Duration duration_;
};

namespace easing {
using Function = std::function<float(float)>;
// Linear interpolation (no easing)
FTXUI_EXPORT(COMPONENT) float Linear(float p);

// Quadratic easing; p^2
FTXUI_EXPORT(COMPONENT) float QuadraticIn(float p);
FTXUI_EXPORT(COMPONENT) float QuadraticOut(float p);
FTXUI_EXPORT(COMPONENT) float QuadraticInOut(float p);

// Cubic easing; p^3
FTXUI_EXPORT(COMPONENT) float CubicIn(float p);
FTXUI_EXPORT(COMPONENT) float CubicOut(float p);
FTXUI_EXPORT(COMPONENT) float CubicInOut(float p);

// Quartic easing; p^4
FTXUI_EXPORT(COMPONENT) float QuarticIn(float p);
FTXUI_EXPORT(COMPONENT) float QuarticOut(float p);
FTXUI_EXPORT(COMPONENT) float QuarticInOut(float p);

// Quintic easing; p^5
FTXUI_EXPORT(COMPONENT) float QuinticIn(float p);
FTXUI_EXPORT(COMPONENT) float QuinticOut(float p);
FTXUI_EXPORT(COMPONENT) float QuinticInOut(float p);

// Sine wave easing; sin(p * PI/2)
FTXUI_EXPORT(COMPONENT) float SineIn(float p);
FTXUI_EXPORT(COMPONENT) float SineOut(float p);
FTXUI_EXPORT(COMPONENT) float SineInOut(float p);

// Circular easing; sqrt(1 - p^2)
FTXUI_EXPORT(COMPONENT) float CircularIn(float p);
FTXUI_EXPORT(COMPONENT) float CircularOut(float p);
FTXUI_EXPORT(COMPONENT) float CircularInOut(float p);

// Exponential easing, base 2
FTXUI_EXPORT(COMPONENT) float ExponentialIn(float p);
FTXUI_EXPORT(COMPONENT) float ExponentialOut(float p);
FTXUI_EXPORT(COMPONENT) float ExponentialInOut(float p);

// Exponentially-damped sine wave easing
FTXUI_EXPORT(COMPONENT) float ElasticIn(float p);
FTXUI_EXPORT(COMPONENT) float ElasticOut(float p);
FTXUI_EXPORT(COMPONENT) float ElasticInOut(float p);

// Overshooting cubic easing;
FTXUI_EXPORT(COMPONENT) float BackIn(float p);
FTXUI_EXPORT(COMPONENT) float BackOut(float p);
FTXUI_EXPORT(COMPONENT) float BackInOut(float p);

// Exponentially-decaying bounce easing
FTXUI_EXPORT(COMPONENT) float BounceIn(float p);
FTXUI_EXPORT(COMPONENT) float BounceOut(float p);
FTXUI_EXPORT(COMPONENT) float BounceInOut(float p);
}  // namespace easing

class FTXUI_EXPORT(COMPONENT) Animator {
 public:
  explicit Animator(float* from,
                    float to = 0.f,
                    Duration duration = std::chrono::milliseconds(250),
                    easing::Function easing_function = easing::Linear,
                    Duration delay = std::chrono::milliseconds(0));

  void OnAnimation(Params&);

  float to() const { return to_; }

 private:
  float* value_;
  float from_;
  float to_;
  Duration duration_;
  easing::Function easing_function_;
  Duration current_;
};

}  // namespace ftxui::animation






namespace ftxui {

class FTXUI_EXPORT(COMPONENT) CapturedMouseInterface {
 public:
  CapturedMouseInterface() = default;
  CapturedMouseInterface(const CapturedMouseInterface&) = default;
  CapturedMouseInterface(CapturedMouseInterface&&) = delete;
  CapturedMouseInterface& operator=(const CapturedMouseInterface&) = default;
  CapturedMouseInterface& operator=(CapturedMouseInterface&&) = delete;
  virtual ~CapturedMouseInterface() = default;
};
using CapturedMouse = std::unique_ptr<CapturedMouseInterface>;
}  // namespace ftxui






namespace ftxui {

/// @brief A mouse event. It contains the coordinate of the mouse, the button
/// pressed and the modifier (shift, ctrl, meta).
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) Mouse {
  enum Button : uint8_t {
    Left = 0,
    Middle = 1,
    Right = 2,
    None = 3,
    WheelUp = 4,
    WheelDown = 5,
    WheelLeft = 6,   /// Supported terminal only.
    WheelRight = 7,  /// Supported terminal only.
  };

  enum Motion : uint8_t {
    Released = 0,
    Pressed = 1,
    Moved = 2,
  };

  // Button
  Button button = Button::None;

  // Motion
  Motion motion = Motion::Pressed;

  // Modifiers:
  bool shift = false;
  bool meta = false;
  bool control = false;

  // Coordinates:
  int x = 0;
  int y = 0;
};

}  // namespace ftxui



namespace ftxui {

class App;
class ComponentBase;

/// @brief Represent an event. It can be key press event, a terminal resize, or
/// more ...
///
/// For example:
/// - Printable character can be created using Event::Character('a').
/// - Some special are predefined, like Event::ArrowLeft.
/// - One can find arbitrary code for special Events using:
///   ./example/util/print_key_press
///  For instance, CTLR+A maps to Event::Special({1});
///
/// Useful documentation about xterm specification:
/// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
///
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) Event {
  // --- Constructor section ---------------------------------------------------
  static Event Character(std::string_view);
  static Event Character(char);
  static Event Character(wchar_t);
  static Event Special(std::string_view);
  static Event Special(std::initializer_list<char>);
  static Event Mouse(std::string_view, Mouse mouse);
  static Event CursorPosition(std::string_view, int x, int y);  // Internal
  static Event CursorShape(std::string_view, int shape);        // Internal
  static Event TerminalNameVersion(std::string_view,
                                   std::string name,
                                   int version);
  static Event TerminalEmulator(std::string_view,
                                std::string name,
                                std::string version);
  static Event TerminalCapabilities(std::string_view,
                                    std::vector<int> capabilities);

  // --- Arrow ---
  static const Event ArrowLeft;
  static const Event ArrowRight;
  static const Event ArrowUp;
  static const Event ArrowDown;

  static const Event ArrowLeftCtrl;
  static const Event ArrowRightCtrl;
  static const Event ArrowUpCtrl;
  static const Event ArrowDownCtrl;

  static const Event ArrowLeftAlt;
  static const Event ArrowRightAlt;
  static const Event ArrowUpAlt;
  static const Event ArrowDownAlt;

  // --- Other ---
  static const Event Backspace;
  static const Event BackspaceAlt;
  static const Event AltBackspace;
  static const Event Delete;
  static const Event DeleteAlt;
  static const Event DeleteCtrl;
  static const Event Return;
  static const Event Escape;
  static const Event Tab;
  static const Event TabReverse;

  // --- Navigation keys ---
  static const Event Insert;
  static const Event Home;
  static const Event End;
  static const Event PageUp;
  static const Event PageDown;

  // --- Function keys ---
  // MSVC's dllexport/dllimport only applies correctly to the first
  // declarator in a multi-name declaration, so these must be declared one
  // per statement (see error C2487) when built as a Windows DLL.
  static const Event F1;
  static const Event F2;
  static const Event F3;
  static const Event F4;
  static const Event F5;
  static const Event F6;
  static const Event F7;
  static const Event F8;
  static const Event F9;
  static const Event F10;
  static const Event F11;
  static const Event F12;

  // --- Control keys ---
  static const Event a;
  static const Event A;
  static const Event CtrlA;
  static const Event AltA;
  static const Event CtrlAltA;
  static const Event b;
  static const Event B;
  static const Event CtrlB;
  static const Event AltB;
  static const Event CtrlAltB;
  static const Event c;
  static const Event C;
  static const Event CtrlC;
  static const Event AltC;
  static const Event CtrlAltC;
  static const Event d;
  static const Event D;
  static const Event CtrlD;
  static const Event AltD;
  static const Event CtrlAltD;
  static const Event e;
  static const Event E;
  static const Event CtrlE;
  static const Event AltE;
  static const Event CtrlAltE;
  static const Event f;
  static const Event F;
  static const Event CtrlF;
  static const Event AltF;
  static const Event CtrlAltF;
  static const Event g;
  static const Event G;
  static const Event CtrlG;
  static const Event AltG;
  static const Event CtrlAltG;
  static const Event h;
  static const Event H;
  static const Event CtrlH;
  static const Event AltH;
  static const Event CtrlAltH;
  static const Event i;
  static const Event I;
  static const Event CtrlI;
  static const Event AltI;
  static const Event CtrlAltI;
  static const Event j;
  static const Event J;
  static const Event CtrlJ;
  static const Event AltJ;
  static const Event CtrlAltJ;
  static const Event k;
  static const Event K;
  static const Event CtrlK;
  static const Event AltK;
  static const Event CtrlAltK;
  static const Event l;
  static const Event L;
  static const Event CtrlL;
  static const Event AltL;
  static const Event CtrlAltL;
  static const Event m;
  static const Event M;
  static const Event CtrlM;
  static const Event AltM;
  static const Event CtrlAltM;
  static const Event n;
  static const Event N;
  static const Event CtrlN;
  static const Event AltN;
  static const Event CtrlAltN;
  static const Event o;
  static const Event O;
  static const Event CtrlO;
  static const Event AltO;
  static const Event CtrlAltO;
  static const Event p;
  static const Event P;
  static const Event CtrlP;
  static const Event AltP;
  static const Event CtrlAltP;
  static const Event q;
  static const Event Q;
  static const Event CtrlQ;
  static const Event AltQ;
  static const Event CtrlAltQ;
  static const Event r;
  static const Event R;
  static const Event CtrlR;
  static const Event AltR;
  static const Event CtrlAltR;
  static const Event s;
  static const Event S;
  static const Event CtrlS;
  static const Event AltS;
  static const Event CtrlAltS;
  static const Event t;
  static const Event T;
  static const Event CtrlT;
  static const Event AltT;
  static const Event CtrlAltT;
  static const Event u;
  static const Event U;
  static const Event CtrlU;
  static const Event AltU;
  static const Event CtrlAltU;
  static const Event v;
  static const Event V;
  static const Event CtrlV;
  static const Event AltV;
  static const Event CtrlAltV;
  static const Event w;
  static const Event W;
  static const Event CtrlW;
  static const Event AltW;
  static const Event CtrlAltW;
  static const Event x;
  static const Event X;
  static const Event CtrlX;
  static const Event AltX;
  static const Event CtrlAltX;
  static const Event y;
  static const Event Y;
  static const Event CtrlY;
  static const Event AltY;
  static const Event CtrlAltY;
  static const Event z;
  static const Event Z;
  static const Event CtrlZ;
  static const Event AltZ;
  static const Event CtrlAltZ;

  // --- Custom ---
  static const Event Custom;

  //--- Method section ---------------------------------------------------------
  bool operator==(const Event& other) const { return input_ == other.input_; }
  bool operator!=(const Event& other) const { return !operator==(other); }
  bool operator<(const Event& other) const { return input_ < other.input_; }

  const std::string& input() const { return input_; }

  bool is_character() const { return type_ == Type::Character; }
  std::string character() const { return input_; }

  bool is_mouse() const { return type_ == Type::Mouse; }
  struct Mouse& mouse() { return data_.mouse; }

  // --- Internal Method section -----------------------------------------------
  bool is_cursor_position() const { return type_ == Type::CursorPosition; }
  int cursor_x() const { return data_.cursor.x; }
  int cursor_y() const { return data_.cursor.y; }

  bool is_cursor_shape() const { return type_ == Type::CursorShape; }
  int cursor_shape() const { return data_.cursor_shape; }

  bool IsTerminalNameVersion() const;
  const std::string& TerminalName() const;
  int TerminalVersion() const;

  bool IsTerminalEmulator() const;
  const std::string& TerminalEmulatorName() const;
  const std::string& TerminalEmulatorVersion() const;

  bool IsTerminalCapabilities() const;
  const std::vector<int>& TerminalCapabilities() const;
  std::vector<std::string> TerminalCapabilityNames() const;

  // Debug
  std::string DebugString() const;

  //--- State section ----------------------------------------------------------
  App* screen_ = nullptr;

 private:
  friend ComponentBase;
  friend App;
  enum class Type : uint8_t {
    Unknown,
    Character,
    Mouse,
    CursorPosition,
    CursorShape,
    TerminalNameVersion,
    TerminalEmulator,
    TerminalCapabilities,
  };
  Type type_ = Type::Unknown;
  struct Cursor {
    int x = 0;
    int y = 0;
  };

  union {
    struct Mouse mouse;
    struct Cursor cursor;
    int cursor_shape;
    int terminal_version;
  } data_ = {};

  std::string input_;
  std::shared_ptr<std::string> terminal_name_;
  std::shared_ptr<std::string> terminal_emulator_version_;
  std::shared_ptr<std::vector<int>> terminal_capabilities_;
};

}  // namespace ftxui


namespace ftxui {
class AnimationTask {};
using Closure = std::function<void()>;
using Task = std::variant<Event, Closure, AnimationTask>;
}  // namespace ftxui







namespace ftxui {

/// @brief Box is a structure that represents a rectangular area in a 2D space.
///
/// It is defined by its minimum and maximum coordinates along the x and y axes.
/// Note that the coordinates are inclusive, meaning that the box includes both
/// the minimum and maximum values.
///
/// @ingroup screen
struct FTXUI_EXPORT(SCREEN) Box {
  int x_min = 0;
  int x_max = 0;
  int y_min = 0;
  int y_max = 0;

  static auto Intersection(Box a, Box b) -> Box;
  static auto Union(Box a, Box b) -> Box;
  void Shift(int x, int y);
  bool Contain(int x, int y) const;
  bool IsEmpty() const;
  bool operator==(const Box& other) const;
  bool operator!=(const Box& other) const;
};

}  // namespace ftxui





#ifdef RGB
// Workaround for wingdi.h (via Windows.h) defining macros that break things.
// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-rgb
#undef RGB
#endif

namespace ftxui {

namespace Terminal {
enum Color : uint8_t;
}  // namespace Terminal

/// @brief Color is a class that represents a color in the terminal user
/// interface.
///
/// @ingroup screen
class FTXUI_EXPORT(SCREEN) Color {
 public:
  enum Palette1 : uint8_t;
  enum Palette16 : uint8_t;
  enum Palette256 : uint8_t;

  // TODO(v8.0.0): Make the constructors and RGB()/RGBA() constexpr. They no
  // longer depend on the terminal color support, but moving them inline
  // removes their exported symbols, which breaks the ABI.
  // NOLINTBEGIN
  Color();                  // Transparent.
  Color(Palette1 index);    // Transparent.
  Color(Palette16 index);   // Implicit conversion from index to Color.
  Color(Palette256 index);  // Implicit conversion from index to Color.
  // NOLINTEND
  Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
  static Color RGB(uint8_t red, uint8_t green, uint8_t blue);
  static Color HSV(uint8_t hue, uint8_t saturation, uint8_t value);
  static Color RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
  static Color HSVA(uint8_t hue,
                    uint8_t saturation,
                    uint8_t value,
                    uint8_t alpha);
  static Color Interpolate(float t, const Color& a, const Color& b);
  static Color Blend(const Color& lhs, const Color& rhs);

  //---------------------------
  // List of colors:
  //---------------------------
  // clang-format off
  enum Palette1 : uint8_t{
    Default, // Transparent
  };

  enum Palette16 : uint8_t {
    Black        = 0,
    Red          = 1,
    Green        = 2,
    Yellow       = 3,
    Blue         = 4,
    Magenta      = 5,
    Cyan         = 6,
    GrayLight    = 7,
    GrayDark     = 8,
    RedLight     = 9,
    GreenLight   = 10,
    YellowLight  = 11,
    BlueLight    = 12,
    MagentaLight = 13,
    CyanLight    = 14,
    White        = 15,
  };

  enum Palette256 : uint8_t {
    Aquamarine1        = 122,
    Aquamarine1Bis     = 86,
    Aquamarine3        = 79,
    Blue1              = 21,
    Blue3              = 19,
    Blue3Bis           = 20,
    BlueViolet         = 57,
    CadetBlue          = 72,
    CadetBlueBis       = 73,
    Chartreuse1        = 118,
    Chartreuse2        = 112,
    Chartreuse2Bis     = 82,
    Chartreuse3        = 70,
    Chartreuse3Bis     = 76,
    Chartreuse4        = 64,
    CornflowerBlue     = 69,
    Cornsilk1          = 230,
    Cyan1              = 51,
    Cyan2              = 50,
    Cyan3              = 43,
    DarkBlue           = 18,
    DarkCyan           = 36,
    DarkGoldenrod      = 136,
    DarkGreen          = 22,
    DarkKhaki          = 143,
    DarkMagenta        = 90,
    DarkMagentaBis     = 91,
    DarkOliveGreen1    = 191,
    DarkOliveGreen1Bis = 192,
    DarkOliveGreen2    = 155,
    DarkOliveGreen3    = 107,
    DarkOliveGreen3Bis = 113,
    DarkOliveGreen3Ter = 149,
    DarkOrange         = 208,
    DarkOrange3        = 130,
    DarkOrange3Bis     = 166,
    DarkRed            = 52,
    DarkRedBis         = 88,
    DarkSeaGreen       = 108,
    DarkSeaGreen1      = 158,
    DarkSeaGreen1Bis   = 193,
    DarkSeaGreen2      = 151,
    DarkSeaGreen2Bis   = 157,
    DarkSeaGreen3      = 115,
    DarkSeaGreen3Bis   = 150,
    DarkSeaGreen4      = 65,
    DarkSeaGreen4Bis   = 71,
    DarkSlateGray1     = 123,
    DarkSlateGray2     = 87,
    DarkSlateGray3     = 116,
    DarkTurquoise      = 44,
    DarkViolet         = 128,
    DarkVioletBis      = 92,
    DeepPink1          = 198,
    DeepPink1Bis       = 199,
    DeepPink2          = 197,
    DeepPink3          = 161,
    DeepPink3Bis       = 162,
    DeepPink4          = 125,
    DeepPink4Bis          = 89,
    DeepPink4Ter       = 53,
    DeepSkyBlue1       = 39,
    DeepSkyBlue2       = 38,
    DeepSkyBlue3       = 31,
    DeepSkyBlue3Bis    = 32,
    DeepSkyBlue4       = 23,
    DeepSkyBlue4Bis    = 24,
    DeepSkyBlue4Ter    = 25,
    DodgerBlue1        = 33,
    DodgerBlue2        = 27,
    DodgerBlue3        = 26,
    Gold1              = 220,
    Gold3              = 142,
    Gold3Bis           = 178,
    Green1             = 46,
    Green3             = 34,
    Green3Bis          = 40,
    Green4             = 28,
    GreenYellow        = 154,
    Grey0              = 16,
    Grey100            = 231,
    Grey11             = 234,
    Grey15             = 235,
    Grey19             = 236,
    Grey23             = 237,
    Grey27             = 238,
    Grey3              = 232,
    Grey30             = 239,
    Grey35             = 240,
    Grey37             = 59,
    Grey39             = 241,
    Grey42             = 242,
    Grey46             = 243,
    Grey50             = 244,
    Grey53             = 102,
    Grey54             = 245,
    Grey58             = 246,
    Grey62             = 247,
    Grey63             = 139,
    Grey66             = 248,
    Grey69             = 145,
    Grey7              = 233,
    Grey70             = 249,
    Grey74             = 250,
    Grey78             = 251,
    Grey82             = 252,
    Grey84             = 188,
    Grey85             = 253,
    Grey89             = 254,
    Grey93             = 255,
    Honeydew2          = 194,
    HotPink            = 205,
    HotPink2           = 169,
    HotPink3           = 132,
    HotPink3Bis        = 168,
    HotPinkBis         = 206,
    IndianRed          = 131,
    IndianRed1         = 203,
    IndianRed1Bis      = 204,
    IndianRedBis       = 167,
    Khaki1             = 228,
    Khaki3             = 185,
    LightCoral         = 210,
    LightCyan1Bis      = 195,
    LightCyan3         = 152,
    LightGoldenrod1    = 227,
    LightGoldenrod2    = 186,
    LightGoldenrod2Bis = 221,
    LightGoldenrod2Ter = 222,
    LightGoldenrod3    = 179,
    LightGreen         = 119,
    LightGreenBis      = 120,
    LightPink1         = 217,
    LightPink3         = 174,
    LightPink4         = 95,
    LightSalmon1       = 216,
    LightSalmon3       = 137,
    LightSalmon3Bis       = 173,
    LightSeaGreen      = 37,
    LightSkyBlue1      = 153,
    LightSkyBlue3      = 109,
    LightSkyBlue3Bis      = 110,
    LightSlateBlue     = 105,
    LightSlateGrey     = 103,
    LightSteelBlue     = 147,
    LightSteelBlue1    = 189,
    LightSteelBlue3    = 146,
    LightYellow3       = 187,
    Magenta1           = 201,
    Magenta2           = 165,
    Magenta2Bis        = 200,
    Magenta3           = 127,
    Magenta3Bis        = 163,
    Magenta3Ter        = 164,
    MediumOrchid       = 134,
    MediumOrchid1      = 171,
    MediumOrchid1Bis   = 207,
    MediumOrchid3      = 133,
    MediumPurple       = 104,
    MediumPurple1      = 141,
    MediumPurple2      = 135,
    MediumPurple2Bis   = 140,
    MediumPurple3      = 97,
    MediumPurple3Bis   = 98,
    MediumPurple4      = 60,
    MediumSpringGreen  = 49,
    MediumTurquoise    = 80,
    MediumVioletRed    = 126,
    MistyRose1         = 224,
    MistyRose3         = 181,
    NavajoWhite1       = 223,
    NavajoWhite3       = 144,
    NavyBlue           = 17,
    Orange1            = 214,
    Orange3            = 172,
    Orange4            = 58,
    Orange4Bis         = 94,
    OrangeRed1         = 202,
    Orchid             = 170,
    Orchid1            = 213,
    Orchid2            = 212,
    PaleGreen1         = 121,
    PaleGreen1Bis      = 156,
    PaleGreen3         = 114,
    PaleGreen3Bis      = 77,
    PaleTurquoise1     = 159,
    PaleTurquoise4     = 66,
    PaleVioletRed1     = 211,
    Pink1              = 218,
    Pink3              = 175,
    Plum1              = 219,
    Plum2              = 183,
    Plum3              = 176,
    Plum4              = 96,
    Purple             = 129,
    Purple3            = 56,
    Purple4            = 54,
    Purple4Bis         = 55,
    PurpleBis          = 93,
    Red1               = 196,
    Red3               = 124,
    Red3Bis            = 160,
    RosyBrown          = 138,
    RoyalBlue1         = 63,
    Salmon1            = 209,
    SandyBrown         = 215,
    SeaGreen1          = 84,
    SeaGreen1Bis       = 85,
    SeaGreen2          = 83,
    SeaGreen3          = 78,
    SkyBlue1           = 117,
    SkyBlue2           = 111,
    SkyBlue3           = 74,
    SlateBlue1         = 99,
    SlateBlue3         = 61,
    SlateBlue3Bis      = 62,
    SpringGreen1       = 48,
    SpringGreen2       = 42,
    SpringGreen2Bis    = 47,
    SpringGreen3       = 35,
    SpringGreen3Bis    = 41,
    SpringGreen4       = 29,
    SteelBlue          = 67,
    SteelBlue1         = 75,
    SteelBlue1Bis      = 81,
    SteelBlue3         = 68,
    Tan                = 180,
    Thistle1           = 225,
    Thistle3           = 182,
    Turquoise2         = 45,
    Turquoise4         = 30,
    Violet             = 177,
    Wheat1             = 229,
    Wheat4             = 101,
    Yellow1            = 226,
    Yellow2            = 190,
    Yellow3            = 148,
    Yellow3Bis         = 184,
    Yellow4            = 100,
    Yellow4Bis         = 106,
  };
  // clang-format on

  // --- Operators ------
  bool operator==(const Color& rhs) const;
  bool operator!=(const Color& rhs) const;

  std::string Print(bool is_background_color) const;
  void PrintTo(std::string& out, bool is_background_color) const;
  void PrintTo(std::string& out,
               bool is_background_color,
               Terminal::Color color_support) const;
  bool IsOpaque() const { return alpha_ == 255; }

  // --- Accessors ------
  // Palette colors are resolved to their RGB approximation.
  uint8_t GetRed() const;
  uint8_t GetGreen() const;
  uint8_t GetBlue() const;
  uint8_t GetAlpha() const;

 private:
  enum class ColorType : uint8_t {
    Palette1,
    Palette16,
    Palette256,
    TrueColor,
  };
  ColorType type_ = ColorType::Palette1;
  uint8_t red_ = 0;
  uint8_t green_ = 0;
  uint8_t blue_ = 0;
  uint8_t alpha_ = 0;
};

inline namespace literals {

/// @brief Creates a color from a combined hex RGB representation,
/// e.g. 0x808000_rgb
FTXUI_EXPORT(SCREEN) Color operator""_rgb(unsigned long long int combined);

}  // namespace literals

}  // namespace ftxui


namespace ftxui {

/// @brief A Unicode character and its associated style.
///
/// @note This struct was previously named Pixel.
///
/// @ingroup screen
struct FTXUI_EXPORT(SCREEN) Cell {
  Cell()
      : blink(false),
        bold(false),
        dim(false),
        italic(false),
        inverted(false),
        underlined(false),
        underlined_double(false),
        strikethrough(false),
        automerge(false) {}

  // A bit field representing the style:
  bool blink : 1;
  bool bold : 1;
  bool dim : 1;
  bool italic : 1;
  bool inverted : 1;
  bool underlined : 1;
  bool underlined_double : 1;
  bool strikethrough : 1;
  bool automerge : 1;

  // The hyperlink associated with the cell.
  // 0 is the default value, meaning no hyperlink.
  // It's an index for accessing Screen meta data
  uint8_t hyperlink = 0;

  // The graphemes stored into the cell. To support combining characters,
  // like: a?, this can potentially contain multiple codepoints.
  std::string character = "";

  // Colors:
  Color background_color = Color::Default;
  Color foreground_color = Color::Default;
};

}  // namespace ftxui


namespace ftxui {

/// @brief A rectangular grid of Cell.
///
/// @note This class was previously named Image.
///
/// @ingroup screen
class FTXUI_EXPORT(SCREEN) Surface {
 public:
  // Constructors:
  Surface() = delete;
  Surface(int dimx, int dimy);

  // Destructor:
  virtual ~Surface() = default;

  // Copy:
  Surface(const Surface&) = default;
  Surface& operator=(const Surface&) = default;

  // Access a character in the grid at a given position.
  std::string& at(int x, int y);
  const std::string& at(int x, int y) const;

  // Access a cell (Cell) in the grid at a given position.
  Cell& CellAt(int x, int y);
  const Cell& CellAt(int x, int y) const;

  // [Deprecated] alias for CellAt.
  Cell& PixelAt(int x, int y) { return CellAt(x, y); }
  const Cell& PixelAt(int x, int y) const { return CellAt(x, y); }

  // Get screen dimensions.
  int dimx() const { return dimx_; }
  int dimy() const { return dimy_; }

  // Fill the surface with space and default style
  void Clear();

  // ABI Reserve:
  virtual void Reserved1();
  virtual void Reserved2();
  virtual void Reserved3();
  virtual void Reserved4();
  virtual void Reserved5();
  virtual void Reserved6();
  virtual void Reserved7();
  virtual void Reserved8();

  Box stencil;

 protected:
  Cell& FastCellAt(int x, int y);
  const Cell& FastCellAt(int x, int y) const;

  int dimx_;
  int dimy_;
  std::vector<Cell> cells_;
};

}  // namespace ftxui





namespace ftxui {

/// @brief Dimensions is a structure that represents the size of the terminal
/// @ingroup screen
struct FTXUI_EXPORT(SCREEN) Dimensions {
  int dimx;
  int dimy;
};

namespace Terminal {
FTXUI_EXPORT(SCREEN) Dimensions Size();
FTXUI_EXPORT(SCREEN) void SetFallbackSize(const Dimensions& fallbackSize);

/// @brief Color is an enumeration that represents the color support of the
/// terminal.
/// @ingroup screen
enum Color : uint8_t {
  Palette1,
  Palette16,
  Palette256,
  TrueColor,
};
FTXUI_EXPORT(SCREEN) Color ColorSupport();
FTXUI_EXPORT(SCREEN) void SetColorSupport(Color color);

/// @brief Quirks is a structure that represents various terminal-specific
/// behaviors that may require fallbacks.
/// @ingroup screen
class FTXUI_EXPORT(SCREEN) Quirks {
 public:
  Quirks();
  ~Quirks();
  Quirks(const Quirks&);
  Quirks& operator=(const Quirks&);
  Quirks(Quirks&&) noexcept;
  Quirks& operator=(Quirks&&) noexcept;

  /// @brief Whether the terminal font supports the 8 Unicode block characters.
  bool BlockCharacters() const;
  void SetBlockCharacters(bool v);

  /// @brief Whether the terminal correctly handles hiding the cursor.
  bool CursorHiding() const;
  void SetCursorHiding(bool v);

  /// @brief Whether the terminal should use ASCII characters for components.
  bool ComponentAscii() const;
  void SetComponentAscii(bool v);

  /// @brief The level of color support of the terminal.
  Color ColorSupport() const;
  void SetColorSupport(Color v);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
FTXUI_EXPORT(SCREEN) Quirks GetQuirks();
FTXUI_EXPORT(SCREEN) void SetQuirks(const Quirks& quirks);

/// @brief TerminalInfo is a structure that contains information about the
/// terminal.
/// @ingroup screen
class FTXUI_EXPORT(SCREEN) TerminalInfo {
 public:
  TerminalInfo();
  ~TerminalInfo();
  TerminalInfo(const TerminalInfo&) = delete;
  TerminalInfo& operator=(const TerminalInfo&) = delete;
  TerminalInfo(TerminalInfo&&) noexcept;
  TerminalInfo& operator=(TerminalInfo&&) noexcept;

  void SetTerm(std::string_view term);
  void SetColorterm(std::string_view colorterm);
  void SetTermProgram(std::string_view term_program);
  void SetTerminalName(std::string_view terminal_name);
  void SetTerminalEmulatorName(std::string_view terminal_emulator_name);
  void SetCapabilities(std::vector<int> capabilities);

  Color ComputeColorSupport() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// @brief Compute the color support based on environment variables and terminal
/// identification.
/// @param term The TERM environment variable.
/// @param colorterm The COLORTERM environment variable.
/// @param term_program The TERM_PROGRAM environment variable.
/// @param terminal_name The terminal name (from DA2).
/// @param terminal_emulator_name The terminal emulator name (from XTVERSION).
/// @param capabilities The terminal capabilities (from DA1).
FTXUI_EXPORT(SCREEN)
Color ComputeColorSupport(std::string_view term,
                          std::string_view colorterm,
                          std::string_view term_program,
                          std::string_view terminal_name,
                          std::string_view terminal_emulator_name,
                          const std::vector<int>& capabilities);

}  // namespace Terminal

}  // namespace ftxui


namespace ftxui {

/// @brief Define how the Screen's dimensions should look like.
/// @ingroup screen
namespace Dimension {
FTXUI_EXPORT(SCREEN) Dimensions Fixed(int);
FTXUI_EXPORT(SCREEN) Dimensions Full();
}  // namespace Dimension

/// @brief A rectangular grid of Cell.
/// @ingroup screen
class FTXUI_EXPORT(SCREEN) Screen : public Surface {
 public:
  // Constructors:
  Screen(int dimx, int dimy);
  static Screen Create(Dimensions dimension);
  static Screen Create(Dimensions width, Dimensions height);

  // Destructor:
  ~Screen() override = default;

  // Copy:
  Screen(const Screen&) = default;
  Screen& operator=(const Screen&) = default;

  std::string ToString() const;
  void ToString(std::string& ss) const;
  // Update the terminal from |previous| to this screen, printing only the cells
  // that changed. The cursor must be at the top-left corner of |previous|.
  void ToString(std::string& ss, const Screen& previous) const;

  // Print the Screen on to the terminal.
  void Print() const;

  // Fill the screen with space and reset any screen state, like hyperlinks, and
  // cursor
  void Clear();

  // Move the terminal cursor n-lines up with n = dimy().
  std::string ResetPosition(bool clear = false) const;
  void ResetPosition(std::string& ss, bool clear = false) const;

  void ApplyShader();

  struct Cursor {
    int x = 0;
    int y = 0;

    enum Shape : uint8_t {
      Hidden = 0,
      BlockBlinking = 1,
      Block = 2,
      UnderlineBlinking = 3,
      Underline = 4,
      BarBlinking = 5,
      Bar = 6,
    };
    Shape shape = Hidden;
  };

  Cursor cursor() const { return cursor_; }
  void SetCursor(Cursor cursor) { cursor_ = cursor; }

  // ABI Reserve:
  void Reserved1() override;
  void Reserved2() override;
  void Reserved3() override;
  void Reserved4() override;
  void Reserved5() override;
  void Reserved6() override;
  void Reserved7() override;
  void Reserved8() override;

  // Store an hyperlink in the screen. Return the id of the hyperlink. The id is
  // used to identify the hyperlink when the user click on it.
  uint8_t RegisterHyperlink(std::string_view link);
  const std::string& Hyperlink(uint8_t id) const;

  using SelectionStyle = std::function<void(Cell&)>;
  const SelectionStyle& GetSelectionStyle() const;
  void SetSelectionStyle(SelectionStyle decorator);

 protected:
  Cursor cursor_;
  std::vector<std::string> hyperlinks_ = {""};

  // The current selection style. This is overridden by various dom elements.
  SelectionStyle selection_style_ = [](Cell& cell) { cell.inverted ^= true; };
};

}  // namespace ftxui


namespace ftxui {
class ComponentBase;
using Component = std::shared_ptr<ComponentBase>;
class Node;
using Element = std::shared_ptr<Node>;
struct Event;
class Selection;
class TaskRunner;

/// @brief App is a class that manages the application lifecycle.
/// It is responsible for initializing the terminal, running the main loop,
/// and cleaning up on exit.
///
/// @note This class was previously named ScreenInteractive.
///
/// @ingroup component
class FTXUI_EXPORT(COMPONENT) App : public Screen {
 public:
  // Constructors:

  /// @brief Create an App with a fixed size.
  /// @param dimx The width of the app.
  /// @param dimy The height of the app.
  static App FixedSize(int dimx, int dimy);

  /// @brief Create an App taking the full terminal size. This is using the
  /// alternate screen buffer to avoid messing with the terminal content.
  /// @note This is the same as `App::FullscreenAlternateScreen()`
  static App Fullscreen();

  /// @brief Create an App taking the full terminal size. The primary screen
  /// buffer is being used. It means if the terminal is resized, the previous
  /// content might mess up with the terminal content.
  static App FullscreenPrimaryScreen();

  /// @brief Create an App taking the full terminal size. This is using the
  /// alternate screen buffer to avoid messing with the terminal content.
  static App FullscreenAlternateScreen();

  /// @brief Create an App whose width and height match the component being
  /// drawn.
  static App FitComponent();

  /// @brief Create an App whose width match the terminal output width and
  /// the height matches the component being drawn.
  static App TerminalOutput();

  // Destructor.
  ~App() override;

  App(App&&) noexcept;
  App& operator=(App&&) noexcept;
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  // Options. Must be called before Loop().

  /// @brief Set whether mouse is tracked and events reported.
  /// @param enable Whether to enable mouse event tracking.
  /// @note Mouse tracking is enabled by default.
  /// @note Mouse tracking is only supported on terminals that supports it.
  /// @note This must be called before calling `App::Loop`.
  void TrackMouse(bool enable = true);

  /// @brief Enable or disable automatic piped input handling.
  /// When enabled, FTXUI will detect piped input and redirect stdin from
  /// /dev/tty for keyboard input, allowing applications to read piped data
  /// while still receiving interactive keyboard events.
  /// @param enable Whether to enable piped input handling. Default is true.
  /// @note This must be called before Loop().
  /// @note This feature is enabled by default.
  /// @note This feature is only available on POSIX systems (Linux/macOS).
  void HandlePipedInput(bool enable = true);

  /// @brief Return the currently active app, nullptr if none.
  static App* Active();

  // Start/Stop the main loop.

  /// @brief Execute the main loop.
  /// @param component The component to draw.
  void Loop(Component component);

  /// @brief Exit the main loop.
  void Exit();

  /// @brief Return a function to exit the main loop.
  Closure ExitLoopClosure();

  /// @brief Decorate a function. The outputted one will execute similarly to
  /// the inputted one, but with the currently active app terminal hooks
  /// temporarily uninstalled.
  Closure WithRestoredIO(Closure fn);

  /// @brief FTXUI implements handlers for Ctrl-C and Ctrl-Z. By default, these
  /// handlers are executed, even if the component catches the event. This avoid
  /// users handling every event to be trapped in the application. However, in
  /// some cases, the application may want to handle these events itself. In
  /// this case, the application can force FTXUI to not handle these events by
  /// calling the following functions with force=true.
  void ForceHandleCtrlC(bool force = true);

  /// @brief Force FTXUI to handle or not handle Ctrl-Z, even if the component
  /// catches the Event::CtrlZ.
  void ForceHandleCtrlZ(bool force = true);

  // Post tasks to be executed by the loop.

  /// @brief Add a task to the main loop.
  /// It will be executed later, after every other scheduled tasks.
  void Post(Task task);

  /// @brief Add an event to the main loop.
  /// It will be executed later, after every other scheduled events.
  void PostEvent(Event event);

  /// @brief Add a task to the main loop.
  /// It will be executed later, after every other scheduled tasks.
  static void PostEventOrExecute(Closure closure);

  /// @brief Add a task to draw the screen one more time, until all the
  /// animations are done.
  void RequestAnimationFrame();

  /// @brief Print an element above the app. It is printed once, and becomes
  /// part of the terminal scrollback. The app keeps being drawn below.
  /// @param element The element to print. Its width is the terminal width.
  /// @note This is meant for apps not using the alternate screen, such as
  /// `App::TerminalOutput()`.
  void PrintAbove(Element element);

  // Selection API:

  /// @brief Try to get the unique lock about being able to capture the mouse.
  /// @return A unique lock if the mouse is not already captured, otherwise a
  /// null.
  CapturedMouse CaptureMouse();

  /// @brief Returns the content of the current selection.
  std::string GetSelection();

  /// @brief Set a callback that will be called when the selection changes.
  void SelectionChange(std::function<void()> callback);

  // Terminal info.

  /// @brief Return the terminal name.
  const std::string& TerminalName() const;

  /// @brief Return the terminal version.
  int TerminalVersion() const;

  /// @brief Return the terminal emulator name.
  const std::string& TerminalEmulatorName() const;

  /// @brief Return the terminal emulator version.
  const std::string& TerminalEmulatorVersion() const;

  /// @brief Return the terminal capabilities.
  const std::vector<int>& TerminalCapabilities() const;

  /// @brief Return the names of the terminal capabilities.
  std::vector<std::string> TerminalCapabilityNames() const;

 private:
  void ExitNow();
  void Install();
  void Uninstall();

  void PreMain();
  void PostMain();

  /// @brief Return whether the main loop has been quit.
  bool HasQuitted();
  void RunOnce(const Component& component);
  void RunOnceBlocking(Component component);

  void HandleTask(Component component, Task& task);
  bool HandleSelection(bool handled, Event event);
  void Draw(Component component);
  std::string ResetCursorPosition();

  void RequestCursorPosition(bool force = false);

  void TerminalSend(std::string_view);
  void TerminalFlush();

  void InstallPipedInputHandling();
  void InstallTerminalInfo();

  void Signal(int signal);

  size_t FetchTerminalEvents();

  void PostAnimationTask();

  struct Internal;
  explicit App(std::unique_ptr<Internal> internal, int dimx, int dimy);

  std::unique_ptr<Internal> internal_;

  friend class Loop;

 public:
  class Private {
   public:
    static void Signal(App& s, int signal) { s.Signal(signal); }
  };
  friend Private;
};

}  // namespace ftxui




#ifdef min
#error \
    "The macro 'min' is defined, which conflicts with the standard C++ library and FTXUI. This is often caused by including <windows.h>. To fix this, add '#define NOMINMAX' before including <windows.h>, or pass '/DNOMINMAX' as a compiler flag."
#endif

#ifdef max
#error \
    "The macro 'max' is defined, which conflicts with the standard C++ library and FTXUI. This is often caused by including <windows.h>. To fix this, add '#define NOMINMAX' before including <windows.h>, or pass '/DNOMINMAX' as a compiler flag."
#endif








#ifdef DrawText
// Workaround for WinUsr.h (via Windows.h) defining macros that break things.
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-drawtext
#undef DrawText
#endif

namespace ftxui {

/// @brief Canvas is a drawable buffer associated with drawing operations.
///
/// Canvas is a drawable area that can be used to create complex graphics. It
/// supports drawing points, lines, circles, ellipses, text, and images using
/// braille, block, or normal characters.
///
/// Note: A terminal contains cells. A cells is a unit of:
/// - 2x4 braille characters (1x1 pixel)
/// - 2x2 block characters (2x2 pixels)
/// - 2x4 normal characters (2x4 pixels)
///
/// You need to multiply the x coordinate by 2 and the y coordinate by 4 to
/// get the correct position in the terminal.
///
/// @ingroup dom
struct FTXUI_EXPORT(DOM) Canvas {
 public:
  Canvas() = default;
  Canvas(int width, int height);

  // Getters:
  int width() const { return width_; }
  int height() const { return height_; }
  Cell GetCell(int x, int y) const;
  // [Deprecated] alias for GetCell.
  Cell GetPixel(int x, int y) const { return GetCell(x, y); }

  using Stylizer = std::function<void(Cell&)>;

  // Draws using braille characters --------------------------------------------
  void DrawPointOn(int x, int y);
  void DrawPointOff(int x, int y);
  void DrawPointToggle(int x, int y);
  void DrawPoint(int x, int y, bool value);
  void DrawPoint(int x, int y, bool value, const Stylizer& s);
  void DrawPoint(int x, int y, bool value, const Color& color);
  void DrawPointLine(int x1, int y1, int x2, int y2);
  void DrawPointLine(int x1, int y1, int x2, int y2, const Stylizer& s);
  void DrawPointLine(int x1, int y1, int x2, int y2, const Color& color);
  void DrawPointCircle(int x, int y, int radius);
  void DrawPointCircle(int x, int y, int radius, const Stylizer& s);
  void DrawPointCircle(int x, int y, int radius, const Color& color);
  void DrawPointCircleFilled(int x, int y, int radius);
  void DrawPointCircleFilled(int x, int y, int radius, const Stylizer& s);
  void DrawPointCircleFilled(int x, int y, int radius, const Color& color);
  void DrawPointEllipse(int x, int y, int r1, int r2);
  void DrawPointEllipse(int x, int y, int r1, int r2, const Color& color);
  void DrawPointEllipse(int x, int y, int r1, int r2, const Stylizer& s);
  void DrawPointEllipseFilled(int x, int y, int r1, int r2);
  void DrawPointEllipseFilled(int x, int y, int r1, int r2, const Color& color);
  void DrawPointEllipseFilled(int x, int y, int r1, int r2, const Stylizer& s);

  // Draw using box characters -------------------------------------------------
  // Block are of size 1x2. y is considered to be a multiple of 2.
  void DrawBlockOn(int x, int y);
  void DrawBlockOff(int x, int y);
  void DrawBlockToggle(int x, int y);
  void DrawBlock(int x, int y, bool value);
  void DrawBlock(int x, int y, bool value, const Stylizer& s);
  void DrawBlock(int x, int y, bool value, const Color& color);
  void DrawBlockLine(int x1, int y1, int x2, int y2);
  void DrawBlockLine(int x1, int y1, int x2, int y2, const Stylizer& s);
  void DrawBlockLine(int x1, int y1, int x2, int y2, const Color& color);
  void DrawBlockCircle(int x1, int y1, int radius);
  void DrawBlockCircle(int x1, int y1, int radius, const Stylizer& s);
  void DrawBlockCircle(int x1, int y1, int radius, const Color& color);
  void DrawBlockCircleFilled(int x1, int y1, int radius);
  void DrawBlockCircleFilled(int x1, int y1, int radius, const Stylizer& s);
  void DrawBlockCircleFilled(int x1, int y1, int radius, const Color& color);
  void DrawBlockEllipse(int x1, int y1, int r1, int r2);
  void DrawBlockEllipse(int x1, int y1, int r1, int r2, const Stylizer& s);
  void DrawBlockEllipse(int x1, int y1, int r1, int r2, const Color& color);
  void DrawBlockEllipseFilled(int x1, int y1, int r1, int r2);
  void DrawBlockEllipseFilled(int x1,
                              int y1,
                              int r1,
                              int r2,
                              const Stylizer& s);
  void DrawBlockEllipseFilled(int x1,
                              int y1,
                              int r1,
                              int r2,
                              const Color& color);

  // Draw using normal characters ----------------------------------------------
  // Draw using character of size 2x4 at position (x,y)
  // x is considered to be a multiple of 2.
  // y is considered to be a multiple of 4.
  void DrawText(int x, int y, std::string_view value);
  void DrawText(int x, int y, std::string_view value, const Color& color);
  void DrawText(int x, int y, std::string_view value, const Stylizer& style);

  // Draw using directly pixels or images --------------------------------------
  // x is considered to be a multiple of 2.
  // y is considered to be a multiple of 4.
  void DrawCell(int x, int y, const Cell&);
  void DrawSurface(int x, int y, const Surface&);

  // [Deprecated] alias for DrawCell.
  void DrawPixel(int x, int y, const Cell& cell) { DrawCell(x, y, cell); }
  // [Deprecated] alias for DrawSurface.
  void DrawImage(int x, int y, const Surface& s) { DrawSurface(x, y, s); }

  // Decorator:
  // x is considered to be a multiple of 2.
  // y is considered to be a multiple of 4.
  void Style(int x, int y, const Stylizer& style);

 private:
  bool IsIn(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
  }

  enum CellType {
    kCell,     // Units of size 2x4
    kBlock,    // Units of size 2x2
    kBraille,  // Units of size 1x1
  };

  struct CanvasCell {
    CellType type = kCell;
    Cell content;
  };

  struct XY {
    int x;
    int y;
    bool operator==(const XY& other) const {
      return x == other.x && y == other.y;
    }
  };

  struct XYHash {
    size_t operator()(const XY& xy) const {
      constexpr size_t shift = 1024;
      return size_t(xy.x) * shift + size_t(xy.y);
    }
  };

  int width_ = 0;
  int height_ = 0;
  std::unordered_map<XY, CanvasCell, XYHash> storage_;
};

}  // namespace ftxui



namespace ftxui {

/// @brief Direction is an enumeration that represents the four cardinal
/// directions.
///
/// @ingroup dom
enum class Direction : uint8_t {
  Up = 0,
  Down = 1,
  Left = 2,
  Right = 3,
};

}  // namespace ftxui




/*
  This replicate the CSS flexbox model.
  See guide for documentation:
  https://css-tricks.com/snippets/css/a-guide-to-flexbox/
*/

namespace ftxui {

/// @brief FlexboxConfig is a configuration structure that defines the layout
/// properties for a flexbox container.
//
/// It allows you to specify the direction of the flex items, whether they
/// should wrap, how they should be justified along the main axis, and how
/// they should be aligned along the cross axis.
/// It also includes properties for gaps between flex items in both the
/// main and cross axes.
/// This structure is used to configure the layout behavior of flexbox
/// containers in a terminal user interface.
///
/// @ingroup dom
struct FTXUI_EXPORT(DOM) FlexboxConfig {
  /// This establishes the main-axis, thus defining the direction flex items are
  /// placed in the flex container. Flexbox is (aside wrapping) single-direction
  /// layout concept. Think of flex items as primarily laying out either in
  /// horizontal rows or vertical columns.
  enum class Direction : uint8_t {
    Row,            ///< Flex items are laid out in a row.
    RowInversed,    ///< Flex items are laid out in a row, but in reverse order.
    Column,         ///< Flex items are laid out in a column.
    ColumnInversed  ///< Flex items are laid out in a column, but in reverse
                    ///< order.
  };
  Direction direction = Direction::Row;

  /// By default, flex items will all try to fit onto one line. You can change
  /// that and allow the items to wrap as needed with this property.
  enum class Wrap : uint8_t {
    NoWrap,        ///< Flex items will all try to fit onto one line.
    Wrap,          ///< Flex items will wrap onto multiple lines.
    WrapInversed,  ///< Flex items will wrap onto multiple lines, but in reverse
                   ///< order.
  };
  Wrap wrap = Wrap::Wrap;

  /// This defines the alignment along the main axis. It helps distribute extra
  /// free space leftover when either all the flex items on a line are
  /// inflexible, or are flexible but have reached their maximum size. It also
  /// exerts some control over the alignment of items when they overflow the
  /// line.
  enum class JustifyContent : uint8_t {
    /// Items are aligned to the start of flexbox's direction.
    FlexStart,
    /// Items are aligned to the end of flexbox's direction.
    FlexEnd,
    /// Items are centered along the line.
    Center,
    /// Items are stretched to fill the line.
    Stretch,
    /// Items are evenly distributed in the line; first item is on the start
    // line, last item on the end line
    SpaceBetween,
    /// Items are evenly distributed in the line with equal space around them.
    /// Note that visually the spaces aren’t equal, since all the items have
    /// equal space on both sides. The first item will have one unit of space
    /// against the container edge, but two units of space between the next item
    /// because that next item has its own spacing that applies.
    SpaceAround,
    /// Items are distributed so that the spacing between any two items (and the
    /// space to the edges) is equal.
    SpaceEvenly,
  };
  JustifyContent justify_content = JustifyContent::FlexStart;

  /// This defines the default behavior for how flex items are laid out along
  /// the cross axis on the current line. Think of it as the justify-content
  /// version for the cross-axis (perpendicular to the main-axis).
  enum class AlignItems : uint8_t {
    FlexStart,  ///< items are placed at the start of the cross axis.
    FlexEnd,    ///< items are placed at the end of the cross axis.
    Center,     ///< items are centered along the cross axis.
    Stretch,    ///< items are stretched to fill the cross axis.
  };
  AlignItems align_items = AlignItems::FlexStart;

  // This aligns a flex container’s lines within when there is extra space in
  // the cross-axis, similar to how justify-content aligns individual items
  // within the main-axis.
  enum class AlignContent : uint8_t {
    FlexStart,     ///< items are placed at the start of the cross axis.
    FlexEnd,       ///< items are placed at the end of the cross axis.
    Center,        ///< items are centered along the cross axis.
    Stretch,       ///< items are stretched to fill the cross axis.
    SpaceBetween,  ///< items are evenly distributed in the cross axis.
    SpaceAround,   ///< tems evenly distributed with equal space around each
                   ///< line.
    SpaceEvenly,  ///< items are evenly distributed in the cross axis with equal
                  ///< space around them.
  };
  AlignContent align_content = AlignContent::FlexStart;

  int gap_x = 0;
  int gap_y = 0;

  // Constructor pattern. For chained use like:
  // ```
  // FlexboxConfig()
  //    .Set(FlexboxConfig::Direction::Row)
  //    .Set(FlexboxConfig::Wrap::Wrap);
  // ```
  FlexboxConfig& Set(FlexboxConfig::Direction);
  FlexboxConfig& Set(FlexboxConfig::Wrap);
  FlexboxConfig& Set(FlexboxConfig::JustifyContent);
  FlexboxConfig& Set(FlexboxConfig::AlignItems);
  FlexboxConfig& Set(FlexboxConfig::AlignContent);
  FlexboxConfig& SetGap(int gap_x, int gap_y);
};

}  // namespace ftxui




namespace ftxui {

/// @brief A class representing the settings for linear-gradient color effect.
///
/// Example:
/// ```cpp
/// LinearGradient()
///    .Angle(45)
///    .Stop(Color::Red, 0.0)
///    .Stop(Color::Green, 0.5)
///    .Stop(Color::Blue, 1.0);
/// ```
///
/// There are also shorthand constructors:
/// ```cpp
/// LinearGradient(Color::Red, Color::Blue);
/// LinearGradient(45, Color::Red, Color::Blue);
/// ```
///
/// @ingroup dom
struct FTXUI_EXPORT(DOM) LinearGradient {
  float angle = 0.f;

  /// A stop is a color at a specific position in the gradient.
  /// The position is a value between 0.0 and 1.0,
  /// where 0.0 is the start of the gradient
  /// and 1.0 is the end of the gradient.
  struct Stop {
    Color color = Color::Default;
    std::optional<float> position;
  };
  std::vector<Stop> stops;

  // Simple constructor
  LinearGradient();
  LinearGradient(Color begin, Color end);
  LinearGradient(float angle, Color begin, Color end);

  // Modifier using the builder pattern.
  LinearGradient& Angle(float angle);
  LinearGradient& Stop(Color color, float position);
  LinearGradient& Stop(Color color);
};

}  // namespace ftxui





namespace ftxui {
class Node;

/// @brief Requirement is a structure that defines the layout requirements for a
/// Node in the terminal user interface.
///
/// It specifies the minimum size required to fully draw the element,
/// @ingroup dom
struct FTXUI_EXPORT(DOM) Requirement {
  // The required size to fully draw the element.
  int min_x = 0;
  int min_y = 0;

  // How much flexibility is given to the component.
  int flex_grow_x = 0;
  int flex_grow_y = 0;
  int flex_shrink_x = 0;
  int flex_shrink_y = 0;

  // Focus management to support the frame/focus/select element.
  struct Focused {
    bool enabled = false;
    Box box;
    Node* node = nullptr;
    Screen::Cursor::Shape cursor_shape = Screen::Cursor::Shape::Hidden;

    // Internal for interactions with components.
    bool component_active = false;
    bool component_focused = false;

    // Return whether this requirement should be preferred over the other.
    bool Prefer(const Focused& other) const {
      if (!other.enabled) {
        return false;
      }
      if (!enabled) {
        return true;
      }
      if (other.component_focused != component_focused) {
        return other.component_focused;
      }

      return other.component_active && !component_active;
    }
  };
  Focused focused;
};

}  // namespace ftxui




namespace ftxui {

/// @brief Represents a selection in a terminal user interface.
///
/// Selection is a class that represents the two endpoints of a selection in a
/// terminal user interface.
///
/// @ingroup dom
class FTXUI_EXPORT(DOM) Selection {
 public:
  Selection();  // Empty selection.
  Selection(int start_x, int start_y, int end_x, int end_y);

  const Box& GetBox() const;

  Selection SaturateHorizontal(Box box);
  Selection SaturateVertical(Box box);
  bool IsEmpty() const { return empty_; }

  void AddPart(std::string_view part, int y, int left, int right);
  std::string GetParts() { return parts_.str(); }

 private:
  Selection(int start_x, int start_y, int end_x, int end_y, Selection* parent);

  const int start_x_ = 0;
  const int start_y_ = 0;
  const int end_x_ = 0;
  const int end_y_ = 0;
  const Box box_ = {};
  Selection* const parent_ = this;
  const bool empty_ = true;
  std::stringstream parts_;

  // The position of the last inserted part.
  int x_ = 0;
  int y_ = 0;
};

}  // namespace ftxui


namespace ftxui {

class Node;
class Screen;

using Element = std::shared_ptr<Node>;
using Elements = std::vector<Element>;

/// @brief Node is the base class for all elements in the DOM tree.
///
/// It represents a single node in the document object model (DOM) and provides
/// the basic structure for layout and rendering.
/// It contains methods for computing layout requirements, setting the box
/// dimensions, selecting content, rendering to the screen, and checking the
/// layout status.
/// It typically contains child elements, which are also instances of Node.
///
/// Users are expected to derive from this class to create custom elements.
///
/// A list of builtin elements can be found in the `elements.hpp` file.
///
/// @ingroup dom
class FTXUI_EXPORT(DOM) Node {
 public:
  Node();
  explicit Node(Elements children);
  Node(const Node&) = delete;
  Node(const Node&&) = delete;
  Node& operator=(const Node&) = delete;
  Node& operator=(const Node&&) = delete;

  virtual ~Node();

  // Step 1: Compute layout requirement. Tell parent what dimensions this
  //         element wants to be.
  //         Propagated from Children to Parents.
  virtual void ComputeRequirement();
  Requirement requirement() { return requirement_; }

  // Step 2: Assign this element its final dimensions.
  //         Propagated from Parents to Children.
  virtual void SetBox(Box box);

  // Step 3: (optional) Selection
  //         Propagated from Parents to Children.
  virtual void Select(Selection& selection);

  // Step 4: Draw this element.
  virtual void Render(Screen& screen);

  virtual std::string GetSelectedContent(Selection& selection);

  // Layout may not resolve within a single iteration for some elements. This
  // allows them to request additional iterations. This signal must be
  // forwarded to children at least once.
  struct Status {
    int iteration = 0;
    bool need_iteration = false;
  };
  virtual void Check(Status* status);

  // ABI Reserve:
  virtual void Reserved1();
  virtual void Reserved2();
  virtual void Reserved3();
  virtual void Reserved4();
  virtual void Reserved5();
  virtual void Reserved6();
  virtual void Reserved7();
  virtual void Reserved8();

  friend FTXUI_EXPORT(DOM) void Render(Screen& screen, Node* node, Selection& selection);

 protected:
  Elements children_;
  Requirement requirement_;
  Box box_;
};

FTXUI_EXPORT(DOM) void Render(Screen& screen, const Element& element);
FTXUI_EXPORT(DOM) void Render(Screen& screen, Node* node);
FTXUI_EXPORT(DOM)
void Render(Screen& screen, Node* node, Selection& selection);
FTXUI_EXPORT(DOM)
std::string GetNodeSelectedContent(Screen& screen,
                                   Node* node,
                                   Selection& selection);

}  // namespace ftxui





namespace ftxui {
FTXUI_EXPORT(SCREEN) std::string to_string(std::wstring_view s);
FTXUI_EXPORT(SCREEN) std::wstring to_wstring(std::string_view s);

template <typename T>
std::wstring to_wstring(T s) {
  return to_wstring(std::string_view(std::to_string(s)));
}
inline std::wstring to_wstring(const std::string& s) {
  return to_wstring(std::string_view(s));
}
template <>
inline std::wstring to_wstring(const char* s) {
  return to_wstring(std::string_view(s));
}

FTXUI_EXPORT(SCREEN) int string_width(std::string_view);

// Split the string into a its glyphs. An empty one is inserted ater fullwidth
// ones.
FTXUI_EXPORT(SCREEN)
std::vector<std::string> Utf8ToGlyphs(std::string_view input);

// Map every cells drawn by |input| to their corresponding Glyphs. Half-size
// Glyphs takes one cell, full-size Glyphs take two cells.
FTXUI_EXPORT(SCREEN)
std::vector<int> CellToGlyphIndex(std::string_view input);

}  // namespace ftxui



namespace ftxui {

/// @brief An adapter. Own or reference an immutable object.
template <typename T>
class ConstRef {
 public:
  ConstRef() = default;

  // Owning constructors:
  ConstRef(T t) : variant_(std::move(t)) {}  // NOLINT

  // Referencing constructors:
  ConstRef(const T* t) : variant_(t) {}  // NOLINT

  ConstRef& operator=(ConstRef&&) noexcept = default;
  ConstRef(const ConstRef<T>&) = default;
  ConstRef(ConstRef<T>&&) noexcept = default;
  ~ConstRef() = default;

  // Make a "reseatable" reference
  ConstRef<T>& operator=(const ConstRef<T>&) = default;

  // Accessors:
  const T& operator()() const { return *Address(); }
  const T& operator*() const { return *Address(); }
  const T* operator->() const { return Address(); }

 private:
  std::variant<T, const T*> variant_ = T{};

  const T* Address() const {
    if (const T* t = std::get_if<T>(&variant_)) {
      return t;
    }
    return std::get<const T*>(variant_);
  }
};

/// @brief An adapter. Own or reference an mutable object.
template <typename T>
class Ref {
 public:
  Ref() = default;

  // Owning constructors:
  Ref(T t)
      : variant_(std::move(t)) {}  // NOLINT
                                   //
  // Referencing constructors:
  Ref(T* t)
      : variant_(t) {}  // NOLINT
                        //
  ~Ref() = default;
  Ref& operator=(Ref&&) noexcept = default;
  Ref(const Ref<T>&) = default;
  Ref(Ref<T>&&) noexcept = default;

  // Make a "reseatable" reference.
  Ref<T>& operator=(const Ref<T>&) = default;

  // Accessors:
  T& operator()() { return *Address(); }
  T& operator*() { return *Address(); }
  T* operator->() { return Address(); }
  const T& operator()() const { return *Address(); }
  const T& operator*() const { return *Address(); }
  const T* operator->() const { return Address(); }

 private:
  std::variant<T, T*> variant_ = T{};

  const T* Address() const {
    if (const T* t = std::get_if<T>(&variant_)) {
      return t;
    }
    return std::get<T*>(variant_);
  }
  T* Address() {
    if (T* t = std::get_if<T>(&variant_)) {
      return t;
    }
    return std::get<T*>(variant_);
  }
};

/// @brief An adapter. Own or reference a constant string. For convenience, this
/// class convert multiple mutable string toward a shared representation.
class FTXUI_EXPORT(SCREEN) StringRef : public Ref<std::string> {
 public:
  using Ref<std::string>::Ref;

  // Owning constructors:
  StringRef(const wchar_t* ref)  // NOLINT
      : StringRef(to_string(std::wstring(ref))) {}
  StringRef(const char* ref)  // NOLINT
      : StringRef(std::string(ref)) {}
  StringRef(std::string_view ref)  // NOLINT
      : StringRef(std::string(ref)) {}
  StringRef(std::wstring_view ref)  // NOLINT
      : StringRef(to_string(ref)) {}
};

/// @brief An adapter. Own or reference a constant string. For convenience, this
/// class convert multiple immutable string toward a shared representation.
class FTXUI_EXPORT(SCREEN) ConstStringRef : public ConstRef<std::string> {
 public:
  using ConstRef<std::string>::ConstRef;

  // Referencing constructors:
  ConstStringRef(const std::wstring* ref)  // NOLINT
      : ConstStringRef(to_string(*ref)) {}

  // Owning constructors:
  ConstStringRef(const std::wstring ref)  // NOLINT
      : ConstStringRef(to_string(ref)) {}
  ConstStringRef(std::wstring_view ref)  // NOLINT
      : ConstStringRef(to_string(ref)) {}
  ConstStringRef(const wchar_t* ref)  // NOLINT
      : ConstStringRef(to_string(std::wstring(ref))) {}
  ConstStringRef(const char* ref)  // NOLINT
      : ConstStringRef(std::string(ref)) {}
  ConstStringRef(std::string_view ref)  // NOLINT
      : ConstStringRef(std::string(ref)) {}
};

/// @brief An adapter. Reference a list of strings.
///
/// Supported input:
/// - `std::vector<std::string>`
/// - `std::vector<std::string>*`
/// - `std::vector<std::wstring>*`
/// - `Adapter*`
/// - `std::unique_ptr<Adapter>`
class FTXUI_EXPORT(SCREEN) ConstStringListRef {
 public:
  // Bring your own adapter:
  class Adapter {
   public:
    Adapter() = default;
    Adapter(const Adapter&) = default;
    Adapter& operator=(const Adapter&) = default;
    Adapter(Adapter&&) = default;
    Adapter& operator=(Adapter&&) = default;
    virtual ~Adapter() = default;
    virtual size_t size() const = 0;
    virtual std::string_view operator[](size_t i) const = 0;
  };
  using Variant = std::variant<const std::vector<std::string>,        //
                               const std::vector<std::string>*,       //
                               const std::vector<std::string_view>,   //
                               const std::vector<std::string_view>*,  //
                               const std::vector<std::wstring>*,      //
                               Adapter*,                              //
                               std::unique_ptr<Adapter>               //
                               >;

  ConstStringListRef() = default;
  ~ConstStringListRef() = default;
  ConstStringListRef& operator=(const ConstStringListRef&) = default;
  ConstStringListRef& operator=(ConstStringListRef&&) = default;
  ConstStringListRef(ConstStringListRef&&) = default;
  ConstStringListRef(const ConstStringListRef&) = default;

  ConstStringListRef(std::vector<std::string> value) {  // NOLINT
    variant_ = std::make_shared<Variant>(value);
  }
  ConstStringListRef(const std::vector<std::string>* value) {  // NOLINT
    variant_ = std::make_shared<Variant>(value);
  }
  ConstStringListRef(std::vector<std::string_view> value) {  // NOLINT
    variant_ = std::make_shared<Variant>(value);
  }
  ConstStringListRef(const std::vector<std::string_view>* value) {  // NOLINT
    variant_ = std::make_shared<Variant>(value);
  }
  ConstStringListRef(const std::vector<std::wstring>* value) {  // NOLINT
    variant_ = std::make_shared<Variant>(value);
  }
  ConstStringListRef(Adapter* adapter) {  // NOLINT
    variant_ = std::make_shared<Variant>(adapter);
  }
  template <typename AdapterType>
  ConstStringListRef(std::unique_ptr<AdapterType> adapter) {  // NOLINT
    variant_ = std::make_shared<Variant>(
        static_cast<std::unique_ptr<Adapter>>(std::move(adapter)));
  }

  size_t size() const {
    return variant_ ? std::visit(SizeVisitor(), *variant_) : 0;
  }

  std::string_view operator[](size_t i) const {
    return variant_ ? std::visit(IndexedGetter{i}, *variant_) : "";
  }

 private:
  struct IndexedGetter {
    size_t i;
    std::string_view operator()(const std::vector<std::string>& v) const {
      return v[i];
    }
    std::string_view operator()(const std::vector<std::string>* v) const {
      return (*v)[i];
    }
    std::string_view operator()(const std::vector<std::string_view>& v) const {
      return v[i];
    }
    std::string_view operator()(const std::vector<std::string_view>* v) const {
      return (*v)[i];
    }
    std::string_view operator()(
        [[maybe_unused]] const std::vector<std::wstring>* v) const {
      return "";  // Temporary fix: Cannot return a view to a temporary
                  // conversion.
    }
    std::string_view operator()(Adapter* v) const { return (*v)[i]; }
    std::string_view operator()(const std::unique_ptr<Adapter>& v) const {
      return (*v)[i];
    }
  };

  struct SizeVisitor {
    size_t operator()(const std::vector<std::string>& v) const {
      return v.size();
    }
    size_t operator()(const std::vector<std::string>* v) const {
      return v->size();
    }
    size_t operator()(const std::vector<std::string_view>& v) const {
      return v.size();
    }
    size_t operator()(const std::vector<std::string_view>* v) const {
      return v->size();
    }
    size_t operator()(const std::vector<std::wstring>* v) const {
      return v->size();
    }
    size_t operator()(const Adapter* v) const { return v->size(); }
    size_t operator()(const std::unique_ptr<Adapter>& v) const {
      return v->size();
    }
  };

  std::shared_ptr<Variant> variant_;
};

}  // namespace ftxui


namespace ftxui {
class Node;
using Element = std::shared_ptr<Node>;
using Elements = std::vector<Element>;
using Decorator = std::function<Element(Element)>;
using GraphFunction = std::function<std::vector<int>(int, int)>;

/// @brief BorderStyle is an enumeration that represents the different styles
/// of borders that can be applied to elements in the terminal UI.
///
/// BorderStyle is an enumeration that represents the different styles of
/// borders that can be applied to elements in the terminal UI.
/// It is used to define the visual appearance of borders around elements,
/// such as windows, frames, or separators.
/// @ingroup dom
enum BorderStyle : uint8_t {
  LIGHT,
  DASHED,
  HEAVY,
  DOUBLE,
  ROUNDED,
  EMPTY,
};

// Pipe elements into decorator together.
// For instance the next lines are equivalents:
// -> text("ftxui") | bold | underlined
// -> underlined(bold(text("FTXUI")))
FTXUI_EXPORT(DOM) Element operator|(Element, Decorator);
FTXUI_EXPORT(DOM) Element& operator|=(Element&, Decorator);
FTXUI_EXPORT(DOM) Elements operator|(Elements, Decorator);
FTXUI_EXPORT(DOM) Decorator operator|(Decorator, Decorator);

// --- Widget ---
FTXUI_EXPORT(DOM) Element text(std::string_view text);
FTXUI_EXPORT(DOM) Element vtext(std::string_view text);
FTXUI_EXPORT(DOM) Element separator();
FTXUI_EXPORT(DOM) Element separatorLight();
FTXUI_EXPORT(DOM) Element separatorDashed();
FTXUI_EXPORT(DOM) Element separatorHeavy();
FTXUI_EXPORT(DOM) Element separatorDouble();
FTXUI_EXPORT(DOM) Element separatorEmpty();
FTXUI_EXPORT(DOM) Element separatorStyled(BorderStyle);
FTXUI_EXPORT(DOM) Element separator(Cell);
FTXUI_EXPORT(DOM) Element separatorCharacter(std::string_view);
FTXUI_EXPORT(DOM)
Element separatorHSelector(float left,
                           float right,
                           Color unselected_color,
                           Color selected_color);
FTXUI_EXPORT(DOM)
Element separatorVSelector(float up,
                           float down,
                           Color unselected_color,
                           Color selected_color);
FTXUI_EXPORT(DOM) Element gauge(float progress);
FTXUI_EXPORT(DOM) Element gaugeLeft(float progress);
FTXUI_EXPORT(DOM) Element gaugeRight(float progress);
FTXUI_EXPORT(DOM) Element gaugeUp(float progress);
FTXUI_EXPORT(DOM) Element gaugeDown(float progress);
FTXUI_EXPORT(DOM) Element gaugeDirection(float progress, Direction direction);

// charset[0] is the "empty" glyph, charset.back() is "full"; entries in
// between shade the boundary cell.
FTXUI_EXPORT(DOM)
Element gaugeCharset(float progress,
                      std::vector<std::string> charset,
                      Direction direction = Direction::Right);
FTXUI_EXPORT(DOM) Element border(Element);
FTXUI_EXPORT(DOM) Element borderLight(Element);
FTXUI_EXPORT(DOM) Element borderDashed(Element);
FTXUI_EXPORT(DOM) Element borderHeavy(Element);
FTXUI_EXPORT(DOM) Element borderDouble(Element);
FTXUI_EXPORT(DOM) Element borderRounded(Element);
FTXUI_EXPORT(DOM) Element borderEmpty(Element);
FTXUI_EXPORT(DOM) Decorator borderStyled(BorderStyle);
FTXUI_EXPORT(DOM) Decorator borderStyled(BorderStyle, Color);
FTXUI_EXPORT(DOM) Decorator borderStyled(Color);
FTXUI_EXPORT(DOM) Decorator borderWith(const Cell&);
FTXUI_EXPORT(DOM)
Element window(Element title, Element content, BorderStyle border = ROUNDED);
FTXUI_EXPORT(DOM) Element spinner(int charset_index, size_t image_index);
FTXUI_EXPORT(DOM) Element paragraph(std::string_view text);
FTXUI_EXPORT(DOM) Element paragraphAlignLeft(std::string_view text);
FTXUI_EXPORT(DOM) Element paragraphAlignRight(std::string_view text);
FTXUI_EXPORT(DOM) Element paragraphAlignCenter(std::string_view text);
FTXUI_EXPORT(DOM) Element paragraphAlignJustify(std::string_view text);
FTXUI_EXPORT(DOM) Element graph(GraphFunction);
FTXUI_EXPORT(DOM) Element emptyElement();
FTXUI_EXPORT(DOM) Element canvas(ConstRef<Canvas>);
FTXUI_EXPORT(DOM)
Element canvas(int width, int height, std::function<void(Canvas&)>);
FTXUI_EXPORT(DOM) Element canvas(std::function<void(Canvas&)>);

// -- Decorator ---
FTXUI_EXPORT(DOM) Element bold(Element);
FTXUI_EXPORT(DOM) Element dim(Element);
FTXUI_EXPORT(DOM) Element italic(Element);
FTXUI_EXPORT(DOM) Element inverted(Element);
FTXUI_EXPORT(DOM) Element underlined(Element);
FTXUI_EXPORT(DOM) Element underlinedDouble(Element);
FTXUI_EXPORT(DOM) Element blink(Element);
FTXUI_EXPORT(DOM) Element strikethrough(Element);
FTXUI_EXPORT(DOM) Decorator color(Color);
FTXUI_EXPORT(DOM) Decorator bgcolor(Color);
FTXUI_EXPORT(DOM) Decorator color(const LinearGradient&);
FTXUI_EXPORT(DOM) Decorator bgcolor(const LinearGradient&);
FTXUI_EXPORT(DOM) Element color(Color, Element);
FTXUI_EXPORT(DOM) Element bgcolor(Color, Element);
FTXUI_EXPORT(DOM) Element color(const LinearGradient&, Element);
FTXUI_EXPORT(DOM) Element bgcolor(const LinearGradient&, Element);
FTXUI_EXPORT(DOM) Decorator focusPosition(int x, int y);
FTXUI_EXPORT(DOM) Decorator focusPositionRelative(float x, float y);
FTXUI_EXPORT(DOM) Element automerge(Element child);
FTXUI_EXPORT(DOM) Decorator hyperlink(std::string_view link);
FTXUI_EXPORT(DOM) Element hyperlink(std::string_view link, Element child);
FTXUI_EXPORT(DOM) Element selectionStyleReset(Element);
FTXUI_EXPORT(DOM) Decorator selectionColor(Color foreground);
FTXUI_EXPORT(DOM) Decorator selectionBackgroundColor(Color foreground);
FTXUI_EXPORT(DOM) Decorator selectionForegroundColor(Color foreground);
FTXUI_EXPORT(DOM) Decorator selectionStyle(std::function<void(Cell&)> style);

// --- Layout is
// Horizontal, Vertical or stacked set of elements.
FTXUI_EXPORT(DOM) Element hbox(Elements);
FTXUI_EXPORT(DOM) Element vbox(Elements);
FTXUI_EXPORT(DOM) Element dbox(Elements);
FTXUI_EXPORT(DOM)
Element flexbox(Elements, FlexboxConfig config = FlexboxConfig());
FTXUI_EXPORT(DOM) Element gridbox(std::vector<Elements> lines);

FTXUI_EXPORT(DOM)
Element hflow(Elements);  // Helper: default flexbox with row direction.
FTXUI_EXPORT(DOM)
Element vflow(Elements);  // Helper: default flexbox with column direction.

// -- Flexibility ---
// Define how to share the remaining space when not all of it is used inside a
// container.
FTXUI_EXPORT(DOM) Element flex(Element);  // Expand/Minimize if possible/needed.
FTXUI_EXPORT(DOM) Element flex_grow(Element);    // Expand element if possible.
FTXUI_EXPORT(DOM) Element flex_shrink(Element);  // Minimize element if needed.
FTXUI_EXPORT(DOM) Decorator flex_factor(int grow, int shrink);
FTXUI_EXPORT(DOM) Decorator flex_grow_factor(int grow);
FTXUI_EXPORT(DOM) Decorator flex_shrink_factor(int shrink);

FTXUI_EXPORT(DOM)
Element xflex(Element);  // Expand/Minimize if possible/needed on X axis.
FTXUI_EXPORT(DOM)
Element xflex_grow(Element);  // Expand element if possible on X axis.
FTXUI_EXPORT(DOM)
Element xflex_shrink(Element);  // Minimize element if needed on X axis.
FTXUI_EXPORT(DOM) Decorator xflex_factor(int grow, int shrink);
FTXUI_EXPORT(DOM) Decorator xflex_grow_factor(int grow);
FTXUI_EXPORT(DOM) Decorator xflex_shrink_factor(int shrink);

FTXUI_EXPORT(DOM)
Element yflex(Element);  // Expand/Minimize if possible/needed on Y axis.
FTXUI_EXPORT(DOM)
Element yflex_grow(Element);  // Expand element if possible on Y axis.
FTXUI_EXPORT(DOM)
Element yflex_shrink(Element);  // Minimize element if needed on Y axis.
FTXUI_EXPORT(DOM) Decorator yflex_factor(int grow, int shrink);
FTXUI_EXPORT(DOM) Decorator yflex_grow_factor(int grow);
FTXUI_EXPORT(DOM) Decorator yflex_shrink_factor(int shrink);

FTXUI_EXPORT(DOM) Element notflex(Element);  // Reset the flex attribute.
FTXUI_EXPORT(DOM) Element filler();          // A blank expandable element.

// -- Size override;
enum WidthOrHeight : uint8_t { WIDTH, HEIGHT };
enum Constraint : uint8_t { LESS_THAN, EQUAL, GREATER_THAN };
FTXUI_EXPORT(DOM) Decorator size(WidthOrHeight, Constraint, int value);

// --- Frame ---
// A frame is a scrollable area. The internal area is potentially larger than
// the external one. The internal area is scrolled in order to make visible the
// focused element.
FTXUI_EXPORT(DOM) Element frame(Element);
FTXUI_EXPORT(DOM) Element xframe(Element);
FTXUI_EXPORT(DOM) Element yframe(Element);
FTXUI_EXPORT(DOM) Element focus(Element);
FTXUI_EXPORT(DOM) Element select(Element e);  // Deprecated - Alias for focus.

// --- Cursor ---
// Those are similar to `focus`, but also change the shape of the cursor.
FTXUI_EXPORT(DOM) Element focusCursorBlock(Element);
FTXUI_EXPORT(DOM) Element focusCursorBlockBlinking(Element);
FTXUI_EXPORT(DOM) Element focusCursorBar(Element);
FTXUI_EXPORT(DOM) Element focusCursorBarBlinking(Element);
FTXUI_EXPORT(DOM) Element focusCursorUnderline(Element);
FTXUI_EXPORT(DOM) Element focusCursorUnderlineBlinking(Element);

// --- Misc ---
FTXUI_EXPORT(DOM) Element vscroll_indicator(Element);
FTXUI_EXPORT(DOM) Element hscroll_indicator(Element);
FTXUI_EXPORT(DOM) Decorator reflect(Box& box);
// Before drawing the |element| clear the pixel below. This is useful in
// combination with dbox.
FTXUI_EXPORT(DOM) Element clear_under(Element element);

// --- Util --------------------------------------------------------------------
FTXUI_EXPORT(DOM) Element hcenter(Element);
FTXUI_EXPORT(DOM) Element vcenter(Element);
FTXUI_EXPORT(DOM) Element center(Element);
FTXUI_EXPORT(DOM) Element align_right(Element);
FTXUI_EXPORT(DOM) Element nothing(Element element);

namespace Dimension {
FTXUI_EXPORT(DOM) Dimensions Fit(Element&, bool extend_beyond_screen = false);
}  // namespace Dimension

}  // namespace ftxui

// Make container able to take any number of children as input.

// IWYU pragma: private, include "ftxui/dom/elements.hpp"

namespace ftxui {

inline void Merge(Elements& container, Element element) {
  container.push_back(std::move(element));
}

// Turn a set of arguments into a vector.
template <class... Args>
Elements unpack(Args... args) {
  std::vector<Element> vec;
  (Merge(vec, std::move(args)), ...);
  return vec;
}

// Make |container| able to take any number of arguments.
#define TAKE_ANY_ARGS(container)                               \
  inline Element container(Element child) {                    \
    return container(unpack(std::move(child)));                \
  }                                                            \
                                                               \
  template <class... Args>                                     \
  inline Element container(Args... children) {                 \
    return container(unpack(std::forward<Args>(children)...)); \
  }                                                            \
                                                               \
  template <class Container>                                   \
  inline Element container(Container&& children) {             \
    Elements elements;                                         \
    for (auto& child : children) {                             \
      elements.push_back(std::move(child));                    \
    }                                                          \
    return container(std::move(elements));                     \
  }                                                            \
  template <>                                                  \
  inline Element container(std::stack<Element>&& children) {   \
    Elements elements;                                         \
    while (!children.empty()) {                                \
      elements.push_back(std::move(children.top()));           \
      children.pop();                                          \
    }                                                          \
    return container(std::move(elements));                     \
  }                                                            \
  template <>                                                  \
  inline Element container(std::queue<Element>&& children) {   \
    Elements elements;                                         \
    while (!children.empty()) {                                \
      elements.push_back(std::move(children.front()));         \
      children.pop();                                          \
    }                                                          \
    return container(std::move(elements));                     \
  }                                                            \
  template <>                                                  \
  inline Element container(std::deque<Element>&& children) {   \
    Elements elements;                                         \
    for (auto& child : children) {                             \
      elements.push_back(std::move(child));                    \
    }                                                          \
    return container(std::move(elements));                     \
  }

TAKE_ANY_ARGS(vbox)
TAKE_ANY_ARGS(hbox)
TAKE_ANY_ARGS(dbox)
TAKE_ANY_ARGS(hflow)
}  // namespace ftxui


// Include old definitions using wstring.


namespace ftxui {
FTXUI_EXPORT(DOM) Element text(std::wstring_view text);
FTXUI_EXPORT(DOM) Element vtext(std::wstring_view text);
}  // namespace ftxui


namespace ftxui {

class Delegate;
class Focus;
struct Event;

namespace animation {
class Params;
}  // namespace animation

class ComponentBase;
using Component = std::shared_ptr<ComponentBase>;
using Components = std::vector<Component>;

/// @brief It implement rendering itself as ftxui::Element. It implement
/// keyboard navigation by responding to ftxui::Event.
/// @ingroup component
class FTXUI_EXPORT(COMPONENT) ComponentBase {
 public:
  explicit ComponentBase(Components children);
  virtual ~ComponentBase();
  ComponentBase();

  // A component is not copyable/movable.
  ComponentBase(const ComponentBase&) = delete;
  ComponentBase(ComponentBase&&) = delete;
  ComponentBase& operator=(const ComponentBase&) = delete;
  ComponentBase& operator=(ComponentBase&&) = delete;

  // Component hierarchy:
  ComponentBase* Parent() const;
  Component& ChildAt(size_t i);
  size_t ChildCount() const;
  int Index() const;
  void Add(Component children);
  void Detach();
  void DetachAllChildren();

  // Renders the component.
  Element Render();

  // Override this function modify how `Render` works.
  virtual Element OnRender();

  // Handles an event.
  // By default, reduce on children with a lazy OR.
  //
  // Returns whether the event was handled or not.
  virtual bool OnEvent(Event);

  // Handle an animation step.
  virtual void OnAnimation(animation::Params& params);

  // Focus management ----------------------------------------------------------
  //
  // If this component contains children, this indicates which one is active,
  // nullptr if none is active.
  //
  // We say an element has the focus if the chain of ActiveChild() from the
  // root component contains this object.
  virtual Component ActiveChild();

  // Return true when the component contains focusable elements.
  // The non focusable Component will be skipped when navigating using the
  // keyboard.
  virtual bool Focusable() const;

  // Whether this is the active child of its parent.
  bool Active() const;
  // Whether all the ancestors are active.
  bool Focused() const;

  // Make the |child| to be the "active" one.
  virtual void SetActiveChild(ComponentBase* child);
  void SetActiveChild(Component child);

  // Configure all the ancestors to give focus to this component.
  void TakeFocus();

  // ABI Reserve:
  virtual void Reserved1();
  virtual void Reserved2();
  virtual void Reserved3();
  virtual void Reserved4();
  virtual void Reserved5();
  virtual void Reserved6();
  virtual void Reserved7();
  virtual void Reserved8();

 protected:
  CapturedMouse CaptureMouse(const Event& event);

  Components& children();
  const Components& children() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ftxui




namespace ftxui {

/// @brief arguments for transform from |ButtonOption|, |CheckboxOption|,
/// |RadioboxOption|, |MenuEntryOption|, |MenuOption|.
struct FTXUI_EXPORT(COMPONENT) EntryState {
  std::string label;  ///< The label to display.
  bool state;         ///< The state of the button/checkbox/radiobox
  bool active;        ///< Whether the entry is the active one.
  bool focused;       ///< Whether the entry is one focused by the user.
  int index;          ///< Index of the entry when applicable or -1.
};

/// @brief Option for the underline effect.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) UnderlineOption {
  bool enabled = false;

  Color color_active = Color::White;
  Color color_inactive = Color::GrayDark;

  animation::easing::Function leader_function =
      animation::easing::QuadraticInOut;
  animation::easing::Function follower_function =
      animation::easing::QuadraticInOut;

  animation::Duration leader_duration = std::chrono::milliseconds(250);
  animation::Duration leader_delay = std::chrono::milliseconds(0);
  animation::Duration follower_duration = std::chrono::milliseconds(250);
  animation::Duration follower_delay = std::chrono::milliseconds(0);

  void SetAnimation(animation::Duration d, animation::easing::Function f);
  void SetAnimationDuration(animation::Duration d);
  void SetAnimationFunction(animation::easing::Function f);
  void SetAnimationFunction(animation::easing::Function f_leader,
                            animation::easing::Function f_follower);
};

/// @brief Option about a potentially animated color.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) AnimatedColorOption {
  void Set(
      Color inactive,
      Color active,
      animation::Duration duration = std::chrono::milliseconds(250),
      animation::easing::Function function = animation::easing::QuadraticInOut);

  bool enabled = false;
  Color inactive;
  Color active;
  animation::Duration duration = std::chrono::milliseconds(250);
  animation::easing::Function function = animation::easing::QuadraticInOut;
};

struct FTXUI_EXPORT(COMPONENT) AnimatedColorsOption {
  AnimatedColorOption background;
  AnimatedColorOption foreground;
};

/// @brief Option for the MenuEntry component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) MenuEntryOption {
  ConstStringRef label = "MenuEntry";
  std::function<Element(const EntryState& state)> transform;
  AnimatedColorsOption animated_colors;
};

/// @brief Option for the Menu component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) MenuOption {
  // Standard constructors:
  static MenuOption Horizontal();
  static MenuOption HorizontalAnimated();
  static MenuOption Vertical();
  static MenuOption VerticalAnimated();
  static MenuOption Toggle();

  ConstStringListRef entries;  ///> The list of entries.
  Ref<int> selected = 0;       ///> The index of the selected entry.

  // Style:
  UnderlineOption underline;
  MenuEntryOption entries_option;
  Direction direction = Direction::Down;
  std::function<Element()> elements_prefix;
  std::function<Element()> elements_infix;
  std::function<Element()> elements_postfix;

  // Observers:
  std::function<void()> on_change;  ///> Called when the selected entry changes.
  std::function<void()> on_enter;   ///> Called when the user presses enter.
  Ref<int> focused_entry = 0;
};

/// @brief Option for the AnimatedButton component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) ButtonOption {
  // Standard constructors:
  static ButtonOption Ascii();
  static ButtonOption Simple();
  static ButtonOption Border();
  static ButtonOption Animated();
  static ButtonOption Animated(Color color);
  static ButtonOption Animated(Color background, Color foreground);
  static ButtonOption Animated(Color background,
                               Color foreground,
                               Color background_active,
                               Color foreground_active);

  ConstStringRef label = "Button";
  std::function<void()> on_click = [] {};

  // Style:
  std::function<Element(const EntryState&)> transform;
  AnimatedColorsOption animated_colors;
};

/// @brief Option for the Checkbox component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) CheckboxOption {
  // Standard constructors:
  static CheckboxOption Simple();

  ConstStringRef label = "Checkbox";

  Ref<bool> checked = false;

  // Style:
  std::function<Element(const EntryState&)> transform;

  // Observer:
  /// Called when the user change the state.
  std::function<void()> on_change = [] {};
};

/// @brief Used to define style for the Input component.
struct FTXUI_EXPORT(COMPONENT) InputState {
  Element element;
  bool hovered;         ///< Whether the input is hovered by the mouse.
  bool focused;         ///< Whether the input is focused by the user.
  bool is_placeholder;  ///< Whether the input is empty and displaying the
                        ///< placeholder.
};

/// @brief Option for the Input component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) InputOption {
  // A set of predefined styles:

  /// @brief Create the default input style:
  static InputOption Default();
  /// @brief A white on black style with high margins:
  static InputOption Spacious();

  /// The content of the input.
  StringRef content = "";

  /// The content of the input when it's empty.
  StringRef placeholder = "";

  // Style:
  std::function<Element(InputState)> transform;
  Ref<bool> password = false;  ///< Obscure the input content using '*'.
  Ref<bool> multiline = true;  ///< Whether the input can be multiline.
  Ref<bool> insert = true;     ///< Insert or overtype character mode.

  /// Called when the content changes.
  std::function<void()> on_change = [] {};
  /// Called when the user presses enter.
  std::function<void()> on_enter = [] {};

  // The char position of the cursor:
  Ref<int> cursor_position = 0;
};

/// @brief Option for the Radiobox component.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) RadioboxOption {
  // Standard constructors:
  static RadioboxOption Simple();

  // Content:
  ConstStringListRef entries;
  Ref<int> selected = 0;

  // Style:
  std::function<Element(const EntryState&)> transform;

  // Observers:
  /// Called when the selected entry changes.
  std::function<void()> on_change = [] {};
  Ref<int> focused_entry = 0;
};

struct FTXUI_EXPORT(COMPONENT) ResizableSplitOption {
  Component main;
  Component back;
  Ref<Direction> direction = Direction::Left;
  Ref<int> main_size =
      (direction() == Direction::Left || direction() == Direction::Right) ? 20
                                                                          : 10;
  std::function<Element()> separator_func = [] { return ::ftxui::separator(); };

  // Constraints on main_size:
  Ref<int> min = 0;
  Ref<int> max = std::numeric_limits<int>::max();
};

// @brief Option for the `Slider` component.
// @ingroup component
template <typename T>
struct SliderOption {
  Ref<T> value;
  ConstRef<T> min = T(0);
  ConstRef<T> max = T(100);
  ConstRef<T> increment = (max() - min()) / 20;
  Direction direction = Direction::Right;
  Color color_active = Color::White;
  Color color_inactive = Color::GrayDark;
  std::function<void()> on_change;  ///> Called when `value` is updated.
};

/// @brief State passed to the `Window` component's render function.
/// @ingroup component
struct FTXUI_EXPORT(COMPONENT) WindowRenderState {
  Element inner;             ///< The element wrapped inside this window.
  const std::string& title;  ///< The title of the window.
  bool active = false;       ///< Whether the window is the active one.
  bool drag = false;         ///< Whether the window is being dragged.
  bool resize = false;       ///< Whether the window is being resized.
  bool hover_left = false;   ///< Whether the resizeable left side is hovered.
  bool hover_right = false;  ///< Whether the resizeable right side is hovered.
  bool hover_top = false;    ///< Whether the resizeable top side is hovered.
  bool hover_down = false;   ///< Whether the resizeable down side is hovered.
};

// @brief Option for the `Window` component.
// @ingroup component
struct FTXUI_EXPORT(COMPONENT) WindowOptions {
  Component inner;            ///< The component wrapped by this window.
  ConstStringRef title = "";  ///< The title displayed by this window.

  Ref<int> left = 0;     ///< The left side position of the window.
  Ref<int> top = 0;      ///< The top side position of the window.
  Ref<int> width = 20;   ///< The width of the window.
  Ref<int> height = 10;  ///< The height of the window.

  Ref<bool> resize_left = true;   ///< Can the left side be resized?
  Ref<bool> resize_right = true;  ///< Can the right side be resized?
  Ref<bool> resize_top = true;    ///< Can the top side be resized?
  Ref<bool> resize_down = true;   ///< Can the down side be resized?

  /// An optional function to customize how the window looks like:
  std::function<Element(const WindowRenderState&)> render;
};

/// @brief Option for the Dropdown component.
/// @ingroup component
/// A dropdown menu is a checkbox opening/closing a radiobox.
struct FTXUI_EXPORT(COMPONENT) DropdownOption {
  /// Whether the dropdown is open or closed. Takes precedence over
  /// `checkbox.checked`, unless only the latter is bound to a variable.
  Ref<bool> open = false;
  // The options for the checkbox:
  CheckboxOption checkbox;
  // The options for the radiobox:
  RadioboxOption radiobox;
  // The transformation function:
  std::function<Element(bool open, Element checkbox, Element radiobox)>
      transform;
};

}  // namespace ftxui


namespace ftxui {
struct ButtonOption;
struct CheckboxOption;
struct Event;
struct InputOption;
struct MenuOption;
struct RadioboxOption;
struct MenuEntryOption;

template <class T, class... Args>
std::shared_ptr<T> Make(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

// Pipe operator to decorate components.
using ComponentDecorator = std::function<Component(Component)>;
using ElementDecorator = std::function<Element(Element)>;
FTXUI_EXPORT(COMPONENT)
Component operator|(Component component, ComponentDecorator decorator);
FTXUI_EXPORT(COMPONENT)
Component operator|(Component component, ElementDecorator decorator);
FTXUI_EXPORT(COMPONENT)
Component& operator|=(Component& component, ComponentDecorator decorator);
FTXUI_EXPORT(COMPONENT)
Component& operator|=(Component& component, ElementDecorator decorator);

namespace Container {
FTXUI_EXPORT(COMPONENT) Component Vertical(Components children);
FTXUI_EXPORT(COMPONENT) Component Vertical(Components children, int* selector);
FTXUI_EXPORT(COMPONENT) Component Horizontal(Components children);
FTXUI_EXPORT(COMPONENT)
Component Horizontal(Components children, int* selector);
FTXUI_EXPORT(COMPONENT) Component Tab(Components children, int* selector);
FTXUI_EXPORT(COMPONENT) Component Stacked(Components children);
}  // namespace Container

FTXUI_EXPORT(COMPONENT) Component Button(ButtonOption options);
FTXUI_EXPORT(COMPONENT)
Component Button(ConstStringRef label,
                 std::function<void()> on_click,
                 ButtonOption options = ButtonOption::Simple());

FTXUI_EXPORT(COMPONENT) Component Checkbox(CheckboxOption options);
FTXUI_EXPORT(COMPONENT)
Component Checkbox(ConstStringRef label,
                   bool* checked,
                   CheckboxOption options = CheckboxOption::Simple());

FTXUI_EXPORT(COMPONENT) Component Input(InputOption options = {});
FTXUI_EXPORT(COMPONENT)
Component Input(StringRef content, InputOption options = {});
FTXUI_EXPORT(COMPONENT)
Component Input(StringRef content,
                StringRef placeholder,
                InputOption options = {});

FTXUI_EXPORT(COMPONENT) Component Menu(MenuOption options);
FTXUI_EXPORT(COMPONENT)
Component Menu(ConstStringListRef entries,
               int* selected_,
               MenuOption options = MenuOption::Vertical());
FTXUI_EXPORT(COMPONENT) Component MenuEntry(MenuEntryOption options);
FTXUI_EXPORT(COMPONENT)
Component MenuEntry(ConstStringRef label, MenuEntryOption options = {});

FTXUI_EXPORT(COMPONENT) Component Radiobox(RadioboxOption options);
FTXUI_EXPORT(COMPONENT)
Component Radiobox(ConstStringListRef entries,
                   int* selected_,
                   RadioboxOption options = {});

FTXUI_EXPORT(COMPONENT)
Component Dropdown(ConstStringListRef entries, int* selected);
FTXUI_EXPORT(COMPONENT) Component Dropdown(DropdownOption options);

FTXUI_EXPORT(COMPONENT)
Component Toggle(ConstStringListRef entries, int* selected);

// General slider constructor:
template <typename T>
Component Slider(SliderOption<T> options);

extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int8_t>);
extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int16_t>);
extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int32_t>);
extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int64_t>);

extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<uint8_t>);
extern template FTXUI_EXPORT(COMPONENT) Component
    Slider(SliderOption<uint16_t>);
extern template FTXUI_EXPORT(COMPONENT) Component
    Slider(SliderOption<uint32_t>);
extern template FTXUI_EXPORT(COMPONENT) Component
    Slider(SliderOption<uint64_t>);

extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<float>);
extern template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<double>);

// Shorthand without the `SliderOption` constructor:
FTXUI_EXPORT(COMPONENT)
Component Slider(ConstStringRef label,
                 Ref<int> value,
                 ConstRef<int> min = 0,
                 ConstRef<int> max = 100,
                 ConstRef<int> increment = 5);
FTXUI_EXPORT(COMPONENT)
Component Slider(ConstStringRef label,
                 Ref<float> value,
                 ConstRef<float> min = 0.f,
                 ConstRef<float> max = 100.f,
                 ConstRef<float> increment = 5.f);
FTXUI_EXPORT(COMPONENT)
Component Slider(ConstStringRef label,
                 Ref<long> value,
                 ConstRef<long> min = 0L,
                 ConstRef<long> max = 100L,
                 ConstRef<long> increment = 5L);

FTXUI_EXPORT(COMPONENT)
Component ResizableSplit(ResizableSplitOption options);
FTXUI_EXPORT(COMPONENT)
Component ResizableSplitLeft(Component main, Component back, int* main_size);
FTXUI_EXPORT(COMPONENT)
Component ResizableSplitRight(Component main, Component back, int* main_size);
FTXUI_EXPORT(COMPONENT)
Component ResizableSplitTop(Component main, Component back, int* main_size);
FTXUI_EXPORT(COMPONENT)
Component ResizableSplitBottom(Component main, Component back, int* main_size);

FTXUI_EXPORT(COMPONENT)
Component Renderer(Component child, std::function<Element()>);
FTXUI_EXPORT(COMPONENT) Component Renderer(std::function<Element()>);
FTXUI_EXPORT(COMPONENT)
Component Renderer(std::function<Element(bool /* focused */)>);
FTXUI_EXPORT(COMPONENT) ComponentDecorator Renderer(ElementDecorator);

FTXUI_EXPORT(COMPONENT)
Component CatchEvent(Component child, std::function<bool(Event)>);
FTXUI_EXPORT(COMPONENT)
ComponentDecorator CatchEvent(std::function<bool(Event)> on_event);

FTXUI_EXPORT(COMPONENT) Component Maybe(Component, const bool* show);
FTXUI_EXPORT(COMPONENT) Component Maybe(Component, std::function<bool()>);
FTXUI_EXPORT(COMPONENT) ComponentDecorator Maybe(const bool* show);
FTXUI_EXPORT(COMPONENT) ComponentDecorator Maybe(std::function<bool()>);

FTXUI_EXPORT(COMPONENT)
Component Modal(Component main, Component modal, const bool* show_modal);
FTXUI_EXPORT(COMPONENT)
ComponentDecorator Modal(Component modal, const bool* show_modal);

FTXUI_EXPORT(COMPONENT)
Component Collapsible(ConstStringRef label,
                      Component child,
                      Ref<bool> show = false);

FTXUI_EXPORT(COMPONENT)
Component Hoverable(Component component, bool* hover);
FTXUI_EXPORT(COMPONENT)
Component Hoverable(Component component,
                    std::function<void()> on_enter,
                    std::function<void()> on_leave);
FTXUI_EXPORT(COMPONENT)
Component Hoverable(Component component,  //
                    std::function<void(bool)> on_change);
FTXUI_EXPORT(COMPONENT) ComponentDecorator Hoverable(bool* hover);
FTXUI_EXPORT(COMPONENT)
ComponentDecorator Hoverable(std::function<void()> on_enter,
                             std::function<void()> on_leave);
FTXUI_EXPORT(COMPONENT)
ComponentDecorator Hoverable(std::function<void(bool)> on_change);

FTXUI_EXPORT(COMPONENT) Component Window(WindowOptions option);

}  // namespace ftxui




namespace ftxui {
class ComponentBase;

using Component = std::shared_ptr<ComponentBase>;
class App;

/// @brief Loop is a class that manages the event loop for a component.
///
/// It is responsible for running the component, handling events, and
/// updating the screen.
///
/// The Loop class is designed to be used with an App object,
/// which represents the terminal screen.
///
/// **Example**
/// ```cpp
/// #include <ftxui/component/component.hpp>
/// #include <ftxui/component/app.hpp>
/// #include <ftxui/component/loop.hpp>
///
/// int main() {
///  auto screen = ftxui::App::TerminalOutput();
///  auto component = ftxui::Button("Click me", [] { ... });
///
///  ftxui::Loop loop(screen.get(), component);
///
///  // Either
///  loop.Run();  // Blocking until the component quits.
///
///  // Or
///  loop.RunOnce();  // Non-blocking, returns immediately.
///
///  // Or
///  loop.RunOnceBlocking();  // Blocking until handling one event.
///
///  // Or in a loop:
///  while (!loop.HasQuitted()) {
///    loop.RunOnce();
///
///    // Do something else like running a different library loop function.
///  }
/// }
/// ```
///
/// @ingroup component
class FTXUI_EXPORT(COMPONENT) Loop {
 public:
  Loop(App* screen, Component component);
  ~Loop();

  bool HasQuitted();
  void RunOnce();
  void RunOnceBlocking();
  void Run();

  // This class is non copyable/movable.
  Loop& operator=(Loop&&) = delete;
  Loop& operator=(const Loop&) = delete;
  Loop(Loop&&) = delete;
  Loop(const Loop&) = delete;

 private:
  App* screen_;
  Component component_;
};

}  // namespace ftxui



namespace ftxui {

// Deprecated
//
// Usage:
//
// Initialization:
// ---------------
//
// auto receiver = MakeReceiver<std:string>();
// auto sender_1= receiver->MakeSender();
// auto sender_2 = receiver->MakeSender();
//
// Then move the senders elsewhere, potentially in a different thread.
//
// On the producer side:
// ----------------------
// [thread 1] sender_1->Send("hello");
// [thread 2] sender_2->Send("world");
//
// On the consumer side:
// ---------------------
// char c;
// while(receiver->Receive(&c)) // Return true as long as there is a producer.
//   print(c)
//
// Receiver::Receive() returns true when there are no more senders.

// clang-format off
// Deprecated:
template<class T> class SenderImpl;
// Deprecated:
template<class T> class ReceiverImpl;
// Deprecated:

// Deprecated:
template<class T> using Sender = std::unique_ptr<SenderImpl<T>>;
// Deprecated:
template<class T> using Receiver = std::unique_ptr<ReceiverImpl<T>>;
// Deprecated:
template<class T> Receiver<T> MakeReceiver();
// clang-format on

// ---- Implementation part ----

template <class T>
// Deprecated:
class SenderImpl {
 public:
  SenderImpl(const SenderImpl&) = delete;
  SenderImpl(SenderImpl&&) = delete;
  SenderImpl& operator=(const SenderImpl&) = delete;
  SenderImpl& operator=(SenderImpl&&) = delete;
  void Send(T t) { receiver_->Receive(std::move(t)); }
  ~SenderImpl() { receiver_->ReleaseSender(); }

  Sender<T> Clone() { return receiver_->MakeSender(); }

 private:
  friend class ReceiverImpl<T>;
  explicit SenderImpl(ReceiverImpl<T>* consumer) : receiver_(consumer) {}
  ReceiverImpl<T>* receiver_;
};

template <class T>
class ReceiverImpl {
 public:
  Sender<T> MakeSender() {
    std::unique_lock<std::mutex> lock(mutex_);
    senders_++;
    return std::unique_ptr<SenderImpl<T>>(new SenderImpl<T>(this));
  }
  ReceiverImpl() = default;

  bool Receive(T* t) {
    while (senders_ || !queue_.empty()) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (queue_.empty()) {
        notifier_.wait(lock);
      }
      if (queue_.empty()) {
        continue;
      }
      *t = std::move(queue_.front());
      queue_.pop();
      return true;
    }
    return false;
  }

  bool ReceiveNonBlocking(T* t) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    *t = queue_.front();
    queue_.pop();
    return true;
  }

  bool HasPending() {
    std::unique_lock<std::mutex> lock(mutex_);
    return !queue_.empty();
  }

  bool HasQuitted() {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty() && !senders_;
  }

 private:
  friend class SenderImpl<T>;

  void Receive(T t) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      queue_.push(std::move(t));
    }
    notifier_.notify_one();
  }

  void ReleaseSender() {
    senders_--;
    notifier_.notify_one();
  }

  std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable notifier_;
  std::atomic<int> senders_{0};
};

template <class T>
Receiver<T> MakeReceiver() {
  return std::make_unique<ReceiverImpl<T>>();
}

}  // namespace ftxui



namespace ftxui {
using ScreenInteractive = App;
}  // namespace ftxui




namespace ftxui {

class Table;
class TableSelection;

/// @brief Table is a utility to draw tables.
///
/// **example**
/// ```cpp
/// auto table = Table({
///  {"X", "Y"},
///  {"-1", "1"},
///  {"+0", "0"},
///  {"+1", "1"},
/// });
///
/// table.SelectAll().Border(LIGHT);
/// table.SelectRow(1).Border(DOUBLE);
/// table.SelectRow(1).SeparatorInternal(LIGHT);
///
/// std::move(table).Render();
/// ```
///
/// @ingroup dom
class FTXUI_EXPORT(DOM) Table {
 public:
  Table();
  explicit Table(const std::vector<std::vector<std::string>>&);
  explicit Table(std::vector<std::vector<Element>>);
  Table(std::initializer_list<std::vector<std::string>> init);
  TableSelection SelectAll();
  TableSelection SelectCell(int column, int row);
  TableSelection SelectRow(int row_index);
  TableSelection SelectRows(int row_min, int row_max);
  TableSelection SelectColumn(int column_index);
  TableSelection SelectColumns(int column_min, int column_max);
  TableSelection SelectRectangle(int column_min,
                                 int column_max,
                                 int row_min,
                                 int row_max);
  Element Render();

 private:
  void Initialize(std::vector<std::vector<Element>>);
  friend TableSelection;
  std::vector<std::vector<Element>> elements_;
  int input_dim_x_ = 0;
  int input_dim_y_ = 0;
  int dim_x_ = 0;
  int dim_y_ = 0;
};

class FTXUI_EXPORT(DOM) TableSelection {
 public:
  // Decorate the whole selection with a decorator.
  void Decorate(const Decorator&);
  void DecorateAlternateRow(const Decorator&, int modulo = 2, int shift = 0);
  void DecorateAlternateColumn(const Decorator&, int modulo = 2, int shift = 0);

  // Decorate only the cells of the selection with a decorator.
  void DecorateCells(const Decorator&);
  void DecorateCellsAlternateColumn(const Decorator&,
                                    int modulo = 2,
                                    int shift = 0);
  void DecorateCellsAlternateRow(const Decorator&,
                                 int modulo = 2,
                                 int shift = 0);

  // Decorate only the border of the selection with a decorator.
  void DecorateBorder(const Decorator&);
  void DecorateBorderLeft(const Decorator&);
  void DecorateBorderRight(const Decorator&);
  void DecorateBorderTop(const Decorator&);
  void DecorateBorderBottom(const Decorator&);

  // Decorate only the separator of the selection with a decorator.
  void DecorateSeparator(const Decorator&);
  void DecorateSeparatorVertical(const Decorator&);
  void DecorateSeparatorHorizontal(const Decorator&);

  // Decorate the border of the selection with a border style and a decorator.
  void Border(BorderStyle border = LIGHT);
  void Border(BorderStyle, const Decorator&);
  void BorderLeft(BorderStyle border = LIGHT);
  void BorderLeft(BorderStyle, const Decorator&);
  void BorderRight(BorderStyle border = LIGHT);
  void BorderRight(BorderStyle, const Decorator&);
  void BorderTop(BorderStyle border = LIGHT);
  void BorderTop(BorderStyle, const Decorator&);
  void BorderBottom(BorderStyle border = LIGHT);
  void BorderBottom(BorderStyle, const Decorator&);

  // Decorate the separator of the selection with a border style and a
  // decorator.
  void Separator(BorderStyle border = LIGHT);
  void Separator(BorderStyle, const Decorator&);
  void SeparatorVertical(BorderStyle border = LIGHT);
  void SeparatorVertical(BorderStyle, const Decorator&);
  void SeparatorHorizontal(BorderStyle border = LIGHT);
  void SeparatorHorizontal(BorderStyle, const Decorator&);

 private:
  friend Table;
  Table* table_;
  int x_min_;
  int x_max_;
  int y_min_;
  int y_max_;
};

}  // namespace ftxui




namespace ftxui {

/// @brief ColorInfo is a structure that contains information about the terminal
/// color palette.
///
/// @ingroup screen
struct FTXUI_EXPORT(SCREEN) ColorInfo {
  const char* name;
  uint8_t index_256;
  uint8_t index_16;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t hue;
  uint8_t saturation;
  uint8_t value;
};

FTXUI_EXPORT(SCREEN) ColorInfo GetColorInfo(Color::Palette256 index);
FTXUI_EXPORT(SCREEN) ColorInfo GetColorInfo(Color::Palette16 index);

/// @brief Get the color information for the palette256, sorted in 2D.
/// @return A 2D vector of ColorInfo.
/// @ingroup screen
FTXUI_EXPORT(SCREEN) std::vector<std::vector<ColorInfo>> ColorInfoSorted2D();

}  // namespace ftxui




namespace ftxui {
using Pixel = Cell;
}  // namespace ftxui


namespace ftxui {
using Image = Surface;
}  // namespace ftxui



namespace ftxui {

/// Assign a value to a variable, reset its old value when going out of scope.
template <typename T>
class AutoReset {
 public:
  AutoReset(T* variable, T new_value)
      : variable_(variable), previous_value_(std::move(*variable)) {
    *variable_ = std::move(new_value);
  }
  AutoReset(const AutoReset&) = delete;
  AutoReset(AutoReset&&) = delete;
  AutoReset& operator=(const AutoReset&) = delete;
  AutoReset& operator=(AutoReset&&) = delete;
  ~AutoReset() { *variable_ = std::move(previous_value_); }

 private:
  T* variable_;
  T previous_value_;
};

}  // namespace ftxui


#ifdef FTXUI_IMPLEMENTATION

// ---- component\animation.cpp -------------------------------------



// NOLINTBEGIN(*-magic-numbers)
namespace ftxui::animation {

namespace easing {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kPi2 = kPi / 2.f;
}  // namespace

// Easing function have been taken out of:
// https://github.com/warrenm/AHEasing/blob/master/AHEasing/easing.c
//
// Corresponding license:
//  Copyright (c) 2011, Auerhaus Development, LLC
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What The Fuck You Want
//  To Public License, Version 2, as published by Sam Hocevar. See
//  http://sam.zoy.org/wtfpl/COPYING for more details.

/// @brief Modeled after the line y = x
float Linear(float p) {
  return p;
}

/// @brief Modeled after the parabola y = x^2
float QuadraticIn(float p) {
  return p * p;
}

// @brief Modeled after the parabola y = -x^2 + 2x
float QuadraticOut(float p) {
  return -(p * (p - 2.f));
}

// @brief Modeled after the piecewise quadratic
// y = (1/2)((2x)^2)             ; [0, 0.5)
// y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
float QuadraticInOut(float p) {
  return p < 0.5f ? 2.f * p * p : (-2.f * p * p) + (4.f * p) - 1.f;
}

// @brief Modeled after the cubic y = x^3
float CubicIn(float p) {
  return p * p * p;
}

// @brief Modeled after the cubic y = (x - 1)^3 + 1
float CubicOut(float p) {
  const float f = (p - 1.f);
  return f * f * f + 1.f;
}

// @brief Modeled after the piecewise cubic
// y = (1/2)((2x)^3)       ; [0, 0.5)
// y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
float CubicInOut(float p) {
  if (p < 0.5f) {
    return 4.f * p * p * p;
  }
  const float f = ((2.f * p) - 2.f);
  return 0.5f * f * f * f + 1.f;
}

// @brief Modeled after the quartic x^4
float QuarticIn(float p) {
  return p * p * p * p;
}

// @brief Modeled after the quartic y = 1 - (x - 1)^4
float QuarticOut(float p) {
  const float f = (p - 1.f);
  return f * f * f * (1.f - p) + 1.f;
}

// @brief Modeled after the piecewise quartic
// y = (1/2)((2x)^4)        ; [0, 0.5)
// y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
float QuarticInOut(float p) {
  if (p < 0.5f) {
    return 8.f * p * p * p * p;
  }
  const float f = (p - 1.f);
  return -8.f * f * f * f * f + 1.f;
}

// @brief Modeled after the quintic y = x^5
float QuinticIn(float p) {
  return p * p * p * p * p;
}

// @brief Modeled after the quintic y = (x - 1)^5 + 1
float QuinticOut(float p) {
  const float f = (p - 1.f);
  return f * f * f * f * f + 1.f;
}

// @brief Modeled after the piecewise quintic
// y = (1/2)((2x)^5)       ; [0, 0.5)
// y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
float QuinticInOut(float p) {
  if (p < 0.5f) {
    return 16.f * p * p * p * p * p;
  }
  const float f = ((2.f * p) - 2.f);
  return 0.5f * f * f * f * f * f + 1.f;
}

// @brief Modeled after quarter-cycle of sine wave
float SineIn(float p) {
  return std::sin((p - 1.f) * kPi2) + 1.f;
}

// @brief Modeled after quarter-cycle of sine wave (different phase)
float SineOut(float p) {
  return std::sin(p * kPi2);
}

// @brief Modeled after half sine wave
float SineInOut(float p) {
  return 0.5f * (1.f - std::cos(p * kPi));
}

// @brief Modeled after shifted quadrant IV of unit circle
float CircularIn(float p) {
  return 1.f - std::sqrt(1.f - (p * p));
}

// @brief Modeled after shifted quadrant II of unit circle
float CircularOut(float p) {
  return std::sqrt((2.f - p) * p);
}

// @brief Modeled after the piecewise circular function
// y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
// y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
float CircularInOut(float p) {
  if (p < 0.5f) {
    return 0.5f * (1.f - std::sqrt(1.f - 4.f * (p * p)));
  }
  return 0.5f * (std::sqrt(-((2.f * p) - 3.f) * ((2.f * p) - 1.f)) + 1.f);
}

// @brief Modeled after the exponential function y = 2^(10(x - 1))
float ExponentialIn(float p) {
  return (p == 0.f) ? p : std::pow(2.f, 10.f * (p - 1.f));
}

// @brief Modeled after the exponential function y = -2^(-10x) + 1
float ExponentialOut(float p) {
  return (p == 1.f) ? p : 1.f - std::pow(2.f, -10.f * p);
}

// @brief Modeled after the piecewise exponential
// y = (1/2)2^(10(2x - 1))         ; [0,0.5)
// y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
float ExponentialInOut(float p) {
  if (p == 0.f || p == 1.f) {
    return p;
  }

  if (p < 0.5f) {
    return 0.5f * std::pow(2.f, (20.f * p) - 10.f);
  }
  return -0.5f * std::pow(2.f, (-20.f * p) + 10.f) + 1.f;
}

// @brief Modeled after the damped sine wave y = sin(13pi/2*x)*pow(2, 10 * (x -
// 1))
float ElasticIn(float p) {
  return std::sin(13.f * kPi2 * p) * std::pow(2.f, 10.f * (p - 1.f));
}

// @brief Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2,
// -10x) +
// 1
float ElasticOut(float p) {
  return std::sin(-13.f * kPi2 * (p + 1.f)) * std::pow(2.f, -10.f * p) + 1.f;
}

// @brief Modeled after the piecewise exponentially-damped sine wave:
// y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1))      ; [0,0.5)
// y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) ; [0.5, 1]
float ElasticInOut(float p) {
  if (p < 0.5f) {
    return 0.5f * std::sin(13.f * kPi2 * (2.f * p)) *
           std::pow(2.f, 10.f * ((2.f * p) - 1.f));
  }
  return 0.5f * (std::sin(-13.f * kPi2 * ((2.f * p - 1.f) + 1.f)) *
                     std::pow(2.f, -10.f * (2.f * p - 1.f)) +
                 2.f);
}

// @brief Modeled after the overshooting cubic y = x^3-x*sin(x*pi)
float BackIn(float p) {
  return p * p * p - p * std::sin(p * kPi);
}

// @brief Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
float BackOut(float p) {
  const float f = (1.f - p);
  return 1.f - (f * f * f - f * std::sin(f * kPi));
}

// @brief Modeled after the piecewise overshooting cubic function:
// y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
// y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
float BackInOut(float p) {
  if (p < 0.5f) {
    const float f = 2.f * p;
    return 0.5f * (f * f * f - f * std::sin(f * kPi));
  }
  const float f = (1.f - (2.f * p - 1.f));
  return 0.5f * (1.f - (f * f * f - f * std::sin(f * kPi))) + 0.5f;
}

float BounceIn(float p) {
  return 1.f - BounceOut(1.f - p);
}

float BounceOut(float p) {
  if (p < 4.f / 11.f) {
    return (121.f * p * p) / 16.f;
  }

  if (p < 8.f / 11.f) {
    return (363.f / 40.f * p * p) - (99.f / 10.f * p) + 17.f / 5.f;
  }

  if (p < 9.f / 10.f) {
    return (4356.f / 361.f * p * p) - (35442.f / 1805.f * p) + 16061.f / 1805.f;
  }

  return (54.f / 5.f * p * p) - (513 / 25.f * p) + 268 / 25.f;
}

float BounceInOut(float p) {
  if (p < 0.5f) {
    return 0.5f * BounceIn(p * 2.f);
  }
  return 0.5f * BounceOut(p * 2.f - 1.f) + 0.5f;
}

}  // namespace easing

Animator::Animator(float* from,
                   float to,
                   Duration duration,
                   easing::Function easing_function,
                   Duration delay)
    : value_(from),
      from_(*from),
      to_(to),
      duration_(duration),
      easing_function_(std::move(easing_function)),
      current_(-delay) {
  RequestAnimationFrame();
}

void Animator::OnAnimation(Params& params) {
  current_ += params.duration();

  if (current_ >= duration_) {
    *value_ = to_;
    return;
  }

  if (current_ <= Duration()) {
    *value_ = from_;
  } else {
    *value_ = from_ + (to_ - from_) * easing_function_(current_ / duration_);
  }

  RequestAnimationFrame();
}

}  // namespace ftxui::animation

// NOLINTEND(*-magic-numbers)

// ---- component\app.cpp -------------------------------------------



namespace ftxui {

template <typename T>
class MultiReceiverBuffer {
 public:
  class Receiver {
   public:
    explicit Receiver(MultiReceiverBuffer* buffer)
        : buffer_(buffer), index_(buffer->next_index_) {
      buffer_->receivers_.push_back(this);
    }

    Receiver(MultiReceiverBuffer* buffer, size_t index)
        : buffer_(buffer), index_(index) {
      buffer_->receivers_.push_back(this);
    }

    ~Receiver() {
      if (buffer_) {
        buffer_->RemoveReceiver(this);
      }
    }

    Receiver(const Receiver&) = delete;
    Receiver(Receiver&& other) noexcept
        : buffer_(other.buffer_), index_(other.index_) {
      other.buffer_ = nullptr;
      if (buffer_) {
        std::replace(buffer_->receivers_.begin(), buffer_->receivers_.end(),
                     &other, this);
      }
    }

    Receiver& operator=(const Receiver&) = delete;
    Receiver& operator=(Receiver&& other) noexcept {
      if (this != &other) {
        if (buffer_) {
          buffer_->RemoveReceiver(this);
        }
        buffer_ = other.buffer_;
        index_ = other.index_;
        other.buffer_ = nullptr;
        if (buffer_) {
          std::replace(buffer_->receivers_.begin(), buffer_->receivers_.end(),
                       &other, this);
        }
      }
      return *this;
    }

    bool Has() const { return buffer_ && index_ < buffer_->next_index_; }

    T Pop() {
      if (!Has()) {
        return {};
      }
      T value = buffer_->Get(index_);
      index_++;
      buffer_->Prune();
      return value;
    }

    size_t index() const { return index_; }

   private:
    friend class MultiReceiverBuffer;
    MultiReceiverBuffer* buffer_;
    size_t index_;
  };

  std::unique_ptr<Receiver> CreateReceiver() {
    return std::make_unique<Receiver>(this);
  }

  std::unique_ptr<Receiver> CreateReceiverAt(size_t index) {
    return std::make_unique<Receiver>(this, index);
  }

  void Push(T value) {
    values_.push_back(std::move(value));
    next_index_++;
  }

 private:
  void RemoveReceiver(Receiver* receiver) {
    receivers_.erase(
        std::remove(receivers_.begin(), receivers_.end(), receiver),
        receivers_.end());
    Prune();
  }

  void Prune() {
    if (receivers_.empty()) {
      values_.clear();
      start_index_ = next_index_;
      return;
    }
    size_t min_index = next_index_;
    for (auto* r : receivers_) {
      min_index = std::min(min_index, r->index_);
    }
    while (start_index_ < min_index) {
      values_.pop_front();
      start_index_++;
    }
  }

  T Get(size_t index) const { return values_[index - start_index_]; }

  std::deque<T> values_;
  std::vector<Receiver*> receivers_;
  size_t start_index_ = 0;
  size_t next_index_ = 0;
};

}  // namespace ftxui

#ifndef TASK_RUNNER_HPP
#define TASK_RUNNER_HPP

#ifndef TASK_HPP
#define TASK_HPP


namespace ftxui::task {

/// A task represents a unit of work.
using Task = std::function<void()>;

/// A PendingTask represents a task that is scheduled to be executed at a
/// specific time, or as soon as possible.
struct PendingTask {
  // Immediate task:
  PendingTask(Task t) : task(std::move(t)) {}  // NOLINT

  // Delayed task with a duration
  PendingTask(Task t, std::chrono::steady_clock::duration duration)
      : task(std::move(t)), time(std::chrono::steady_clock::now() + duration) {}

  /// The task to be executed.
  Task task;

  /// The time when the task should be executed. If the time is empty, the task
  /// should be executed as soon as possible.
  std::optional<std::chrono::steady_clock::time_point> time;

  /// Compare two PendingTasks by their time.
  /// If both tasks have no time, they are considered equal.
  bool operator<(const PendingTask& other) const;
};

}  // namespace ftxui::task

#endif  // TASK_HPP_
#ifndef TASK_QUEUE_HPP
#define TASK_QUEUE_HPP



namespace ftxui::task {

/// A task queue that schedules tasks to be executed in the future. Tasks can be
/// scheduled to be executed immediately, or after a certain duration.
/// - The tasks are executed in the order they were scheduled.
/// - If multiple tasks are scheduled to be executed at the same time, they are
///   executed in the order they were scheduled.
/// - If a task is scheduled to be executed in the past, it is executed
///   immediately.
struct TaskQueue {
  using MaybeTask =
      std::variant<Task, std::chrono::steady_clock::duration, std::monostate>;

  auto Get() -> MaybeTask;
  auto HasImmediateTasks() const -> bool;
  auto PostTask(PendingTask task) -> void;

 private:
  mutable std::mutex mutex_;
  std::queue<PendingTask> immediate_tasks_;
  std::priority_queue<PendingTask> delayed_tasks_;
};

}  // namespace ftxui::task

#endif

namespace ftxui::task {

class TaskRunner {
 public:
  TaskRunner();
  ~TaskRunner();

  // Returns the task runner for the current thread.
  static auto Current() -> TaskRunner*;

  /// Schedules a task to be executed immediately.
  auto PostTask(Task task) -> void;

  /// Schedules a task to be executed after a certain duration.
  auto PostDelayedTask(Task task, std::chrono::steady_clock::duration duration)
      -> void;

  /// Runs the tasks in the queue, return the delay until the next delayed task
  /// can be executed.
  auto RunUntilIdle() -> std::optional<std::chrono::steady_clock::duration>;

  // Runs the tasks in the queue, blocking until all tasks are executed.
  auto Run() -> void;

  bool HasImmediateTasks() const { return queue_.HasImmediateTasks(); }

  size_t ExecutedTasks() const { return executed_tasks_; }

 private:
  TaskRunner* previous_task_runner_ = nullptr;
  TaskQueue queue_;
  size_t executed_tasks_ = 0;
};

}  // namespace ftxui::task

#endif  // TASK_RUNNER_HPP



namespace ftxui {
struct Event;

// Parse a sequence of |char| across |time|. Produces |Event|.
class TerminalInputParser {
 public:
  explicit TerminalInputParser(std::function<void(Event)> out);
  void Timeout(int time);
  void Add(char c);

 private:
  unsigned char Current();
  bool Eat();

  enum Type {
    UNCOMPLETED,
    DROP,
    RESYNC,
    CHARACTER,
    MOUSE,
    CURSOR_POSITION,
    CURSOR_SHAPE,
    TERMINAL_NAME_VERSION,
    TERMINAL_EMULATOR,
    TERMINAL_CAPABILITIES,
    SPECIAL,
  };

  struct CursorPosition {
    int x;
    int y;
  };

  struct TerminalNameVersion {
    std::string name;
    int version;
  };

  struct TerminalEmulator {
    std::string name;
    std::string version;
  };

  struct TerminalCapabilities {
    std::vector<int> capabilities;
  };

  struct Output {
    Type type;
    union {
      Mouse mouse;
      CursorPosition cursor{};
      int cursor_shape;
      int terminal_version;
    };
    std::string terminal_name;
    std::string terminal_version_string;
    std::vector<int> terminal_capabilities;

    Output(Type t)  // NOLINT
        : type(t) {}
  };

  void Send(Output output);
  Output Parse();
  Output ParseUTF8();
  Output ParseESC();
  Output ParseDCS();
  Output ParseCSI();
  Output ParseOSC();
  Output ParseMouse(bool altered, bool pressed, std::vector<int> arguments);
  Output ParseCursorPosition(std::vector<int> arguments);
  Output ParseDeviceAttributes(bool altered_greater,
                               bool altered_question,
                               std::vector<int> arguments);

  std::function<void(Event)> out_;
  int position_ = -1;
  int timeout_ = 0;
  std::string pending_;
};

}  // namespace ftxui



namespace ftxui::util {

// Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

// Returns the value of the environment variable |name|, or "" if unset.
inline const char* GetEnv(const char* name) {
  // The Microsoft CRT marks std::getenv as deprecated.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
  const char* value = std::getenv(name);  // NOLINT
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
  return value ? value : "";
}

}  // namespace ftxui::util


#if defined(_WIN32)
#define DEFINE_CONSOLEV2_PROPERTIES
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#else
#endif

#if defined(__EMSCRIPTEN__)
#endif

namespace ftxui {

enum class AppDimension {
  FitComponent,
  Fixed,
  Fullscreen,
  TerminalOutput,
};

namespace animation {
void RequestAnimationFrame() {
  auto* screen = App::Active();
  if (screen) {
    screen->RequestAnimationFrame();
  }
}
}  // namespace animation

#if defined(__EMSCRIPTEN__)
extern "C" {
EMSCRIPTEN_KEEPALIVE
void ftxui_on_resize(int columns, int rows) {
  Terminal::SetFallbackSize({
      columns,
      rows,
  });
  std::raise(SIGWINCH);
}
}
#endif

struct App::Internal {
  // The frame currently displayed by the terminal. Its cells are swapped with
  // the App's ones, instead of being copied.
  class PreviousFrame : public Screen {
   public:
    PreviousFrame() : Screen(0, 0) {}

    void Swap(std::vector<Cell>& cells,
              std::vector<std::string>& hyperlinks,
              int dimx,
              int dimy) {
      std::swap(cells_, cells);
      std::swap(hyperlinks_, hyperlinks);
      dimx_ = dimx;
      dimy_ = dimy;
    }
  };

  App* public_;

  App* suspended_screen_ = nullptr;
  const AppDimension dimension_;
  const bool use_alternative_screen_;

  bool track_mouse_ = true;

  std::string set_cursor_position_;
  std::string reset_cursor_position_;

  std::atomic<bool> quit_{false};
  bool installed_ = false;
  bool animation_requested_ = false;
  animation::TimePoint previous_animation_time_;

  int cursor_x_ = 1;
  int cursor_y_ = 1;

  std::uint64_t frame_count_ = 0;
  bool mouse_captured = false;
  bool previous_frame_resized_ = false;

  bool frame_valid_ = false;

  // The frame currently displayed by the terminal. Only the cells that changed
  // since are printed. Empty when the terminal must be fully redrawn.
  PreviousFrame previous_frame_;

  // Elements to print above the app on the next frame.
  std::vector<Element> print_above_;

  bool force_handle_ctrl_c_ = true;
  bool force_handle_ctrl_z_ = true;

  // 0: the terminal's default shape, used when it doesn't report its own.
  int cursor_reset_shape_ = 0;

  // Piped input handling state (POSIX only)
  bool handle_piped_input_ = true;
  bool is_stdin_a_tty_ = false;
  bool is_stdout_a_tty_ = false;
  // File descriptor for /dev/tty, used for piped input handling.
  int tty_fd_ = -1;

  std::string terminal_name_ = "unknown";
  int terminal_version_ = 0;

  std::string terminal_emulator_name_ = "unknown";
  std::string terminal_emulator_version_ = "unknown";

  std::vector<int> terminal_capabilities_;

  // Selection API:
  CapturedMouse selection_pending_;
  struct SelectionData {
    int start_x = -1;
    int start_y = -1;
    int end_x = -2;
    int end_y = -2;
    bool empty = true;
    bool operator==(const SelectionData& other) const {
      if (empty && other.empty) {
        return true;
      }
      if (empty || other.empty) {
        return false;
      }
      return start_x == other.start_x && start_y == other.start_y &&
             end_x == other.end_x && end_y == other.end_y;
    }
    bool operator!=(const SelectionData& other) const {
      return !(*this == other);
    }
  };
  SelectionData selection_data_;
  SelectionData selection_data_previous_;
  std::unique_ptr<Selection> selection_;
  std::function<void()> selection_on_change_;

  Component component_;

  // Pre-existing in Internal:
  TerminalInputParser terminal_input_parser;
  task::TaskRunner task_runner;
  std::chrono::time_point<std::chrono::steady_clock> last_char_time =
      std::chrono::steady_clock::now();
  std::string output_buffer;

  class ThrottledRequest {
   public:
    ThrottledRequest(App::Internal* internal, std::function<void()> send)
        : internal_(internal), send_(std::move(send)) {}

    void Request(bool force = false) {
      if (!internal_->is_stdin_a_tty_) {
        return;
      }

      if (force) {
        Send();
        return;
      }

      // Allow only one pending request at a time. This is to avoid flooding the
      // terminal with requests.
      if (HasPending()) {
        return;
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - last_request_time_ < std::chrono::milliseconds(500)) {
        // Too soon since the last request. Skip it: the request must be sent
        // synchronously from Draw(), right after the cursor is moved to the
        // frame's origin, so that the terminal's reply reflects that
        // position. Draw() calls Request() again on the next frame, so the
        // request isn't lost, only delayed.
        return;
      }

      Send();
    }

    void OnReply() { pending_request_ = false; }

    bool HasPending() const {
      if (!pending_request_) {
        return false;
      }
      const auto now = std::chrono::steady_clock::now();
      return now - last_sent_time_ < std::chrono::seconds(5);
    }

   private:
    void Send() {
      last_sent_time_ = std::chrono::steady_clock::now();
      last_request_time_ = last_sent_time_;
      pending_request_ = true;
      send_();
    }

    App::Internal* internal_;
    std::function<void()> send_;
    bool pending_request_ = false;
    std::chrono::steady_clock::time_point last_request_time_ =
        std::chrono::steady_clock::now() - std::chrono::hours(1);
    std::chrono::steady_clock::time_point last_sent_time_ =
        std::chrono::steady_clock::now() - std::chrono::hours(1);
  };

  ThrottledRequest cursor_position_request;

  MultiReceiverBuffer<Event> event_buffer;
  std::unique_ptr<MultiReceiverBuffer<Event>::Receiver> main_loop_receiver;

  Internal(App* app, AppDimension dimension, bool use_alternative_screen);

  void ExitNow();
  void Install();
  void Uninstall();
  void PreMain();
  void PostMain();
  bool HasQuitted();
  void RunOnce(const Component& component);
  void RunOnceBlocking(Component component);
  void HandleTask(Component component, Task& task);
  bool HandleSelection(bool handled, Event event);
  void Draw(Component component);
  std::string ResetCursorPosition();
  void RequestCursorPosition(bool force = false);
  void TerminalSend(std::string_view);
  void TerminalFlush();
  void InstallPipedInputHandling();
  void InstallTerminalInfo();
  void Signal(int signal);
  size_t FetchTerminalEvents();
  void PostAnimationTask();
};

namespace {

App* g_active_screen = nullptr;  // NOLINT

std::stack<Closure> on_exit_functions;  // NOLINT

void OnExit() {
  while (!on_exit_functions.empty()) {
    on_exit_functions.top()();
    on_exit_functions.pop();
  }
}

// CSI: Control Sequence Introducer
const std::string CSI = "\x1b[";  // NOLINT
                                  //
// DCS: Device Control String
const std::string DCS = "\x1bP";  // NOLINT

// ST: String Terminator
const std::string ST = "\x1b\\";  // NOLINT

// DECRQSS: Request Status String
// DECSCUSR: Set Cursor Style
const std::string DECRQSS_DECSCUSR = DCS + "$q q" + ST;  // NOLINT

// Some terminals don't parse DCS sequences, and print their payload ("$q q")
// instead. See https://github.com/ArthurSonzogni/FTXUI/issues/1217
bool SupportsDECRQSS() {
  // Apple's Terminal.app.
  if (std::string_view(util::GetEnv("TERM_PROGRAM")) == "Apple_Terminal") {
    return false;
  }
  // GNU Screen.
  if (util::GetEnv("STY")[0] != '\0') {
    return false;
  }
  return true;
}

// DEC: Digital Equipment Corporation
enum class DECMode : std::uint16_t {
  kLineWrap = 7,
  kCursor = 25,

  kMouseX10 = 9,
  kMouseVt200 = 1000,
  kMouseVt200Highlight = 1001,

  kMouseBtnEventMouse = 1002,
  kMouseAnyEvent = 1003,

  kMouseUtf8 = 1005,
  kMouseSgrExtMode = 1006,
  kMouseUrxvtMode = 1015,
  kMouseSgrPixelsMode = 1016,
  kAlternateScreen = 1049,
  kSynchronizedOutput = 2026,
};

// Device Status Report (DSR) {
enum class DSRMode : std::uint8_t {
  kCursor = 6,
};

std::string Serialize(const std::vector<DECMode>& parameters) {
  bool first = true;
  std::string out;
  for (const DECMode parameter : parameters) {
    if (!first) {
      out += ";";
    }
    out += std::to_string(int(parameter));
    first = false;
  }
  return out;
}

// DEC Private Mode Set (DECSET)
std::string Set(const std::vector<DECMode>& parameters) {
  return CSI + "?" + Serialize(parameters) + "h";
}

// DEC Private Mode Reset (DECRST)
std::string Reset(const std::vector<DECMode>& parameters) {
  return CSI + "?" + Serialize(parameters) + "l";
}

// Device Status Report (DSR)
std::string DeviceStatusReport(DSRMode ps) {
  return CSI + std::to_string(int(ps)) + "n";
}

class CapturedMouseImpl : public CapturedMouseInterface {
 public:
  explicit CapturedMouseImpl(std::function<void(void)> callback)
      : callback_(std::move(callback)) {}
  ~CapturedMouseImpl() override { callback_(); }
  CapturedMouseImpl(const CapturedMouseImpl&) = delete;
  CapturedMouseImpl(CapturedMouseImpl&&) = delete;
  CapturedMouseImpl& operator=(const CapturedMouseImpl&) = delete;
  CapturedMouseImpl& operator=(CapturedMouseImpl&&) = delete;

 private:
  std::function<void(void)> callback_;
};

#if !defined(_WIN32)
std::atomic<int> g_signal_exit_count = 0;    // NOLINT
std::atomic<int> g_signal_stop_count = 0;    // NOLINT
std::atomic<int> g_signal_resize_count = 0;  // NOLINT
#else
std::atomic<int> g_signal_exit_count = 0;  // NOLINT
#endif

// Tracks whether the terminal is currently configured in raw mode.
// Used to prevent double-restoration in emergency and normal exits.
std::atomic<bool> g_terminal_is_raw{false};

// Stores the last received deferred signal (e.g. SIGINT, SIGTERM) to be
// re-raised during uninstallation/exit.
std::atomic<int> g_last_signal{0};  // NOLINT

#if defined(_WIN32)
using SignalHandler = void (*)(int);
// Stores the original signal handlers before FTXUI installed its own.
std::map<int, SignalHandler> g_old_signal_handlers;

// Stores the original console modes to restore them during exit.
DWORD g_original_stdout_mode = 0;
DWORD g_original_stdin_mode = 0;
bool g_has_original_console_mode = false;
#else
// Stores the original sigaction structures before FTXUI installed its own.
std::map<int, struct sigaction> g_old_sigactions;

// Stores the original termios terminal settings to restore them during exit.
struct termios g_original_termios;
bool g_has_original_termios = false;
int g_tty_fd = -1;
#endif

// Restores the original signal handler for the given signal and re-raises it.
// Async-signal-safe function.
void RestoreSignalHandlerAndRaise(int signal) {
#if defined(_WIN32)
  auto it = g_old_signal_handlers.find(signal);
  auto old_handler = (it != g_old_signal_handlers.end()) ? it->second : SIG_DFL;
  std::signal(signal, old_handler);
#else
  auto it = g_old_sigactions.find(signal);
  if (it != g_old_sigactions.end()) {
    sigaction(signal, &it->second, nullptr);
  } else {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
  }
#endif
  std::raise(signal);
}

// Emergency terminal state restoration.
// Async-signal-safe function.
void RestoreTerminalEmergency() {
  if (!g_terminal_is_raw.exchange(false)) {
    return;
  }
#if defined(_WIN32)
  if (g_has_original_console_mode) {
    auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    auto stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(stdout_handle, g_original_stdout_mode);
    SetConsoleMode(stdin_handle, g_original_stdin_mode);
  }
#else
  if (g_has_original_termios && g_tty_fd >= 0) {
    const char restore_seq[] =
        "\x1b[?25h"    // Show cursor.
        "\x1b[?1049l"  // Switch to normal screen buffer.
        "\x1b[?1000l"  // Disable normal mouse tracking.
        "\x1b[?1002l"  // Disable button event mouse tracking.
        "\x1b[?1003l"  // Disable all motion mouse tracking.
        "\x1b[?1006l"  // Disable SGR mouse tracking.
        "\x1b[?1015l"  // Disable Urxvt mouse tracking.
        "\x1b[?7h";    // Enable line wrapping.
    std::ignore = write(STDOUT_FILENO, restore_seq, sizeof(restore_seq) - 1);
    tcsetattr(g_tty_fd, TCSANOW, &g_original_termios);
  }
#endif
}

// Async signal safe function
void RecordSignal(int signal) {
  switch (signal) {
    // Abnormal termination (e.g. abort() or assertion failure).
    case SIGABRT:
    // Erroneous arithmetic operation (e.g. division by zero).
    case SIGFPE:
    // Illegal instruction.
    case SIGILL:
    // Invalid memory reference (segmentation fault).
    case SIGSEGV:
#if !defined(_WIN32)
    // Bus error (e.g. bad memory access alignment).
    case SIGBUS:
    // Bad system call.
    case SIGSYS:
#endif
    {
      RestoreTerminalEmergency();
      RestoreSignalHandlerAndRaise(signal);
      break;
    }

    // Terminal interrupt (e.g. Ctrl-C).
    case SIGINT:
    // Termination request.
    case SIGTERM:
#if !defined(_WIN32)
    // Terminal quit (e.g. Ctrl-\, produces core dump).
    case SIGQUIT:
    // Hangup detected on controlling terminal or death of controlling process.
    case SIGHUP:
#endif
      g_last_signal.store(signal);
      g_signal_exit_count++;
      break;

#if !defined(_WIN32)
    // Terminal stop signal (e.g. Ctrl-Z).
    case SIGTSTP:  // NOLINT
      g_signal_stop_count++;
      break;

    // Terminal window size change.
    case SIGWINCH:  // NOLINT
      g_signal_resize_count++;
      break;
#endif

    default:
      break;
  }
}

void ExecuteSignalHandlers() {
  if (g_last_signal.load() != 0) {
    App::Private::Signal(*g_active_screen, SIGABRT);
  }

  int signal_exit_count = g_signal_exit_count.exchange(0);
  while (signal_exit_count--) {
    App::Private::Signal(*g_active_screen, SIGABRT);
  }

#if !defined(_WIN32)
  int signal_stop_count = g_signal_stop_count.exchange(0);
  while (signal_stop_count--) {
    App::Private::Signal(*g_active_screen, SIGTSTP);
  }

  int signal_resize_count = g_signal_resize_count.exchange(0);
  while (signal_resize_count--) {
    App::Private::Signal(*g_active_screen, SIGWINCH);
  }
#endif
}

void InstallSignalHandler(int sig) {
#if defined(_WIN32)
  auto old_signal_handler = std::signal(sig, RecordSignal);
  g_old_signal_handlers[sig] = old_signal_handler;
  on_exit_functions.emplace(
      [=] { std::ignore = std::signal(sig, old_signal_handler); });
#else
  struct sigaction sa;
  sa.sa_handler = RecordSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  struct sigaction old_sa;
  sigaction(sig, &sa, &old_sa);
  g_old_sigactions[sig] = old_sa;
  on_exit_functions.emplace([=] { sigaction(sig, &old_sa, nullptr); });
#endif
}

}  // namespace

App::Internal::Internal(App* app,
                        AppDimension dimension,
                        bool use_alternative_screen)
    : public_(app),
      dimension_(dimension),
      use_alternative_screen_(use_alternative_screen),
      terminal_input_parser([&](Event event) {
        event_buffer.Push(std::move(event));
      }),
      cursor_position_request(this, [this] {
        TerminalSend(DeviceStatusReport(DSRMode::kCursor));
      }) {
  main_loop_receiver = event_buffer.CreateReceiver();
}

void App::Internal::ExitNow() {
  quit_ = true;
}

void App::Internal::Install() {
  frame_valid_ = false;
  previous_frame_ = PreviousFrame();

  // Flush the buffer for stdout to ensure whatever the user has printed before
  // is fully applied before we start modifying the terminal configuration. This
  // is important, because we are using two different channels (stdout vs
  // termios/WinAPI) to communicate with the terminal emulator below. See
  // https://github.com/ArthurSonzogni/FTXUI/issues/846
  TerminalFlush();

  InstallPipedInputHandling();

  // After uninstalling the new configuration, flush it to the terminal to
  // ensure it is fully applied:
  on_exit_functions.emplace([this] { TerminalFlush(); });

  // Install signal handlers to restore the terminal state on exit. The default
  // signal handlers are restored on exit.
  for (const int signal : {SIGTERM, SIGSEGV, SIGINT, SIGILL, SIGABRT, SIGFPE}) {
    InstallSignalHandler(signal);
  }

// Save the old terminal configuration and restore it on exit.
#if defined(_WIN32)
  // Enable VT processing on stdout and stdin
  auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  auto stdin_handle = GetStdHandle(STD_INPUT_HANDLE);

  DWORD out_mode = 0;
  DWORD in_mode = 0;
  GetConsoleMode(stdout_handle, &out_mode);
  GetConsoleMode(stdin_handle, &in_mode);
  g_original_stdout_mode = out_mode;
  g_original_stdin_mode = in_mode;
  g_has_original_console_mode = true;
  on_exit_functions.push([=] { SetConsoleMode(stdout_handle, out_mode); });
  on_exit_functions.push([=] { SetConsoleMode(stdin_handle, in_mode); });

  // https://docs.microsoft.com/en-us/windows/console/setconsolemode
  const int enable_virtual_terminal_processing = 0x0004;
  const int disable_newline_auto_return = 0x0008;
  out_mode |= enable_virtual_terminal_processing;
  out_mode |= disable_newline_auto_return;

  // https://docs.microsoft.com/en-us/windows/console/setconsolemode
  const int enable_line_input = 0x0002;
  const int enable_echo_input = 0x0004;
  const int enable_virtual_terminal_input = 0x0200;
  const int enable_window_input = 0x0008;
  in_mode &= ~enable_echo_input;
  in_mode &= ~enable_line_input;
  in_mode |= enable_virtual_terminal_input;
  in_mode |= enable_window_input;

  SetConsoleMode(stdin_handle, in_mode);
  SetConsoleMode(stdout_handle, out_mode);
#else  // POSIX (Linux & Mac)
  for (const int signal :
       {SIGWINCH, SIGTSTP, SIGBUS, SIGSYS, SIGQUIT, SIGHUP}) {
    InstallSignalHandler(signal);
  }

  struct termios terminal;  // NOLINT
  tcgetattr(tty_fd_, &terminal);
  g_original_termios = terminal;
  g_tty_fd = tty_fd_;
  g_has_original_termios = true;
  on_exit_functions.emplace([terminal = terminal, tty_fd_ = tty_fd_] {
    tcsetattr(tty_fd_, TCSANOW, &terminal);
  });

  // Enabling raw terminal input mode
  terminal.c_iflag &= ~IGNBRK;  // Disable ignoring break condition
  terminal.c_iflag &= ~BRKINT;  // Disable break causing input and output to be
                                // flushed
  terminal.c_iflag &= ~PARMRK;  // Disable marking parity errors.
  terminal.c_iflag &= ~ISTRIP;  // Disable stripping 8th bit off characters.
  terminal.c_iflag &= ~INLCR;   // Disable mapping NL to CR.
  terminal.c_iflag &= ~IGNCR;   // Disable ignoring CR.
  terminal.c_iflag &= ~ICRNL;   // Disable mapping CR to NL.
  terminal.c_iflag &= ~IXON;    // Disable XON/XOFF flow control on output

  terminal.c_lflag &= ~ECHO;    // Disable echoing input characters.
  terminal.c_lflag &= ~ECHONL;  // Disable echoing new line characters.
  terminal.c_lflag &= ~ICANON;  // Disable Canonical mode.
  terminal.c_lflag &= ~ISIG;    // Disable sending signal when hitting:
                                // -     => DSUSP
                                // - C-Z => SUSP
                                // - C-C => INTR
                                // - C-d => QUIT
  terminal.c_lflag &= ~IEXTEN;  // Disable extended input processing
  terminal.c_cflag |= CS8;      // 8 bits per byte

  terminal.c_cc[VMIN] = 0;   // Minimum number of characters for non-canonical
                             // read.
  terminal.c_cc[VTIME] = 0;  // Timeout in deciseconds for non-canonical read.

  tcsetattr(tty_fd_, TCSANOW, &terminal);

#endif

  auto enable = [&](const std::vector<DECMode>& parameters) {
    TerminalSend(Set(parameters));
    on_exit_functions.emplace(
        [this, parameters] { TerminalSend(Reset(parameters)); });
  };

  auto disable = [&](const std::vector<DECMode>& parameters) {
    TerminalSend(Reset(parameters));
    on_exit_functions.emplace(
        [this, parameters] { TerminalSend(Set(parameters)); });
  };

  if (use_alternative_screen_) {
    enable({
        DECMode::kAlternateScreen,
    });
  }

  disable({
      DECMode::kLineWrap,
  });

  if (track_mouse_) {
    enable({DECMode::kMouseVt200});
    enable({DECMode::kMouseAnyEvent});
    enable({DECMode::kMouseUrxvtMode});
    enable({DECMode::kMouseSgrExtMode});
  }

  // After installing the new configuration, flush it to the terminal to
  // ensure it is fully applied:
  TerminalFlush();

  InstallTerminalInfo();

  quit_ = false;

  PostAnimationTask();

  installed_ = true;
  g_terminal_is_raw = true;
}

void App::Internal::Uninstall() {
  g_terminal_is_raw = false;
  installed_ = false;

  // During shutdown, wait for all of the replies.
  if (is_stdin_a_tty_ && is_stdout_a_tty_) {
    auto closing_receiver =
        event_buffer.CreateReceiverAt(main_loop_receiver->index());
    auto start = std::chrono::steady_clock::now();
    while (cursor_position_request.HasPending()) {
      FetchTerminalEvents();

      while (closing_receiver->Has()) {
        const auto event = closing_receiver->Pop();
        if (event.is_cursor_position()) {
          cursor_x_ = event.cursor_x();
          cursor_y_ = event.cursor_y();
          cursor_position_request.OnReply();
        }
      }

      task_runner.RunUntilIdle();

      if (std::chrono::steady_clock::now() - start >
          std::chrono::milliseconds(400)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  OnExit();
}

void App::Internal::PreMain() {
  // Suspend previously active screen:
  if (g_active_screen) {
    std::swap(suspended_screen_, g_active_screen);
    // Reset cursor position to the top of the screen and clear the screen.
    // Nothing to clear when nothing was drawn yet.
    if (suspended_screen_->internal_->frame_count_ != 0) {
      suspended_screen_->internal_->TerminalSend(
          suspended_screen_->internal_->ResetCursorPosition());
      suspended_screen_->ResetPosition(
          suspended_screen_->internal_->output_buffer,
          /*clear=*/true);
      suspended_screen_->dimx_ = 0;
      suspended_screen_->dimy_ = 0;
    }

    // Reset dimensions to force drawing the screen again next time:
    suspended_screen_->internal_->Uninstall();
  }

  // The first frame is drawn at the cursor position, below what was printed
  // before, including the last frame of a previous Loop. See #809.
  frame_count_ = 0;

  // This screen is now active:
  g_active_screen = public_;
  g_active_screen->internal_->Install();

  previous_animation_time_ = animation::Clock::now();
}

void App::Internal::PostMain() {
  // Put cursor position at the end of the drawing.
  TerminalSend(ResetCursorPosition());

  g_active_screen = nullptr;

  // Restore suspended screen.
  if (suspended_screen_) {
    // Clear screen, and put the cursor at the beginning of the drawing.
    if (frame_count_ != 0) {
      public_->ResetPosition(output_buffer, /*clear=*/true);
      public_->dimx_ = 0;
      public_->dimy_ = 0;
    }
    Uninstall();
    std::swap(g_active_screen, suspended_screen_);
    g_active_screen->internal_->Install();
  } else {
    Uninstall();

    std::cout << "\r";
    // On final exit, keep the current drawing and reset cursor position one
    // line after it.
    if (!use_alternative_screen_) {
      std::cout << "\n";
    }
    std::cout << std::flush;
  }

  int sig = g_last_signal.exchange(0);
  if (sig != 0) {
    RestoreSignalHandlerAndRaise(sig);
  }
}

bool App::Internal::HasQuitted() {
  return quit_;
}

void App::Internal::RunOnce(const Component& component) {
  const AutoReset set_component(&component_, component);
  ExecuteSignalHandlers();
  FetchTerminalEvents();

  while (!quit_ && main_loop_receiver->Has()) {
    public_->Post(main_loop_receiver->Pop());
  }

  // Execute the pending tasks from the queue.
  const size_t executed_task = task_runner.ExecutedTasks();
  task_runner.RunUntilIdle();
  // If no executed task, we can return early without redrawing the screen.
  if (executed_task == task_runner.ExecutedTasks()) {
    return;
  }

  ExecuteSignalHandlers();
  Draw(component);

  if (selection_data_previous_ != selection_data_) {
    selection_data_previous_ = selection_data_;
    if (selection_on_change_) {
      selection_on_change_();
      public_->Post(Event::Custom);
    }
  }
}

void App::Internal::RunOnceBlocking(Component component) {
  // Set FPS to 60 at most.
  const auto time_per_frame = std::chrono::microseconds(16666);  // 1s / 60fps

  auto time = std::chrono::steady_clock::now();
  const size_t executed_task = task_runner.ExecutedTasks();

  // Wait for at least one task to execute.
  while (executed_task == task_runner.ExecutedTasks() && !HasQuitted()) {
    RunOnce(component);

    const auto now = std::chrono::steady_clock::now();
    const auto delta = now - time;
    time = now;

    if (delta < time_per_frame) {
      const auto sleep_duration = time_per_frame - delta;
      std::this_thread::sleep_for(sleep_duration);
    }
  }
}

void App::Internal::HandleTask(Component component, Task& task) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        // clang-format off

    // Handle Event.
    if constexpr (std::is_same_v<T, Event>) {

      if (arg.is_cursor_position()) {
        cursor_x_ = arg.cursor_x();
        cursor_y_ = arg.cursor_y();
        cursor_position_request.OnReply();
        return;
      }

      if (arg.is_cursor_shape()) {
        cursor_reset_shape_ = arg.cursor_shape();
        return;
      }

      if (arg.IsTerminalCapabilities()) {
        terminal_capabilities_ = arg.TerminalCapabilities();
        return;
      }

      if (arg.IsTerminalNameVersion()) {
        terminal_name_ = arg.TerminalName();
        terminal_version_ = arg.TerminalVersion();
        return;
      }

      if (arg.IsTerminalEmulator()) {
        terminal_emulator_name_ = arg.TerminalEmulatorName();
        terminal_emulator_version_ = arg.TerminalEmulatorVersion();
        return;
      }

      if (arg.is_mouse()) {
        arg.mouse().x -= cursor_x_;
        arg.mouse().y -= cursor_y_;
      }

      arg.screen_ = public_;

      bool handled = component->OnEvent(arg);
      handled = HandleSelection(handled, arg);

      if (arg == Event::CtrlC && (!handled || force_handle_ctrl_c_)) {
        RecordSignal(SIGINT);
      }

#if !defined(_WIN32)
      if (arg == Event::CtrlZ && (!handled || force_handle_ctrl_z_)) {
        RecordSignal(SIGTSTP);
      }
#endif
      
      frame_valid_ = false;
      return;
    }

    // Handle callback
    if constexpr (std::is_same_v<T, Closure>) {
      arg();
      return;
    }

    // Handle Animation
    if constexpr (std::is_same_v<T, AnimationTask>) {
      if (!animation_requested_) {
        return;
      }

      animation_requested_ = false;
      const animation::TimePoint now = animation::Clock::now();
      const animation::Duration delta = now - previous_animation_time_;
      previous_animation_time_ = now;

      animation::Params params(delta);
      component->OnAnimation(params);
      frame_valid_ = false;
      return;
    }
  },
  task);
  // clang-format on
}

bool App::Internal::HandleSelection(bool handled, Event event) {
  if (handled) {
    selection_pending_ = nullptr;
    selection_data_.empty = true;
    selection_ = nullptr;
    return true;
  }

  if (!event.is_mouse()) {
    return false;
  }

  auto& mouse = event.mouse();
  if (mouse.button != Mouse::Left) {
    return false;
  }

  if (mouse.motion == Mouse::Pressed) {
    selection_pending_ = public_->CaptureMouse();
    selection_data_.start_x = mouse.x;
    selection_data_.start_y = mouse.y;
    selection_data_.end_x = mouse.x;
    selection_data_.end_y = mouse.y;
    return false;
  }

  if (!selection_pending_) {
    return false;
  }

  if (mouse.motion == Mouse::Moved) {
    if ((mouse.x != selection_data_.end_x) ||
        (mouse.y != selection_data_.end_y)) {
      selection_data_.end_x = mouse.x;
      selection_data_.end_y = mouse.y;
      selection_data_.empty = false;
    }

    return true;
  }

  if (mouse.motion == Mouse::Released) {
    selection_pending_ = nullptr;
    selection_data_.end_x = mouse.x;
    selection_data_.end_y = mouse.y;
    selection_data_.empty = false;
    return true;
  }

  return false;
}

void App::Internal::Draw(Component component) {
  if (frame_valid_) {
    return;
  }
  auto document = component->Render();
  int dimx = 0;
  int dimy = 0;
  auto terminal = Terminal::Size();
  document->ComputeRequirement();
  switch (dimension_) {
    case AppDimension::Fixed:
      dimx = public_->dimx_;
      dimy = public_->dimy_;
      break;
    case AppDimension::TerminalOutput:
      dimx = terminal.dimx;
      dimy = util::clamp(document->requirement().min_y, 0, terminal.dimy);
      break;
    case AppDimension::Fullscreen:
      dimx = terminal.dimx;
      dimy = terminal.dimy;
      break;
    case AppDimension::FitComponent:
      dimx = util::clamp(document->requirement().min_x, 0, terminal.dimx);
      dimy = util::clamp(document->requirement().min_y, 0, terminal.dimy);
      break;
  }

  // Ask the terminal to present the frame atomically, instead of showing it
  // while it is being drawn. Terminals not supporting it ignore this.
  TerminalSend(Set({DECMode::kSynchronizedOutput}));

  // Hide cursor to prevent flickering during reset.
  TerminalSend("\033[?25l");

  const bool resized =
      frame_count_ == 0 || (dimx != public_->dimx_) || (dimy != public_->dimy_);
  // Whether the frame can be updated in place, from the previous one.
  const bool update_in_place = !resized && print_above_.empty();
  TerminalSend(ResetCursorPosition());

  if (frame_count_ != 0) {
    // Reset the cursor position to the lower left corner to start drawing the
    // new frame.
    // Clear the previous frame when printing above it, because the new frame
    // is drawn lower and would not overwrite it fully.
    public_->ResetPosition(output_buffer, resized || !print_above_.empty());

    // If the terminal width decrease, the terminal emulator will start wrapping
    // lines and make the display dirty. We should clear it completely.
    if ((dimx < public_->dimx_) && !use_alternative_screen_) {
      TerminalSend("\033[J");  // clear terminal output
      TerminalSend("\033[H");  // move cursor to home position
    }
  }

  // Print the pending elements above the frame. They are never drawn again,
  // and scroll into the terminal scrollback.
  for (auto& element : print_above_) {
    const int height =
        Dimension::Fit(element, /*extend_beyond_screen=*/true).dimy;
    Screen screen(terminal.dimx, height);
    Render(screen, element.get());
    screen.ToString(output_buffer);
    TerminalSend("\r\n");
  }
  print_above_.clear();

  // Resize the screen if needed.
  if (resized) {
    public_->dimx_ = dimx;
    public_->dimy_ = dimy;
    public_->cells_ = std::vector<Cell>(static_cast<size_t>(dimx) *
                                        static_cast<size_t>(dimy));
    Cursor cursor = public_->cursor_;
    cursor.x = dimx - 1;
    cursor.y = dimy - 1;
    public_->SetCursor(cursor);
  }

  // Periodically request the terminal emulator the frame position relative to
  // the screen. This is useful for converting mouse position reported in
  // screen's coordinates to frame's coordinates.
  if (!use_alternative_screen_ && is_stdout_a_tty_) {
    RequestCursorPosition(previous_frame_resized_);
  }
  previous_frame_resized_ = resized;

  selection_ = selection_data_.empty
                   ? std::make_unique<Selection>()
                   : std::make_unique<Selection>(
                         selection_data_.start_x, selection_data_.start_y,  //
                         selection_data_.end_x, selection_data_.end_y);
  Render(*public_, document.get(), *selection_);

  // Set cursor position for user using tools to insert CJK characters.
  {
    const int dx = public_->dimx_ - 1 - public_->cursor_.x +
                   int(public_->dimx_ != terminal.dimx);
    const int dy = public_->dimy_ - 1 - public_->cursor_.y;

    set_cursor_position_.clear();
    reset_cursor_position_.clear();

    if (dy != 0) {
      set_cursor_position_ += "\x1B[" + std::to_string(dy) + "A";
      reset_cursor_position_ += "\x1B[" + std::to_string(dy) + "B";
    }

    if (dx != 0) {
      set_cursor_position_ += "\x1B[" + std::to_string(dx) + "D";
      reset_cursor_position_ += "\x1B[" + std::to_string(dx) + "C";
    }

    if (public_->cursor_.shape != Screen::Cursor::Hidden) {
      set_cursor_position_ += "\033[?25h";
      set_cursor_position_ +=
          "\033[" + std::to_string(int(public_->cursor_.shape)) + " q";
    }
  }

  if (update_in_place) {
    public_->ToString(output_buffer, previous_frame_);
  } else {
    public_->ToString(output_buffer);
  }
  TerminalSend(set_cursor_position_);
  TerminalSend(Reset({DECMode::kSynchronizedOutput}));
  TerminalFlush();

  previous_frame_.Swap(public_->cells_, public_->hyperlinks_, public_->dimx_,
                       public_->dimy_);
  public_->cells_.resize(static_cast<size_t>(public_->dimx_) *
                         static_cast<size_t>(public_->dimy_));
  public_->Clear();
  frame_valid_ = true;
  frame_count_++;
}

std::string App::Internal::ResetCursorPosition() {
  std::string result = std::move(reset_cursor_position_);
  reset_cursor_position_ = "";
  return result;
}

void App::Internal::RequestCursorPosition(bool force) {
  cursor_position_request.Request(force);
}

void App::Internal::TerminalSend(std::string_view s) {
  output_buffer += s;
}

void App::Internal::TerminalFlush() {
  // Emscripten doesn't implement flush. We interpret zero as flush.
  output_buffer += '\0';
  std::cout << output_buffer << std::flush;
  output_buffer.clear();
}

void App::Internal::InstallPipedInputHandling() {
  is_stdin_a_tty_ = false;
  is_stdout_a_tty_ = false;
#if defined(__EMSCRIPTEN__)
  is_stdin_a_tty_ = true;
  is_stdout_a_tty_ = true;
#elif defined(_WIN32)
  is_stdin_a_tty_ = _isatty(_fileno(stdin));
  is_stdout_a_tty_ = _isatty(_fileno(stdout));
#else
  tty_fd_ = STDIN_FILENO;
  is_stdout_a_tty_ = isatty(STDOUT_FILENO);
  // Handle piped input redirection if explicitly enabled by the application.
  // This allows applications to read data from stdin while still receiving
  // keyboard input from the terminal for interactive use.
  if (!handle_piped_input_) {
    is_stdin_a_tty_ = isatty(STDIN_FILENO);
  } else if (isatty(STDIN_FILENO)) {
    is_stdin_a_tty_ = true;
  } else {
    // Open /dev/tty for keyboard input.
    tty_fd_ = open("/dev/tty", O_RDONLY);  // NOLINT
    if (tty_fd_ < 0) {
      // Failed to open /dev/tty (containers, headless systems, etc.)
      tty_fd_ = STDIN_FILENO;  // Fallback to stdin.
      is_stdin_a_tty_ = isatty(STDIN_FILENO);
    } else {
      is_stdin_a_tty_ = true;
      // Close the /dev/tty file descriptor on exit.
      on_exit_functions.emplace([this] {
        close(tty_fd_);
        tty_fd_ = -1;
      });
    }
  }
#endif
}

void App::Internal::InstallTerminalInfo() {
  // Request the terminal to report the current cursor shape. We will restore it
  // on exit.
  if (is_stdout_a_tty_) {
    if (SupportsDECRQSS()) {
      TerminalSend(DECRQSS_DECSCUSR);
    }
    TerminalSend("\033[>q");  // XTVERSION
    TerminalSend("\033[>c");  // DA2
    TerminalSend("\033[c");   // DA1
    TerminalFlush();
  }

  // Wait for the cursor shape reply using the setup head.
  if (is_stdin_a_tty_ && is_stdout_a_tty_) {
    // A receiver scoped to the setup: keeping one alive after setup would pin
    // every subsequent event in the buffer, growing it for the whole app
    // lifetime.
    auto setup_receiver = event_buffer.CreateReceiver();
    auto start = std::chrono::steady_clock::now();
    bool terminal_capabilities_received = false;
    // Wait for the cursor shape reply using the setup head.
    while (true) {
      FetchTerminalEvents();
      while (setup_receiver->Has()) {
        const auto event = setup_receiver->Pop();
        if (event.is_cursor_shape()) {
          cursor_reset_shape_ = event.cursor_shape();
        }

        if (event.IsTerminalCapabilities()) {
          terminal_capabilities_ = event.TerminalCapabilities();
          terminal_capabilities_received = true;
        }

        if (event.IsTerminalNameVersion()) {
          terminal_name_ = event.TerminalName();
          terminal_version_ = event.TerminalVersion();
        }

        if (event.IsTerminalEmulator()) {
          terminal_emulator_name_ = event.TerminalEmulatorName();
          terminal_emulator_version_ = event.TerminalEmulatorVersion();
        }
      }

      // Response are expected to be received in order, so we can break when
      // the last one (XTVERSION) is received. We also set a timeout to prevent
      // waiting forever in case the terminal doesn't support these queries.
      if (terminal_capabilities_received) {
        break;
      }

      if (std::chrono::steady_clock::now() - start >
          std::chrono::milliseconds(500)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  // Set quirks and color support based on terminal identification.
  Terminal::Quirks quirks = Terminal::GetQuirks();

  auto color_support = Terminal::ComputeColorSupport(
      util::GetEnv("TERM"), util::GetEnv("COLORTERM"),
      util::GetEnv("TERM_PROGRAM"), terminal_name_, terminal_emulator_name_,
      terminal_capabilities_);

  quirks.SetColorSupport(color_support);

  const bool is_modern_emulator = (terminal_emulator_name_ != "unknown");
  const bool is_vt220_plus =
      (terminal_name_ != "vt100" && terminal_name_ != "unknown");
  bool reports_utf8 = false;
  for (const int x : terminal_capabilities_) {
    if (x == 52) {
      reports_utf8 = true;
      break;
    }
  }

  // Heuristic: If the terminal emulator is modern, or it reports supporting
  // UTF-8 or color, we can assume it supports block characters and cursor
  // hiding, which are essential for a good experience. This is a heuristic, but
  // it allows us to work around some older terminal emulators that don't
  // support these features, while still providing a good experience on modern
  // terminal emulators that do support these features.
  bool modern = is_modern_emulator || is_vt220_plus || reports_utf8;
  if (modern) {
    quirks.SetBlockCharacters(true);
    quirks.SetCursorHiding(true);
    quirks.SetComponentAscii(false);
  }

  Terminal::SetQuirks(quirks);

  on_exit_functions.emplace([this] {
    TerminalSend("\033[?25h");  // Enable cursor.
    if (is_stdout_a_tty_) {
      TerminalSend("\033[" + std::to_string(cursor_reset_shape_) + " q");
    }
  });
}

void App::Internal::Signal(int signal) {
  if (signal == SIGABRT) {
    public_->Exit();
    return;
  }

// Windows do no support SIGTSTP / SIGWINCH
#if !defined(_WIN32)
  if (signal == SIGTSTP) {
    public_->Post([&] {
      TerminalSend(ResetCursorPosition());
      public_->ResetPosition(output_buffer, /*clear*/ true);
      Uninstall();
      public_->dimx_ = 0;
      public_->dimy_ = 0;
      (void)std::raise(SIGTSTP);
      Install();
    });
    return;
  }

  if (signal == SIGWINCH) {
    public_->Post(Event::Special({0}));
    return;
  }
#endif
}

size_t App::Internal::FetchTerminalEvents() {
#if defined(_WIN32)
  auto get_input_records = [&]() -> std::vector<INPUT_RECORD> {
    // Check if there is input in the console.
    auto console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD number_of_events = 0;
    if (!GetNumberOfConsoleInputEvents(console, &number_of_events)) {
      return std::vector<INPUT_RECORD>();
    }
    if (number_of_events <= 0) {
      // No input, return.
      return std::vector<INPUT_RECORD>();
    }
    // Read the input events.
    std::vector<INPUT_RECORD> records(number_of_events);
    DWORD number_of_events_read = 0;
    if (!ReadConsoleInput(console, records.data(), (DWORD)records.size(),
                          &number_of_events_read)) {
      return std::vector<INPUT_RECORD>();
    }
    records.resize(number_of_events_read);
    return records;
  };

  auto records = get_input_records();
  if (records.size() == 0) {
    const auto timeout = std::chrono::steady_clock::now() - last_char_time;
    const size_t timeout_microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
    terminal_input_parser.Timeout(timeout_microseconds);
    return 0;
  }
  last_char_time = std::chrono::steady_clock::now();

  // Convert the input events to FTXUI events.
  // For each event, we call the terminal input parser to convert it to
  // Event.
  std::wstring wstring;
  for (const auto& r : records) {
    switch (r.EventType) {
      case KEY_EVENT: {
        auto key_event = r.Event.KeyEvent;
        // ignore UP key events
        if (key_event.bKeyDown == FALSE) {
          continue;
        }
        const wchar_t wc = key_event.uChar.UnicodeChar;
        wstring += wc;
        if (wc >= 0xd800 && wc <= 0xdbff) {
          // Wait for the Low Surrogate to arrive in the next record.
          continue;
        }
        for (auto it : to_string(wstring)) {
          terminal_input_parser.Add(it);
        }
        wstring.clear();
      } break;
      case WINDOW_BUFFER_SIZE_EVENT:
        public_->Post(Event::Special({0}));
        break;
      case MENU_EVENT:
      case FOCUS_EVENT:
      case MOUSE_EVENT:
        // TODO(mauve): Implement later.
        break;
    }
  }
  return records.size();
#elif defined(__EMSCRIPTEN__)
  // Read chars from the terminal.
  // We configured it to be non blocking.
  std::array<char, 4096> out{};
  const ssize_t l = read(STDIN_FILENO, out.data(), out.size());
  if (l <= 0) {
    const auto timeout = std::chrono::steady_clock::now() - last_char_time;
    const size_t timeout_microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
    terminal_input_parser.Timeout(timeout_microseconds);
    return 0;
  }
  last_char_time = std::chrono::steady_clock::now();

  // Convert the chars to events.
  for (ssize_t i = 0; i < l; ++i) {
    terminal_input_parser.Add(out.at(static_cast<size_t>(i)));
  }
  return (size_t)l;
#else  // POSIX (Linux & Mac)
  struct pollfd pfd = {tty_fd_, POLLIN, 0};
  const int poll_result = poll(&pfd, 1, 0);
  if (poll_result <= 0) {
    const auto timeout = std::chrono::steady_clock::now() - last_char_time;
    const size_t timeout_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
    terminal_input_parser.Timeout(static_cast<int>(timeout_ms));
    return 0;
  }
  last_char_time = std::chrono::steady_clock::now();

  // Drain the available input, so that bursts (e.g. fast mouse wheel
  // scrolling) do not accumulate across frames. The total is bounded to keep
  // the frame responsive under a continuous input flood. See #1348.
  constexpr size_t kMaxBytesPerFetch = 64 * 1024;
  std::array<char, 4096> out{};
  size_t total = 0;
  while (total < kMaxBytesPerFetch) {
    const ssize_t l = read(tty_fd_, out.data(), out.size());
    if (l <= 0) {
      break;
    }

    // Convert the chars to events.
    for (ssize_t i = 0; i < l; ++i) {
      terminal_input_parser.Add(out.at(static_cast<size_t>(i)));
    }
    total += static_cast<size_t>(l);

    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) {
      break;
    }
  }
  return total;
#endif
}

void App::Internal::PostAnimationTask() {
  public_->Post(AnimationTask());

  // Repeat the animation task every 15ms. This correspond to a frame rate
  // of around 66fps.
  task_runner.PostDelayedTask([this] { PostAnimationTask(); },
                              std::chrono::milliseconds(15));
}

App::App(std::unique_ptr<Internal> internal, int dimx, int dimy)
    : Screen(dimx, dimy), internal_(std::move(internal)) {
  internal_->public_ = this;
}

App::App(App&& other) noexcept : Screen(std::move(other)) {
  internal_ = std::move(other.internal_);
  if (internal_) {
    internal_->public_ = this;
  }
}

App& App::operator=(App&& other) noexcept {
  Screen::operator=(std::move(other));
  internal_ = std::move(other.internal_);
  if (internal_) {
    internal_->public_ = this;
  }
  return *this;
}

App::~App() = default;

// static
App App::FixedSize(int dimx, int dimy) {
  auto internal =
      std::make_unique<Internal>(nullptr, AppDimension::Fixed, false);
  return App(std::move(internal), dimx, dimy);
}

// static
App App::Fullscreen() {
  return FullscreenAlternateScreen();
}

// static
App App::FullscreenPrimaryScreen() {
  auto terminal = Terminal::Size();
  auto internal =
      std::make_unique<Internal>(nullptr, AppDimension::Fullscreen, false);
  return App(std::move(internal), terminal.dimx, terminal.dimy);
}

// static
App App::FullscreenAlternateScreen() {
  auto terminal = Terminal::Size();
  auto internal =
      std::make_unique<Internal>(nullptr, AppDimension::Fullscreen, true);
  return App(std::move(internal), terminal.dimx, terminal.dimy);
}

// static
App App::FitComponent() {
  auto terminal = Terminal::Size();
  auto internal =
      std::make_unique<Internal>(nullptr, AppDimension::FitComponent, false);
  return App(std::move(internal), terminal.dimx, terminal.dimy);
}

// static
App App::TerminalOutput() {
  auto terminal = Terminal::Size();
  auto internal =
      std::make_unique<Internal>(nullptr, AppDimension::TerminalOutput, false);
  return App(std::move(internal), terminal.dimx, terminal.dimy);
}

void App::TrackMouse(bool enable) {
  internal_->track_mouse_ = enable;
}

void App::HandlePipedInput(bool enable) {
  internal_->handle_piped_input_ = enable;
}

// static
App* App::Active() {
  return g_active_screen;
}

void App::Loop(Component component) {
  class Loop loop(this, std::move(component));
  loop.Run();
}

void App::Exit() {
  Post([this] { internal_->ExitNow(); });
}

Closure App::ExitLoopClosure() {
  return [this] { Exit(); };
}

void App::PrintAbove(Element element) {
  Post([this, element = std::move(element)] {
    internal_->print_above_.push_back(element);
    internal_->frame_valid_ = false;
  });
}

void App::Post(Task task) {
  internal_->task_runner.PostTask([this, task = std::move(task)]() mutable {
    if (internal_->component_) {
      internal_->HandleTask(internal_->component_, task);
      return;
    }

    // If there is no component, we can still execute closures.
    if (std::holds_alternative<Closure>(task)) {
      std::get<Closure>(task)();
    }
  });
}

void App::PostEvent(Event event) {
  // PostEvent is documented as thread safe: go through the mutex-protected
  // task queue. The event_buffer is only safe to use from the main thread.
  Post(Task(std::move(event)));
}

// static
void App::PostEventOrExecute(Closure closure) {
  if (!closure) {
    return;
  }
  if (auto* app = App::Active()) {
    app->Post(std::move(closure));
  } else {
    closure();
  }
}

void App::RequestAnimationFrame() {
  if (internal_->animation_requested_) {
    return;
  }
  internal_->animation_requested_ = true;
  auto now = animation::Clock::now();
  const auto time_histeresis = std::chrono::milliseconds(33);
  if (now - internal_->previous_animation_time_ >= time_histeresis) {
    internal_->previous_animation_time_ = now;
  }
}

CapturedMouse App::CaptureMouse() {
  if (internal_->mouse_captured) {
    return nullptr;
  }
  internal_->mouse_captured = true;
  return std::make_unique<CapturedMouseImpl>(
      [this] { internal_->mouse_captured = false; });
}

Closure App::WithRestoredIO(Closure fn) {
  return [this, fn] {
    internal_->Uninstall();
    fn();
    internal_->Install();
  };
}

void App::ForceHandleCtrlC(bool force) {
  internal_->force_handle_ctrl_c_ = force;
}

void App::ForceHandleCtrlZ(bool force) {
  internal_->force_handle_ctrl_z_ = force;
}

std::string App::GetSelection() {
  if (!internal_->selection_) {
    return "";
  }
  return internal_->selection_->GetParts();
}

void App::SelectionChange(std::function<void()> callback) {
  internal_->selection_on_change_ = std::move(callback);
}

const std::string& App::TerminalName() const {
  return internal_->terminal_name_;
}

int App::TerminalVersion() const {
  return internal_->terminal_version_;
}

const std::string& App::TerminalEmulatorName() const {
  return internal_->terminal_emulator_name_;
}

const std::string& App::TerminalEmulatorVersion() const {
  return internal_->terminal_emulator_version_;
}

const std::vector<int>& App::TerminalCapabilities() const {
  return internal_->terminal_capabilities_;
}

std::vector<std::string> App::TerminalCapabilityNames() const {
  return Event::TerminalCapabilities("", internal_->terminal_capabilities_)
      .TerminalCapabilityNames();
}

// Loop calls these:

void App::ExitNow() {
  internal_->ExitNow();
}
void App::Install() {
  internal_->Install();
}
void App::Uninstall() {
  internal_->Uninstall();
}
void App::PreMain() {
  internal_->PreMain();
}
void App::PostMain() {
  internal_->PostMain();
}
bool App::HasQuitted() {
  return internal_->HasQuitted();
}
void App::RunOnce(const Component& component) {
  internal_->RunOnce(component);
}
void App::RunOnceBlocking(Component component) {
  internal_->RunOnceBlocking(component);
}
void App::HandleTask(Component component, Task& task) {
  internal_->HandleTask(component, task);
}
bool App::HandleSelection(bool handled, Event event) {
  return internal_->HandleSelection(handled, event);
}
void App::Draw(Component component) {
  internal_->Draw(component);
}
std::string App::ResetCursorPosition() {
  return internal_->ResetCursorPosition();
}
void App::RequestCursorPosition(bool force) {
  internal_->RequestCursorPosition(force);
}
void App::TerminalSend(std::string_view s) {
  internal_->TerminalSend(s);
}
void App::TerminalFlush() {
  internal_->TerminalFlush();
}
void App::InstallPipedInputHandling() {
  internal_->InstallPipedInputHandling();
}
void App::InstallTerminalInfo() {
  internal_->InstallTerminalInfo();
}
void App::Signal(int signal) {
  internal_->Signal(signal);
}
size_t App::FetchTerminalEvents() {
  return internal_->FetchTerminalEvents();
}
void App::PostAnimationTask() {
  internal_->PostAnimationTask();
}

Loop::Loop(App* screen, Component component)
    : screen_(screen), component_(std::move(component)) {
  screen_->PreMain();
}

Loop::~Loop() {
  screen_->PostMain();
}

bool Loop::HasQuitted() {
  return screen_->HasQuitted();
}

void Loop::RunOnce() {
  screen_->RunOnce(component_);
}

void Loop::RunOnceBlocking() {
  screen_->RunOnceBlocking(component_);
}

void Loop::Run() {
  while (!HasQuitted()) {
    RunOnceBlocking();
  }
}

}  // namespace ftxui

// ---- component\button.cpp ----------------------------------------



namespace ftxui {

namespace {

Element DefaultTransform(EntryState params) {  // NOLINT
  auto element = text(params.label) | border;
  if (params.active) {
    element |= bold;
  }
  if (params.focused) {
    element |= inverted;
  }
  return element;
}

class ButtonBase : public ComponentBase, public ButtonOption {
 public:
  explicit ButtonBase(ButtonOption option) : ButtonOption(std::move(option)) {}

  // Component implementation:
  Element OnRender() override {
    const bool active = Active();
    const bool focused = Focused();
    const bool focused_or_hover = focused || mouse_hover_;

    float target = focused_or_hover ? 1.f : 0.f;  // NOLINT
    if (target != animator_background_.to()) {
      SetAnimationTarget(target);
    }

    const EntryState state{
        std::string(*label), false, active, focused_or_hover, Index(),
    };

    auto element = (transform ? transform : DefaultTransform)  //
        (state);
    element |= AnimatedColorStyle();
    element |= focus;
    element |= reflect(box_);
    return element;
  }

  Decorator AnimatedColorStyle() {
    Decorator style = nothing;
    if (animated_colors.background.enabled) {
      style = style |
              bgcolor(Color::Interpolate(animation_foreground_,  //
                                         animated_colors.background.inactive,
                                         animated_colors.background.active));
    }
    if (animated_colors.foreground.enabled) {
      style =
          style | color(Color::Interpolate(animation_foreground_,  //
                                           animated_colors.foreground.inactive,
                                           animated_colors.foreground.active));
    }
    return style;
  }

  void SetAnimationTarget(float target) {
    if (animated_colors.foreground.enabled) {
      animator_foreground_ = animation::Animator(
          &animation_foreground_, target, animated_colors.foreground.duration,
          animated_colors.foreground.function);
    }
    if (animated_colors.background.enabled) {
      animator_background_ = animation::Animator(
          &animation_background_, target, animated_colors.background.duration,
          animated_colors.background.function);
    }
  }

  void OnAnimation(animation::Params& p) override {
    animator_background_.OnAnimation(p);
    animator_foreground_.OnAnimation(p);
  }

  void OnClick() {
    animation_background_ = 0.5F;  // NOLINT
    animation_foreground_ = 0.5F;  // NOLINT
    SetAnimationTarget(1.F);       // NOLINT

    App::PostEventOrExecute(on_click);
  }

  bool OnEvent(Event event) override {
    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    if (event == Event::Return) {
      OnClick();  // May delete this.
      return true;
    }
    return false;
  }

  bool OnMouseEvent(Event event) {
    mouse_hover_ =
        box_.Contain(event.mouse().x, event.mouse().y) && CaptureMouse(event);

    if (!mouse_hover_) {
      return false;
    }

    if (event.mouse().button == Mouse::Left &&
        event.mouse().motion == Mouse::Pressed) {
      TakeFocus();
      OnClick();  // May delete this.
      return true;
    }

    return false;
  }

  bool Focusable() const final { return true; }

 private:
  bool mouse_hover_ = false;
  Box box_;
  float animation_background_ = 0;
  float animation_foreground_ = 0;
  animation::Animator animator_background_ =
      animation::Animator(&animation_background_);
  animation::Animator animator_foreground_ =
      animation::Animator(&animation_foreground_);
};

}  // namespace

/// @brief Draw a button. Execute a function when clicked.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see ButtonBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// Component button = Button({
///   .label = "Click to quit",
///   .on_click = screen.ExitLoopClosure(),
/// });
/// screen.Loop(button)
/// ```
///
/// ### Output
///
/// ```bash
/// ┌─────────────┐
/// │Click to quit│
/// └─────────────┘
/// ```
Component Button(ButtonOption option) {
  return Make<ButtonBase>(std::move(option));
}

/// @brief Draw a button. Execute a function when clicked.
/// @param label The label of the button.
/// @param on_click The action to execute when clicked.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see ButtonBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// std::string label = "Click to quit";
/// Component button = Button(&label, screen.ExitLoopClosure());
/// screen.Loop(button)
/// ```
///
/// ### Output
///
/// ```bash
/// ┌─────────────┐
/// │Click to quit│
/// └─────────────┘
/// ```
// NOLINTNEXTLINE
Component Button(ConstStringRef label,
                 std::function<void()> on_click,
                 ButtonOption option) {
  option.label = std::move(label);
  option.on_click = std::move(on_click);
  return Make<ButtonBase>(std::move(option));
}

}  // namespace ftxui

// ---- component\catch_event.cpp -----------------------------------


namespace ftxui {

class CatchEventBase : public ComponentBase {
 public:
  // Constructor.
  explicit CatchEventBase(std::function<bool(Event)> on_event)
      : on_event_(std::move(on_event)) {}

  // Component implementation.
  Component ActiveChild() override {
    return Active() ? ComponentBase::ActiveChild() : nullptr;
  }

  bool OnEvent(Event event) override {
    if (on_event_(event)) {
      return true;
    } else {
      return ComponentBase::OnEvent(event);
    }
  }

 protected:
  std::function<bool(Event)> on_event_;
};

/// @brief Return a component, using |on_event| to catch events. This function
/// must returns true when the event has been handled, false otherwise.
/// @param child The wrapped component.
/// @param on_event The function drawing the interface.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// auto renderer = Renderer([] {
///   return text("My interface");
/// });
/// auto component = CatchEvent(renderer, [&](Event event) {
///   if (event == Event::Character('q')) {
///     screen.ExitLoopClosure()();
///     return true;
///   }
///   return false;
/// });
/// screen.Loop(component);
/// ```
Component CatchEvent(Component child,
                     std::function<bool(Event event)> on_event) {
  auto out = Make<CatchEventBase>(std::move(on_event));
  out->Add(std::move(child));
  return out;
}

/// @brief Decorate a component, using |on_event| to catch events. This function
/// must returns true when the event has been handled, false otherwise.
/// @param on_event The function drawing the interface.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// auto renderer = Renderer([] { return text("Hello world"); });
/// renderer |= CatchEvent([&](Event event) {
///   if (event == Event::Character('q')) {
///     screen.ExitLoopClosure()();
///     return true;
///   }
///   return false;
/// });
/// screen.Loop(renderer);
/// ```
ComponentDecorator CatchEvent(std::function<bool(Event)> on_event) {
  return [on_event = std::move(on_event)](Component child) {
    return CatchEvent(std::move(child), [on_event = on_event](Event event) {
      return on_event(std::move(event));
    });
  };
}

}  // namespace ftxui

// ---- component\checkbox.cpp --------------------------------------


namespace ftxui {

namespace {
class CheckboxBase : public ComponentBase, public CheckboxOption {
 public:
  explicit CheckboxBase(CheckboxOption option)
      : CheckboxOption(std::move(option)) {}

 private:
  // Component implementation.
  Element OnRender() override {
    const bool is_focused = Focused();
    const bool is_active = Active();
    auto entry_state = EntryState{
        std::string(*label), *checked, is_active, is_focused || hovered_, -1,
    };
    auto element = (transform ? transform : CheckboxOption::Simple().transform)(
        entry_state);
    element |= focus;
    element |= reflect(box_);
    return element;
  }

  bool OnEvent(Event event) override {
    if (!CaptureMouse(event)) {
      return false;
    }

    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    hovered_ = false;
    if (event == Event::Character(' ') || event == Event::Return) {
      *checked = !*checked;
      App::PostEventOrExecute(on_change);
      TakeFocus();
      return true;
    }
    return false;
  }

  bool OnMouseEvent(Event event) {
    hovered_ = box_.Contain(event.mouse().x, event.mouse().y);

    if (!CaptureMouse(event)) {
      return false;
    }

    if (!hovered_) {
      return false;
    }

    if (event.mouse().button == Mouse::Left &&
        event.mouse().motion == Mouse::Pressed) {
      *checked = !*checked;
      App::PostEventOrExecute(on_change);
      TakeFocus();
      return true;
    }

    return false;
  }

  bool Focusable() const final { return true; }

  bool hovered_ = false;
  Box box_;
};
}  // namespace

/// @brief Draw checkable element.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see CheckboxBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// CheckboxOption option;
/// option.label = "Make a sandwidth";
/// option.checked = false;
/// Component checkbox = Checkbox(option);
/// screen.Loop(checkbox)
/// ```
///
/// ### Output
///
/// ```bash
/// ☐ Make a sandwich
/// ```
// NOLINTNEXTLINE
Component Checkbox(CheckboxOption option) {
  return Make<CheckboxBase>(std::move(option));
}

/// @brief Draw checkable element.
/// @param label The label of the checkbox.
/// @param checked Whether the checkbox is checked or not.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see CheckboxBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// std::string label = "Make a sandwidth";
/// bool checked = false;
/// Component checkbox = Checkbox(&label, &checked);
/// screen.Loop(checkbox)
/// ```
///
/// ### Output
///
/// ```bash
/// ☐ Make a sandwich
/// ```
// NOLINTNEXTLINE
Component Checkbox(ConstStringRef label, bool* checked, CheckboxOption option) {
  option.label = std::move(label);
  option.checked = checked;
  return Make<CheckboxBase>(std::move(option));
}

}  // namespace ftxui

// ---- component\collapsible.cpp -----------------------------------


namespace ftxui {

/// @brief A collapsible component. It displays a checkbox with an arrow. Once
/// activated, the child is displayed.
/// @param label The label of the checkbox.
/// @param child The child to display.
/// @param show Hold the state about whether the child is displayed or not.
///
/// ### Example
/// ```cpp
/// auto component = Collapsible("Show details", details);
/// ```
///
/// ### Output
/// ```
///
/// ▼ Show details
/// <details component>
/// ```
// NOLINTNEXTLINE
Component Collapsible(ConstStringRef label, Component child, Ref<bool> show) {
  class Impl : public ComponentBase {
   public:
    Impl(ConstStringRef label, Component child, Ref<bool> show) : show_(show) {
      CheckboxOption opt;
      opt.transform = [](EntryState s) {            // NOLINT
        auto prefix = text(s.state ? "▼ " : "▶ ");  // NOLINT
        auto t = text(s.label);
        if (s.active) {
          t |= bold;
        }
        if (s.focused) {
          t |= inverted;
        }
        return hbox({prefix, t});
      };
      Add(Container::Vertical({
          Checkbox(std::move(label), show_.operator->(), opt),
          Maybe(std::move(child), show_.operator->()),
      }));
    }
    Ref<bool> show_;
  };

  return Make<Impl>(std::move(label), std::move(child), show);
}

}  // namespace ftxui

// ---- component\component.cpp -------------------------------------


namespace ftxui::animation {
class Params;
}  // namespace ftxui::animation

namespace ftxui {

namespace {
class CaptureMouseImpl : public CapturedMouseInterface {};
}  // namespace

struct ComponentBase::Impl {
  Components children;
  ComponentBase* parent = nullptr;
  bool in_render = false;
};

ComponentBase::ComponentBase() : impl_(std::make_unique<Impl>()) {}

ComponentBase::ComponentBase(Components children)
    : impl_(std::make_unique<Impl>()) {
  impl_->children = std::move(children);
}

ComponentBase::~ComponentBase() {
  DetachAllChildren();
}

Components& ComponentBase::children() {
  return impl_->children;
}

const Components& ComponentBase::children() const {
  return impl_->children;
}

/// @brief Return the parent ComponentBase, or nul if any.
/// @see Detach
/// @see Parent
ComponentBase* ComponentBase::Parent() const {
  return impl_->parent;
}

/// @brief Access the child at index `i`.
Component& ComponentBase::ChildAt(size_t i) {
  assert(i < ChildCount());  // NOLINT
  return impl_->children[i];
}

/// @brief Returns the number of children.
size_t ComponentBase::ChildCount() const {
  return impl_->children.size();
}

/// @brief Return index of the component in its parent. -1 if no parent.
int ComponentBase::Index() const {
  if (impl_->parent == nullptr) {
    return -1;
  }
  int index = 0;
  for (const Component& child : impl_->parent->impl_->children) {
    if (child.get() == this) {
      return index;
    }
    index++;
  }
  return -1;  // Not reached.
}

/// @brief Add a child.
/// @param child The child to be attached.
void ComponentBase::Add(Component child) {
  child->Detach();
  child->impl_->parent = this;
  impl_->children.push_back(std::move(child));
}

/// @brief Detach this child from its parent.
/// @see Detach
/// @see Parent
void ComponentBase::Detach() {
  if (impl_->parent == nullptr) {
    return;
  }
  auto it = std::find_if(std::begin(impl_->parent->impl_->children),  // NOLINT
                         std::end(impl_->parent->impl_->children),    //
                         [this](const Component& that) {              //
                           return this == that.get();
                         });
  ComponentBase* parent = impl_->parent;
  impl_->parent = nullptr;
  parent->impl_->children.erase(it);  // Might delete |this|.
}

/// @brief Remove all children.
void ComponentBase::DetachAllChildren() {
  while (!impl_->children.empty()) {
    impl_->children[0]->Detach();
  }
}

/// @brief Draw the component.
/// Build a ftxui::Element to be drawn on the ftxui::Screen representing this
/// ftxui::ComponentBase. Please override OnRender() to modify the rendering.
Element ComponentBase::Render() {
  // Some users might call `ComponentBase::Render()` from
  // `T::OnRender()`. To avoid infinite recursion, we use a flag.
  if (impl_->in_render) {
    return ComponentBase::OnRender();
  }

  impl_->in_render = true;
  Element element = OnRender();
  impl_->in_render = false;

  class Wrapper : public Node {
   public:
    bool active_ = false;
    bool focused_ = false;

    Wrapper(Element child, bool active, bool focused)
        : Node({std::move(child)}), active_(active), focused_(focused) {}

    void SetBox(Box box) override {
      Node::SetBox(box);
      children_[0]->SetBox(box);
    }

    void ComputeRequirement() override {
      Node::ComputeRequirement();
      requirement_.focused.component_active = active_;
      requirement_.focused.component_focused = focused_;
    }
  };

  return std::make_shared<Wrapper>(std::move(element), Active(), Focused());
}

/// @brief Draw the component.
/// Build a ftxui::Element to be drawn on the ftxi::Screen representing this
/// ftxui::ComponentBase. This function is means to be overridden.
Element ComponentBase::OnRender() {
  if (impl_->children.size() == 1) {
    return impl_->children.front()->Render();
  }

  return text("Not implemented component");
}

/// @brief Called in response to an event.
/// @param event The event.
/// @return True when the event has been handled.
/// The default implementation called OnEvent on every child until one return
/// true. If none returns true, return false.
bool ComponentBase::OnEvent(Event event) {    // NOLINT
  for (Component& child : impl_->children) {  // NOLINT
    if (child->OnEvent(event)) {
      return true;
    }
  }
  return false;
}

/// @brief Called in response to an animation event.
/// @param params the parameters of the animation
/// The default implementation dispatch the event to every child.
void ComponentBase::OnAnimation(animation::Params& params) {
  for (const Component& child : impl_->children) {
    child->OnAnimation(params);
  }
}

/// @brief Return the currently Active child.
/// @return the currently Active child.
Component ComponentBase::ActiveChild() {
  for (auto& child : impl_->children) {
    if (child->Focusable()) {
      return child;
    }
  }
  return nullptr;
}

/// @brief Return true when the component contains focusable elements.
/// The non focusable Components will be skipped when navigating using the
/// keyboard.
bool ComponentBase::Focusable() const {
  for (const Component& child : impl_->children) {  // NOLINT
    if (child->Focusable()) {
      return true;
    }
  }
  return false;
}

/// @brief Returns if the element if the currently active child of its parent.
bool ComponentBase::Active() const {
  return impl_->parent == nullptr || impl_->parent->ActiveChild().get() == this;
}

/// @brief Returns if the elements if focused by the user.
/// True when the ComponentBase is focused by the user. An element is Focused
/// when it is with all its ancestors the ActiveChild() of their parents, and it
/// Focusable().
bool ComponentBase::Focused() const {
  const auto* current = this;
  while (current && current->Active()) {
    current = current->impl_->parent;
  }
  return !current && Focusable();
}

/// @brief Make the |child| to be the "active" one.
/// @param child the child to become active.
void ComponentBase::SetActiveChild([[maybe_unused]] ComponentBase* child) {}

/// @brief Make the |child| to be the "active" one.
/// @param child the child to become active.
void ComponentBase::SetActiveChild(Component child) {  // NOLINT
  SetActiveChild(child.get());
}

/// @brief Configure all the ancestors to give focus to this component.
void ComponentBase::TakeFocus() {
  ComponentBase* child = this;
  while (ComponentBase* parent = child->impl_->parent) {
    parent->SetActiveChild(child);
    child = parent;
  }
}

/// @brief Take the CapturedMouse if available. There is only one component of
/// them. It represents a component taking priority over others.
/// @param event The event
CapturedMouse ComponentBase::CaptureMouse(const Event& event) {  // NOLINT
  if (event.screen_) {
    return event.screen_->CaptureMouse();
  }
  return std::make_unique<CaptureMouseImpl>();
}

void ComponentBase::Reserved1() {}
void ComponentBase::Reserved2() {}
void ComponentBase::Reserved3() {}
void ComponentBase::Reserved4() {}
void ComponentBase::Reserved5() {}
void ComponentBase::Reserved6() {}
void ComponentBase::Reserved7() {}
void ComponentBase::Reserved8() {}

}  // namespace ftxui

// ---- component\component_options.cpp -----------------------------


namespace ftxui {

/// @brief A color option that can be animated.
/// @param _inactive The color when the component is inactive.
/// @param _active The color when the component is active.
/// @param _duration The duration of the animation.
/// @param _function The easing function of the animation.
void AnimatedColorOption::Set(Color _inactive,
                              Color _active,
                              animation::Duration _duration,
                              animation::easing::Function _function) {
  enabled = true;
  inactive = _inactive;
  active = _active;
  duration = _duration;
  function = std::move(_function);
}

/// @brief Set how the underline should animate.
/// @param d The duration of the animation.
/// @param f The easing function of the animation.
void UnderlineOption::SetAnimation(animation::Duration d,
                                   animation::easing::Function f) {
  SetAnimationDuration(d);
  SetAnimationFunction(std::move(f));
}

/// @brief Set how the underline should animate.
/// @param d The duration of the animation.
void UnderlineOption::SetAnimationDuration(animation::Duration d) {
  leader_duration = d;
  follower_duration = d;
}

/// @brief Set how the underline should animate.
/// @param f The easing function of the animation.
void UnderlineOption::SetAnimationFunction(animation::easing::Function f) {
  leader_function = f;
  follower_function = std::move(f);
}

/// @brief Set how the underline should animate.
/// This is useful to desynchronize the animation of the leader and the
/// follower.
/// @param f_leader The duration of the animation for the leader.
/// @param f_follower The duration of the animation for the follower.
void UnderlineOption::SetAnimationFunction(
    animation::easing::Function f_leader,
    animation::easing::Function f_follower) {
  leader_function = std::move(f_leader);
  follower_function = std::move(f_follower);
}

/// @brief Standard options for a horizontal menu.
/// This can be useful to implement a tab bar.
// static
MenuOption MenuOption::Horizontal() {
  MenuOption option;
  option.direction = Direction::Right;
  option.entries_option.transform = [](const EntryState& state) {
    Element e = text(state.label);
    if (state.focused) {
      e |= inverted;
    }
    if (state.active) {
      e |= bold;
    }
    if (!state.focused && !state.active) {
      e |= dim;
    }
    return e;
  };
  option.elements_infix = [] { return text(" "); };

  return option;
}

/// @brief Standard options for an animated horizontal menu.
/// This can be useful to implement a tab bar.
// static
MenuOption MenuOption::HorizontalAnimated() {
  auto option = Horizontal();
  option.underline.enabled = true;
  return option;
}

/// @brief Standard options for a vertical menu.
/// This can be useful to implement a list of selectable items.
// static
MenuOption MenuOption::Vertical() {
  MenuOption option;
  option.entries_option.transform = [](const EntryState& state) {
    Element e = text((state.active ? "> " : "  ") + state.label);  // NOLINT
    if (state.focused) {
      e |= inverted;
    }
    if (state.active) {
      e |= bold;
    }
    if (!state.focused && !state.active) {
      e |= dim;
    }
    return e;
  };
  return option;
}

/// @brief Standard options for an animated vertical menu.
/// This can be useful to implement a list of selectable items.
// static
MenuOption MenuOption::VerticalAnimated() {
  auto option = MenuOption::Vertical();
  option.entries_option.transform = [](const EntryState& state) {
    Element e = text(state.label);
    if (state.focused) {
      e |= inverted;
    }
    if (state.active) {
      e |= bold;
    }
    if (!state.focused && !state.active) {
      e |= dim;
    }
    return e;
  };
  option.underline.enabled = true;
  return option;
}

/// @brief Standard options for a horizontal menu with some separator.
/// This can be useful to implement a tab bar.
// static
MenuOption MenuOption::Toggle() {
  auto option = MenuOption::Horizontal();
  option.elements_infix = [] { return text("│") | automerge; };
  return option;
}

/// @brief Create a ButtonOption, highlighted using [] characters.
// static
ButtonOption ButtonOption::Ascii() {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    const std::string t = s.focused ? "[" + s.label + "]"  //
                                    : " " + s.label + " ";
    return text(t);
  };
  return option;
}

/// @brief Create a ButtonOption, inverted when focused.
// static
ButtonOption ButtonOption::Simple() {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    auto element = text(s.label) | borderLight;
    if (s.focused) {
      element |= inverted;
    }
    return element;
  };
  return option;
}

/// @brief Create a ButtonOption. The button is shown using a border, inverted
/// when focused. This is the current default.
ButtonOption ButtonOption::Border() {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    auto element = text(s.label) | border;
    if (s.active) {
      element |= bold;
    }
    if (s.focused) {
      element |= inverted;
    }
    return element;
  };
  return option;
}

/// @brief Create a ButtonOption, using animated colors.
// static
ButtonOption ButtonOption::Animated() {
  return Animated(Color::Black, Color::GrayLight,  //
                  Color::GrayDark, Color::White);
}

/// @brief Create a ButtonOption, using animated colors.
// static
ButtonOption ButtonOption::Animated(Color color) {
  return ButtonOption::Animated(
      Color::Interpolate(0.85F, color, Color::Black),   // NOLINT
      Color::Interpolate(0.10F, color, Color::White),   // NOLINT
      Color::Interpolate(0.10F, color, Color::Black),   // NOLINT
      Color::Interpolate(0.85F, color, Color::White));  // NOLINT
}

/// @brief Create a ButtonOption, using animated colors.
// static
ButtonOption ButtonOption::Animated(Color background, Color foreground) {
  // NOLINTBEGIN
  return ButtonOption::Animated(
      /*background=*/background,
      /*foreground=*/foreground,
      /*background_active=*/foreground,
      /*foreground_active=*/background);
  // NOLINTEND
}

/// @brief Create a ButtonOption, using animated colors.
// static
ButtonOption ButtonOption::Animated(Color background,
                                    Color foreground,
                                    Color background_active,
                                    Color foreground_active) {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    auto element = text(s.label) | borderEmpty;
    if (s.focused) {
      element |= bold;
    }
    return element;
  };
  option.animated_colors.foreground.Set(foreground, foreground_active);
  option.animated_colors.background.Set(background, background_active);
  return option;
}

/// @brief Option for standard Checkbox.
// static
CheckboxOption CheckboxOption::Simple() {
  auto option = CheckboxOption();
  option.transform = [](const EntryState& s) {
    auto prefix = (Terminal::GetQuirks().ComponentAscii())
                      ? text(s.state ? "[X] " : "[ ] ")  // NOLINT
                      : text(s.state ? "▣ " : "☐ ");     // NOLINT
    auto t = text(s.label);
    if (s.active) {
      t |= bold;
    }
    if (s.focused) {
      t |= inverted;
    }
    return hbox({prefix, t});
  };
  return option;
}

/// @brief Option for standard Radiobox
// static
RadioboxOption RadioboxOption::Simple() {
  auto option = RadioboxOption();
  option.transform = [](const EntryState& s) {
    auto prefix = (Terminal::GetQuirks().ComponentAscii())
                      ? text(s.state ? "(*) " : "( ) ")  // NOLINT
                      : text(s.state ? "◉ " : "○ ");     // NOLINT
    auto t = text(s.label);
    if (s.active) {
      t |= bold;
    }
    if (s.focused) {
      t |= inverted;
    }
    return hbox({prefix, t});
  };
  return option;
}

/// @brief Standard options for the input component.
// static
InputOption InputOption::Default() {
  InputOption option;
  option.transform = [](InputState state) {
    if (state.is_placeholder) {
      state.element |= dim;
    }

    if (state.focused) {
      state.element |= inverted;
    } else if (state.hovered) {
      state.element |= underlined;
    }

    return state.element;
  };
  return option;
}

/// @brief Standard options for a more beautiful input component.
// static
InputOption InputOption::Spacious() {
  InputOption option;
  option.transform = [](InputState state) {
    state.element |= borderEmpty;

    if (state.is_placeholder) {
      state.element |= dim;
    }

    if (state.focused) {
      state.element |= inverted;
    } else if (state.hovered) {
      state.element |= bold;
    }

    return state.element;
  };
  return option;
}

}  // namespace ftxui

// ---- component\component_util.cpp --------------------------------


namespace ftxui {

// NOLINTNEXTLINE
Component operator|(Component component, ComponentDecorator decorator) {
  return decorator(component);  // NOLINT
}

// NOLINTNEXTLINE
Component operator|(Component component, ElementDecorator decorator) {
  return component | Renderer(decorator);  // NOLINT
}

// NOLINTNEXTLINE
Component& operator|=(Component& component, ComponentDecorator decorator) {
  component = component | decorator;  // NOLINT
  return component;
}

// NOLINTNEXTLINE
Component& operator|=(Component& component, ElementDecorator decorator) {
  component = component | decorator;  // NOLINT
  return component;
}

}  // namespace ftxui

// ---- component\container.cpp -------------------------------------


namespace ftxui {

class ContainerBase : public ComponentBase {
 public:
  ContainerBase(Components children, int* selector)
      : selector_(selector ? selector : &selected_) {
    for (Component& child : children) {
      Add(std::move(child));
    }
  }

  // Component override.
  bool OnEvent(Event event) override {
    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    if (!Focused()) {
      return false;
    }

    if (ActiveChild() && ActiveChild()->OnEvent(event)) {
      return true;
    }

    return EventHandler(event);
  }

  Component ActiveChild() override {
    if (children().empty()) {
      return nullptr;
    }

    return children()[static_cast<size_t>(*selector_) % children().size()];
  }

  void SetActiveChild(ComponentBase* child) override {
    for (size_t i = 0; i < children().size(); ++i) {
      if (children()[i].get() == child) {
        *selector_ = static_cast<int>(i);
        return;
      }
    }
  }

 protected:
  // Handlers
  virtual bool EventHandler(Event /*unused*/) { return false; }  // NOLINT

  virtual bool OnMouseEvent(Event event) {
    return ComponentBase::OnEvent(std::move(event));
  }

  int selected_ = 0;
  int* selector_ = nullptr;

  void EnsureFocusableSelection() {
    if (!children().empty() && !ActiveChild()->Focusable()) {
      MoveSelectorWrap(+1);
    }
  }

  void MoveSelector(int dir) {
    for (int i = *selector_ + dir; i >= 0 && i < int(children().size());
         i += dir) {
      if (children()[i]->Focusable()) {
        *selector_ = i;
        return;
      }
    }
  }

  void MoveSelectorWrap(int dir) {
    if (children().empty()) {
      return;
    }
    for (size_t offset = 1; offset < children().size(); ++offset) {
      const size_t i =
          (*selector_ + offset * dir + children().size()) % children().size();
      if (children()[i]->Focusable()) {
        *selector_ = int(i);
        return;
      }
    }
  }
};

class VerticalContainer : public ContainerBase {
 public:
  VerticalContainer(Components children, int* selector)
      : ContainerBase(std::move(children), selector) {
    EnsureFocusableSelection();
  }

  Element OnRender() override {
    EnsureFocusableSelection();
    Elements elements;
    elements.reserve(children().size());
    for (auto& it : children()) {
      elements.push_back(it->Render());
    }
    if (elements.empty()) {
      return text("Empty container") | reflect(box_);
    }
    return vbox(std::move(elements)) | reflect(box_);
  }

  bool EventHandler(Event event) override {
    const int old_selected = *selector_;
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      MoveSelector(-1);
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      MoveSelector(+1);
    }
    if (event == Event::PageUp) {
      for (int i = 0; i < box_.y_max - box_.y_min; ++i) {
        MoveSelector(-1);
      }
    }
    if (event == Event::PageDown) {
      for (int i = 0; i < box_.y_max - box_.y_min; ++i) {
        MoveSelector(1);
      }
    }
    if (event == Event::Home) {
      for (size_t i = 0; i < children().size(); ++i) {
        MoveSelector(-1);
      }
    }
    if (event == Event::End) {
      for (size_t i = 0; i < children().size(); ++i) {
        MoveSelector(1);
      }
    }
    if (event == Event::Tab) {
      MoveSelectorWrap(+1);
    }
    if (event == Event::TabReverse) {
      MoveSelectorWrap(-1);
    }

    *selector_ = std::max(0, std::min(int(children().size()) - 1, *selector_));
    return old_selected != *selector_;
  }

  bool OnMouseEvent(Event event) override {
    if (ContainerBase::OnMouseEvent(event)) {
      return true;
    }

    if (event.mouse().button != Mouse::WheelUp &&
        event.mouse().button != Mouse::WheelDown) {
      return false;
    }

    if (!box_.Contain(event.mouse().x, event.mouse().y)) {
      return false;
    }

    const int old_selected = *selector_;
    if (event.mouse().button == Mouse::WheelUp) {
      MoveSelector(-1);
    }
    if (event.mouse().button == Mouse::WheelDown) {
      MoveSelector(+1);
    }
    *selector_ = std::max(0, std::min(int(children().size()) - 1, *selector_));

    return old_selected != *selector_;
  }

  Box box_;
};

class HorizontalContainer : public ContainerBase {
 public:
  HorizontalContainer(Components children, int* selector)
      : ContainerBase(std::move(children), selector) {
    EnsureFocusableSelection();
  }

  Element OnRender() override {
    EnsureFocusableSelection();
    Elements elements;
    elements.reserve(children().size());
    for (auto& it : children()) {
      elements.push_back(it->Render());
    }
    if (elements.empty()) {
      return text("Empty container");
    }
    return hbox(std::move(elements));
  }

  bool EventHandler(Event event) override {
    const int old_selected = *selector_;
    if (event == Event::ArrowLeft || event == Event::Character('h')) {
      MoveSelector(-1);
    }
    if (event == Event::ArrowRight || event == Event::Character('l')) {
      MoveSelector(+1);
    }
    if (event == Event::Tab) {
      MoveSelectorWrap(+1);
    }
    if (event == Event::TabReverse) {
      MoveSelectorWrap(-1);
    }

    *selector_ = std::max(0, std::min(int(children().size()) - 1, *selector_));
    return old_selected != *selector_;
  }
};

class TabContainer : public ContainerBase {
 public:
  using ContainerBase::ContainerBase;

  Element OnRender() override {
    const Component active_child = ActiveChild();
    if (active_child) {
      return active_child->Render();
    }
    return text("Empty container");
  }

  bool Focusable() const override {
    if (children().empty()) {
      return false;
    }
    return children()[size_t(*selector_) % children().size()]->Focusable();
  }

  bool OnMouseEvent(Event event) override {
    return ActiveChild() && ActiveChild()->OnEvent(event);
  }
};

class StackedContainer : public ContainerBase {
 public:
  explicit StackedContainer(Components children)
      : ContainerBase(std::move(children), nullptr) {}

 private:
  Element OnRender() final {
    Elements elements;
    for (auto& child : children()) {
      elements.push_back(child->Render());
    }
    // Reverse the order of the elements.
    std::reverse(elements.begin(), elements.end());  // NOLINT
    return dbox(std::move(elements));
  }

  bool Focusable() const final {
    for (const auto& child : children()) {
      if (child->Focusable()) {
        return true;
      }
    }
    return false;
  }

  Component ActiveChild() final {
    if (children().empty()) {
      return nullptr;
    }
    return children()[0];
  }

  void SetActiveChild(ComponentBase* child) final {
    if (children().empty()) {
      return;
    }

    // Find `child` and put it at the beginning without change the order of the
    // other children.
    auto it =
        std::find_if(children().begin(), children().end(),  // NOLINT
                     [child](const Component& c) { return c.get() == child; });
    if (it == children().end()) {
      return;
    }
    std::rotate(children().begin(), it, it + 1);
  }

  bool OnEvent(Event event) final {
    for (auto& child : children()) {
      if (child->OnEvent(event)) {
        return true;
      }
    }
    return false;
  }
};

namespace Container {

/// @brief A list of components, drawn one by one vertically and navigated
/// vertically using up/down arrow key or 'j'/'k' keys.
/// @param children the list of components.
/// @ingroup component
/// @see ContainerBase
///
/// ### Example
///
/// ```cpp
/// auto container = Container::Vertical({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// });
/// ```
Component Vertical(Components children) {
  return Vertical(std::move(children), nullptr);
}

/// @brief A list of components, drawn one by one vertically and navigated
/// vertically using up/down arrow key or 'j'/'k' keys.
/// This is useful for implementing a Menu for instance.
/// @param children the list of components.
/// @param selector A reference to the index of the selected children.
/// @ingroup component
/// @see ContainerBase
///
/// ### Example
///
/// ```cpp
/// int selected_children = 2;
/// auto container = Container::Vertical({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// }, &selected_children);
/// ```
Component Vertical(Components children, int* selector) {
  return std::make_shared<VerticalContainer>(std::move(children), selector);
}

/// @brief A list of components, drawn one by one horizontally and navigated
/// horizontally using left/right arrow key or 'h'/'l' keys.
/// @param children the list of components.
/// @ingroup component
/// @see ContainerBase
///
/// ### Example
///
/// ```cpp
/// auto container = Container::Horizontal({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// });
/// ```
Component Horizontal(Components children) {
  return Horizontal(std::move(children), nullptr);
}

/// @brief A list of components, drawn one by one horizontally and navigated
/// horizontally using left/right arrow key or 'h'/'l' keys.
/// @param children the list of components.
/// @param selector A reference to the index of the selected children.
/// @ingroup component
/// @see ContainerBase
///
/// ### Example
///
/// ```cpp
/// int selected_children = 2;
/// auto container = Container::Horizontal({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// }, selected_children);
/// ```
Component Horizontal(Components children, int* selector) {
  return std::make_shared<HorizontalContainer>(std::move(children), selector);
}

/// @brief A list of components, where only one is drawn and interacted with at
/// a time. The |selector| gives the index of the selected component. This is
/// useful to implement tabs.
/// @param children The list of components.
/// @param selector The index of the drawn children.
/// @ingroup component
/// @see ContainerBase
///
/// ### Example
///
/// ```cpp
/// int tab_drawn = 0;
/// auto container = Container::Tab({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// }, &tab_drawn);
/// ```
Component Tab(Components children, int* selector) {
  return std::make_shared<TabContainer>(std::move(children), selector);
}

/// @brief A list of components to be stacked on top of each other.
/// Events are propagated to the first component, then the second if not
/// handled, etc.
/// The components are drawn in the reverse order they are given.
/// When a component take focus, it is put at the front, without changing the
/// relative order of the other elements.
///
/// This should be used with the `Window` component.
///
/// @param children The list of components.
/// @ingroup component
/// @see Window
///
/// ### Example
///
/// ```cpp
/// auto container = Container::Stacked({
///   children_1,
///   children_2,
///   children_3,
///   children_4,
/// });
/// ```
Component Stacked(Components children) {
  return std::make_shared<StackedContainer>(std::move(children));
}

}  // namespace Container

}  // namespace ftxui

// ---- component\dropdown.cpp --------------------------------------


namespace ftxui {

namespace {
// Whether `ref` points to a variable owned by the caller, as opposed to a value
// stored inside `ref` itself.
bool IsExternal(const Ref<bool>& ref) {
  const std::less<const void*> less;
  const void* address = &*ref;
  return less(address, &ref) || !less(address, &ref + 1);
}
}  // namespace

/// @brief A dropdown menu.
/// @ingroup component
/// @param entries The list of entries to display.
/// @param selected The index of the selected entry.
Component Dropdown(ConstStringListRef entries, int* selected) {
  DropdownOption option;
  option.radiobox.entries = std::move(entries);
  option.radiobox.selected = selected;
  return Dropdown(option);
}

/// @brief A dropdown menu.
/// @ingroup component
/// @param option The options for the dropdown.
// NOLINTNEXTLINE
Component Dropdown(DropdownOption option) {
  class Impl : public ComponentBase, public DropdownOption {
   public:
    explicit Impl(DropdownOption option) : DropdownOption(std::move(option)) {
      FillDefault();
      checkbox_ = Checkbox(checkbox);
      radiobox_ = Radiobox(radiobox);

      container_ = Container::Vertical({
          checkbox_,
          Maybe(radiobox_, checkbox.checked),
      });
      Add(container_);
    }

    Element OnRender() override {
      selected_ =
          util::clamp(radiobox.selected(), 0, int(radiobox.entries.size()) - 1);
      selected_ = util::clamp(selected_(), 0, int(radiobox.entries.size()) - 1);

      if (selected_() >= 0 && selected_() < int(radiobox.entries.size())) {
        title_ = radiobox.entries[selected_()];
      }

      // Close the dropdown when another component takes the focus. This can
      // happen without this dropdown receiving any event, e.g. when the user
      // clicks on a sibling dropdown. Move the inner focus back to the
      // checkbox without stealing the focus from the other component.
      if (open() && !Focused()) {
        container_->SetActiveChild(checkbox_);
        *open = false;
      }

      return transform(*open, checkbox_->Render(), radiobox_->Render());
    }

    // Switch focus in between the checkbox and the radiobox when selecting it.
    bool OnEvent(ftxui::Event event) override {
      const bool open_old = open();
      const int selected_old = selected_();
      bool handled = ComponentBase::OnEvent(event);

      // Transfer focus to the radiobox when the dropdown is opened.
      if (!open_old && open()) {
        radiobox_->TakeFocus();
      }

      // Auto-close the dropdown when the user selects an item, even if the item
      // it the same as the previous one.
      if (open_old && open()) {
        const bool should_close =
            (selected_() != selected_old) ||     //
            (event == Event::Return) ||          //
            (event == Event::Character(' ')) ||  //
            (event == Event::Escape) ||          //
            (event.is_mouse() && event.mouse().button == Mouse::Left &&
             event.mouse().motion == Mouse::Pressed);

        if (should_close) {
          checkbox_->TakeFocus();
          *open = false;
          handled = true;
        }
      }

      return handled;
    }

    void FillDefault() {
      // Both `open` and `checkbox.checked` can hold the open state. Prefer the
      // one bound to an external variable, then the one set to non-default.
      if (!IsExternal(open) &&
          (IsExternal(checkbox.checked) || checkbox.checked())) {
        open = checkbox.checked;
      }
      selected_ = radiobox.selected;
      checkbox.checked = &*open;
      radiobox.selected = &*selected_;
      checkbox.label = &title_;

      if (!checkbox.transform) {
        checkbox.transform = [](const EntryState& s) {
          auto prefix = text(s.state ? "↓ " : "→ ");  // NOLINT
          auto t = text(s.label);
          if (s.active) {
            t |= bold;
          }
          if (s.focused) {
            t |= inverted;
          }
          return hbox({prefix, t});
        };
      }

      if (!transform) {
        transform = [](bool is_open, Element checkbox_element,
                       Element radiobox_element) {
          if (is_open) {
            const int max_height = 12;
            return vbox({
                       std::move(checkbox_element),
                       separator(),
                       std::move(radiobox_element) | vscroll_indicator | frame |
                           size(HEIGHT, LESS_THAN, max_height),
                   }) |
                   border;
          }
          return vbox({std::move(checkbox_element), filler()}) | border;
        };
      }
    }

   private:
    Ref<int> selected_;
    Component container_;
    Component checkbox_;
    Component radiobox_;
    std::string title_;
  };

  return Make<Impl>(option);
}

}  // namespace ftxui

// ---- component\event.cpp -----------------------------------------


// Disable warning for shadowing variable, for every compilers. Indeed, there is
// a static Event for every letter of the alphabet:
#ifdef __clang__
#pragma clang diagnostic ignored "-Wshadow"
#elif __GNUC__
#pragma GCC diagnostic ignored "-Wshadow"
#elif defined(_MSC_VER)
#pragma warning(disable : 6244)
#pragma warning(disable : 6246)
#endif

namespace ftxui {

/// @brief An event corresponding to a given typed character.
/// @param input The character typed by the user.
// static
Event Event::Character(std::string_view input) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::Character;
  return event;
}

/// @brief An event corresponding to a given typed character.
/// @param c The character typed by the user.
// static
Event Event::Character(char c) {
  return Event::Character(std::string{c});
}

/// @brief An event corresponding to a given typed character.
/// @param c The character typed by the user.
// static
Event Event::Character(wchar_t c) {
  return Event::Character(to_string(std::wstring{c}));
}

/// @brief An event corresponding to a given typed character.
/// @param input The sequence of character send by the terminal.
/// @param mouse The mouse state.
// static
Event Event::Mouse(std::string_view input, struct Mouse mouse) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::Mouse;
  event.data_.mouse = mouse;  // NOLINT
  return event;
}

/// @brief An event corresponding to a terminal DCS (Device Control String).
// static
Event Event::CursorShape(std::string_view input, int shape) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::CursorShape;
  event.data_.cursor_shape = shape;  // NOLINT
  return event;
}

/// @brief An event corresponding to a terminal name and version report.
// static
Event Event::TerminalNameVersion(std::string_view input,
                                 std::string name,
                                 int version) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::TerminalNameVersion;
  event.terminal_name_ = std::make_shared<std::string>(std::move(name));
  event.data_.terminal_version = version;
  return event;
}

/// @brief An event corresponding to a terminal emulator report.
// static
Event Event::TerminalEmulator(std::string_view input,
                              std::string name,
                              std::string version) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::TerminalEmulator;
  event.terminal_name_ = std::make_shared<std::string>(std::move(name));
  event.terminal_emulator_version_ =
      std::make_shared<std::string>(std::move(version));
  return event;
}

/// @brief An event corresponding to a terminal capabilities report.
// static
Event Event::TerminalCapabilities(std::string_view input,
                                  std::vector<int> capabilities) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::TerminalCapabilities;
  event.terminal_capabilities_ =
      std::make_shared<std::vector<int>>(std::move(capabilities));
  return event;
}

/// @brief Return the names of the terminal capabilities.
std::vector<std::string> Event::TerminalCapabilityNames() const {
  if (type_ != Type::TerminalCapabilities) {
    return {};
  }

  std::vector<std::string> names;
  for (const int cap : *terminal_capabilities_) {
    switch (cap) {
      case 0:
        break;
      case 1:
        names.emplace_back("132-columns");
        break;
      case 2:
        names.emplace_back("Printer-port");
        break;
      case 3:
        names.emplace_back("ReGIS-graphics");
        break;
      case 4:
        names.emplace_back("Sixel-graphics");
        break;
      case 6:
        names.emplace_back("Selective-erase");
        break;
      case 7:
        names.emplace_back("Soft-character-set-(DRCS)");
        break;
      case 8:
        names.emplace_back("User-defined-keys-(UDK)");
        break;
      case 9:
        names.emplace_back("National-replacement-character-sets-(NRC)");
        break;
      case 12:
        names.emplace_back("Local-editing");
        break;
      case 15:
        names.emplace_back("Technical-character-set");
        break;
      case 18:
        names.emplace_back("Windowing-capability");
        break;
      case 21:
        names.emplace_back("Horizontal-scrolling");
        break;
      case 22:
        names.emplace_back("ANSI-color");
        break;
      case 28:
        names.emplace_back("Font-loading");
        break;
      case 29:
        names.emplace_back("ANSI-text-locator-(Mouse)");
        break;
      case 52:
        names.emplace_back("UTF-8");
        break;
      case 61:
        names.emplace_back("VT510");
        break;
      case 62:
        names.emplace_back("VT220");
        break;
      case 63:
        names.emplace_back("VT320");
        break;
      case 64:
        names.emplace_back("VT420");
        break;
      case 65:
        names.emplace_back("VT525");
        break;
      default:
        names.emplace_back("Unknown-" + std::to_string(cap));
        break;
    }
  }
  return names;
}

/// @brief Whether the event is a terminal name and version report.
bool Event::IsTerminalNameVersion() const {
  return type_ == Type::TerminalNameVersion;
}

/// @brief Return the terminal name.
const std::string& Event::TerminalName() const {
  return *terminal_name_;
}

/// @brief Return the terminal version.
int Event::TerminalVersion() const {
  return data_.terminal_version;
}

/// @brief Whether the event is a terminal capabilities report.
bool Event::IsTerminalCapabilities() const {
  return type_ == Type::TerminalCapabilities;
}

/// @brief Return the terminal capabilities.
const std::vector<int>& Event::TerminalCapabilities() const {
  return *terminal_capabilities_;
}

/// @brief Whether the event is a terminal emulator report.
bool Event::IsTerminalEmulator() const {
  return type_ == Type::TerminalEmulator;
}

/// @brief Return the terminal emulator name.
const std::string& Event::TerminalEmulatorName() const {
  return *terminal_name_;
}

/// @brief Return the terminal emulator version.
const std::string& Event::TerminalEmulatorVersion() const {
  return *terminal_emulator_version_;
}

/// @brief An custom event whose meaning is defined by the user of the library.
/// @param input An arbitrary sequence of character defined by the developer.
// static
Event Event::Special(std::string_view input) {
  Event event;
  event.input_ = std::string(input);
  return event;
}

/// @brief An custom event whose meaning is defined by the user of the library.
/// @param input An arbitrary sequence of character defined by the developer.
// static
Event Event::Special(std::initializer_list<char> input) {
  return Event::Special(std::string(input));
}

/// @internal
// static
Event Event::CursorPosition(std::string_view input, int x, int y) {
  Event event;
  event.input_ = std::string(input);
  event.type_ = Type::CursorPosition;
  event.data_.cursor = {x, y};  // NOLINT
  return event;
}

/// @brief Return a string representation of the event.
std::string Event::DebugString() const {
  static std::map<Event, const char*> event_to_string = {
      // --- Arrow ---
      {Event::ArrowLeft, "Event::ArrowLeft"},
      {Event::ArrowRight, "Event::ArrowRight"},
      {Event::ArrowUp, "Event::ArrowUp"},
      {Event::ArrowDown, "Event::ArrowDown"},

      // --- ArrowCtrl ---
      {Event::ArrowLeftCtrl, "Event::ArrowLeftCtrl"},
      {Event::ArrowRightCtrl, "Event::ArrowRightCtrl"},
      {Event::ArrowUpCtrl, "Event::ArrowUpCtrl"},
      {Event::ArrowDownCtrl, "Event::ArrowDownCtrl"},

      // --- ArrowAlt ---
      {Event::ArrowLeftAlt, "Event::ArrowLeftAlt"},
      {Event::ArrowRightAlt, "Event::ArrowRightAlt"},
      {Event::ArrowUpAlt, "Event::ArrowUpAlt"},
      {Event::ArrowDownAlt, "Event::ArrowDownAlt"},

      // --- Other ---
      {Event::Backspace, "Event::Backspace"},
      {Event::BackspaceAlt, "Event::BackspaceAlt"},
      {Event::Delete, "Event::Delete"},
      {Event::DeleteAlt, "Event::DeleteAlt"},
      {Event::DeleteCtrl, "Event::DeleteCtrl"},
      {Event::Escape, "Event::Escape"},
      {Event::Return, "Event::Return"},
      {Event::Tab, "Event::Tab"},
      {Event::TabReverse, "Event::TabReverse"},

      // --- Function keys ---
      {Event::F1, "Event::F1"},
      {Event::F2, "Event::F2"},
      {Event::F3, "Event::F3"},
      {Event::F4, "Event::F4"},
      {Event::F5, "Event::F5"},
      {Event::F6, "Event::F6"},
      {Event::F7, "Event::F7"},
      {Event::F8, "Event::F8"},
      {Event::F9, "Event::F9"},
      {Event::F10, "Event::F10"},
      {Event::F11, "Event::F11"},
      {Event::F12, "Event::F12"},

      // --- Navigation keys ---
      {Event::Insert, "Event::Insert"},
      {Event::Home, "Event::Home"},
      {Event::End, "Event::End"},
      {Event::PageUp, "Event::PageUp"},
      {Event::PageDown, "Event::PageDown"},

      // --- Control keys ---
      {Event::CtrlA, "Event::CtrlA"},
      {Event::CtrlB, "Event::CtrlB"},
      {Event::CtrlC, "Event::CtrlC"},
      {Event::CtrlD, "Event::CtrlD"},
      {Event::CtrlE, "Event::CtrlE"},
      {Event::CtrlF, "Event::CtrlF"},
      {Event::CtrlG, "Event::CtrlG"},
      {Event::CtrlH, "Event::CtrlH"},
      {Event::CtrlI, "Event::CtrlI"},
      {Event::CtrlJ, "Event::CtrlJ"},
      {Event::CtrlK, "Event::CtrlK"},
      {Event::CtrlL, "Event::CtrlL"},
      {Event::CtrlM, "Event::CtrlM"},
      {Event::CtrlN, "Event::CtrlN"},
      {Event::CtrlO, "Event::CtrlO"},
      {Event::CtrlP, "Event::CtrlP"},
      {Event::CtrlQ, "Event::CtrlQ"},
      {Event::CtrlR, "Event::CtrlR"},
      {Event::CtrlS, "Event::CtrlS"},
      {Event::CtrlT, "Event::CtrlT"},
      {Event::CtrlU, "Event::CtrlU"},
      {Event::CtrlV, "Event::CtrlV"},
      {Event::CtrlW, "Event::CtrlW"},
      {Event::CtrlX, "Event::CtrlX"},
      {Event::CtrlY, "Event::CtrlY"},
      {Event::CtrlZ, "Event::CtrlZ"},

      // --- Alt keys ---
      {Event::AltA, "Event::AltA"},
      {Event::AltB, "Event::AltB"},
      {Event::AltC, "Event::AltC"},
      {Event::AltD, "Event::AltD"},
      {Event::AltE, "Event::AltE"},
      {Event::AltF, "Event::AltF"},
      {Event::AltG, "Event::AltG"},
      {Event::AltH, "Event::AltH"},
      {Event::AltI, "Event::AltI"},
      {Event::AltJ, "Event::AltJ"},
      {Event::AltK, "Event::AltK"},
      {Event::AltL, "Event::AltL"},
      {Event::AltM, "Event::AltM"},
      {Event::AltN, "Event::AltN"},
      {Event::AltO, "Event::AltO"},
      {Event::AltP, "Event::AltP"},
      {Event::AltQ, "Event::AltQ"},
      {Event::AltR, "Event::AltR"},
      {Event::AltS, "Event::AltS"},
      {Event::AltT, "Event::AltT"},
      {Event::AltU, "Event::AltU"},
      {Event::AltV, "Event::AltV"},
      {Event::AltW, "Event::AltW"},
      {Event::AltX, "Event::AltX"},
      {Event::AltY, "Event::AltY"},
      {Event::AltZ, "Event::AltZ"},

      // --- CtrlAlt keys ---
      {Event::CtrlAltA, "Event::CtrlAltA"},
      {Event::CtrlAltB, "Event::CtrlAltB"},
      {Event::CtrlAltC, "Event::CtrlAltC"},
      {Event::CtrlAltD, "Event::CtrlAltD"},
      {Event::CtrlAltE, "Event::CtrlAltE"},
      {Event::CtrlAltF, "Event::CtrlAltF"},
      {Event::CtrlAltG, "Event::CtrlAltG"},
      {Event::CtrlAltH, "Event::CtrlAltH"},
      {Event::CtrlAltI, "Event::CtrlAltI"},
      {Event::CtrlAltJ, "Event::CtrlAltJ"},
      {Event::CtrlAltK, "Event::CtrlAltK"},
      {Event::CtrlAltL, "Event::CtrlAltL"},
      {Event::CtrlAltM, "Event::CtrlAltM"},
      {Event::CtrlAltN, "Event::CtrlAltN"},
      {Event::CtrlAltO, "Event::CtrlAltO"},
      {Event::CtrlAltP, "Event::CtrlAltP"},
      {Event::CtrlAltQ, "Event::CtrlAltQ"},
      {Event::CtrlAltR, "Event::CtrlAltR"},
      {Event::CtrlAltS, "Event::CtrlAltS"},
      {Event::CtrlAltT, "Event::CtrlAltT"},
      {Event::CtrlAltU, "Event::CtrlAltU"},
      {Event::CtrlAltV, "Event::CtrlAltV"},
      {Event::CtrlAltW, "Event::CtrlAltW"},
      {Event::CtrlAltX, "Event::CtrlAltX"},
      {Event::CtrlAltY, "Event::CtrlAltY"},
      {Event::CtrlAltZ, "Event::CtrlAltZ"},

      // --- Custom ---
      {Event::Custom, "Event::Custom"},
  };

  static std::map<Mouse::Button, const char*> mouse_button_string = {
      {Mouse::Button::Left, ".button = Mouse::Left"},
      {Mouse::Button::Middle, ".button = Mouse::Middle"},
      {Mouse::Button::Right, ".button = Mouse::Right"},
      {Mouse::Button::WheelUp, ".button = Mouse::WheelUp"},
      {Mouse::Button::WheelDown, ".button = Mouse::WheelDown"},
      {Mouse::Button::None, ".button = Mouse::None"},
      {Mouse::Button::WheelLeft, ".button = Mouse::WheelLeft"},
      {Mouse::Button::WheelRight, ".button = Mouse::WheelRight"},
  };

  static std::map<Mouse::Motion, const char*> mouse_motion_string = {
      {Mouse::Motion::Pressed, ".motion = Mouse::Pressed"},
      {Mouse::Motion::Released, ".motion = Mouse::Released"},
      {Mouse::Motion::Moved, ".motion = Mouse::Moved"},
  };

  switch (type_) {
    case Type::Character: {
      return "Event::Character(\"" + input_ + "\")";
    }
    case Type::Mouse: {
      std::string out = "Event::Mouse(\"...\", Mouse{";
      out += std::string(mouse_button_string[data_.mouse.button]);
      out += ", ";
      out += std::string(mouse_motion_string[data_.mouse.motion]);
      out += ", ";
      if (data_.mouse.shift) {
        out += ".shift = true, ";
      }
      if (data_.mouse.meta) {
        out += ".meta = true, ";
      }
      if (data_.mouse.control) {
        out += ".control = true, ";
      }
      out += ".x = " + std::to_string(data_.mouse.x);
      out += ", ";
      out += ".y = " + std::to_string(data_.mouse.y);
      out += "})";
      return out;
    }
    case Type::CursorShape:
      return "Event::CursorShape(" + input_ + ", " +
             std::to_string(data_.cursor_shape) + ")";
    case Type::CursorPosition:
      return "Event::CursorPosition(" + input_ + ", " +
             std::to_string(data_.cursor.x) + ", " +
             std::to_string(data_.cursor.y) + ")";
    case Type::TerminalNameVersion:
      return "Event::TerminalNameVersion(" + input_ + ", " + *terminal_name_ +
             ", " + std::to_string(data_.terminal_version) + ")";
    case Type::TerminalEmulator:
      return "Event::TerminalEmulator(" + input_ + ", " + *terminal_name_ +
             ", " + *terminal_emulator_version_ + ")";
    case Type::TerminalCapabilities: {
      std::string out = "Event::TerminalCapabilities(" + input_ + ", {";
      for (size_t i = 0; i < terminal_capabilities_->size(); ++i) {
        out += std::to_string((*terminal_capabilities_)[i]);
        if (i + 1 < terminal_capabilities_->size()) {
          out += ", ";
        }
      }
      out += "})";
      return out;
    }
    default: {
      auto event_it = event_to_string.find(*this);
      if (event_it != event_to_string.end()) {
        return event_it->second;
      }

      return "";
    }
  }
  return "";
}

// clang-format off
// NOLINTBEGIN

// --- Arrow ---
const Event Event::ArrowLeft      = Event::Special("\x1B[D");
const Event Event::ArrowRight     = Event::Special("\x1B[C");
const Event Event::ArrowUp        = Event::Special("\x1B[A");
const Event Event::ArrowDown      = Event::Special("\x1B[B");
const Event Event::ArrowLeftCtrl  = Event::Special("\x1B[1;5D");
const Event Event::ArrowRightCtrl = Event::Special("\x1B[1;5C");
const Event Event::ArrowUpCtrl    = Event::Special("\x1B[1;5A");
const Event Event::ArrowDownCtrl  = Event::Special("\x1B[1;5B");
const Event Event::ArrowLeftAlt   = Event::Special("\x1B[1;3D");
const Event Event::ArrowRightAlt  = Event::Special("\x1B[1;3C");
const Event Event::ArrowUpAlt     = Event::Special("\x1B[1;3A");
const Event Event::ArrowDownAlt   = Event::Special("\x1B[1;3B");
const Event Event::Backspace      = Event::Special({127});
const Event Event::BackspaceAlt   = Event::Special({27, 127});
const Event Event::AltBackspace   = Event::Special({27, 127});
const Event Event::Delete         = Event::Special("\x1B[3~");
const Event Event::DeleteAlt      = Event::Special("\x1B[3;3~");
const Event Event::DeleteCtrl     = Event::Special("\x1B[3;5~");
const Event Event::Escape         = Event::Special("\x1B");
const Event Event::Return         = Event::Special({10});
const Event Event::Tab            = Event::Special({9});
const Event Event::TabReverse     = Event::Special({27, 91, 90});

// See https://invisible-island.net/xterm/xterm-function-keys.html
// We follow xterm-new / vterm-xf86-v4 / mgt / screen
const Event Event::F1  = Event::Special("\x1BOP");
const Event Event::F2  = Event::Special("\x1BOQ");
const Event Event::F3  = Event::Special("\x1BOR");
const Event Event::F4  = Event::Special("\x1BOS");
const Event Event::F5  = Event::Special("\x1B[15~");
const Event Event::F6  = Event::Special("\x1B[17~");
const Event Event::F7  = Event::Special("\x1B[18~");
const Event Event::F8  = Event::Special("\x1B[19~");
const Event Event::F9  = Event::Special("\x1B[20~");
const Event Event::F10 = Event::Special("\x1B[21~");
const Event Event::F11 = Event::Special("\x1B[23~");
const Event Event::F12 = Event::Special("\x1B[24~");

const Event Event::Insert   = Event::Special("\x1B[2~");
const Event Event::Home     = Event::Special({27, 91, 72});
const Event Event::End      = Event::Special({27, 91, 70});
const Event Event::PageUp   = Event::Special({27, 91, 53, 126});
const Event Event::PageDown = Event::Special({27, 91, 54, 126});
const Event Event::Custom   = Event::Special({0});

const Event Event::a = Event::Character("a");
const Event Event::b = Event::Character("b");
const Event Event::c = Event::Character("c");
const Event Event::d = Event::Character("d");
const Event Event::e = Event::Character("e");
const Event Event::f = Event::Character("f");
const Event Event::g = Event::Character("g");
const Event Event::h = Event::Character("h");
const Event Event::i = Event::Character("i");
const Event Event::j = Event::Character("j");
const Event Event::k = Event::Character("k");
const Event Event::l = Event::Character("l");
const Event Event::m = Event::Character("m");
const Event Event::n = Event::Character("n");
const Event Event::o = Event::Character("o");
const Event Event::p = Event::Character("p");
const Event Event::q = Event::Character("q");
const Event Event::r = Event::Character("r");
const Event Event::s = Event::Character("s");
const Event Event::t = Event::Character("t");
const Event Event::u = Event::Character("u");
const Event Event::v = Event::Character("v");
const Event Event::w = Event::Character("w");
const Event Event::x = Event::Character("x");
const Event Event::y = Event::Character("y");
const Event Event::z = Event::Character("z");

const Event Event::A = Event::Character("A");
const Event Event::B = Event::Character("B");
const Event Event::C = Event::Character("C");
const Event Event::D = Event::Character("D");
const Event Event::E = Event::Character("E");
const Event Event::F = Event::Character("F");
const Event Event::G = Event::Character("G");
const Event Event::H = Event::Character("H");
const Event Event::I = Event::Character("I");
const Event Event::J = Event::Character("J");
const Event Event::K = Event::Character("K");
const Event Event::L = Event::Character("L");
const Event Event::M = Event::Character("M");
const Event Event::N = Event::Character("N");
const Event Event::O = Event::Character("O");
const Event Event::P = Event::Character("P");
const Event Event::Q = Event::Character("Q");
const Event Event::R = Event::Character("R");
const Event Event::S = Event::Character("S");
const Event Event::T = Event::Character("T");
const Event Event::U = Event::Character("U");
const Event Event::V = Event::Character("V");
const Event Event::W = Event::Character("W");
const Event Event::X = Event::Character("X");
const Event Event::Y = Event::Character("Y");
const Event Event::Z = Event::Character("Z");

const Event Event::CtrlA = Event::Special("\x01");
const Event Event::CtrlB = Event::Special("\x02");
const Event Event::CtrlC = Event::Special("\x03");
const Event Event::CtrlD = Event::Special("\x04");
const Event Event::CtrlE = Event::Special("\x05");
const Event Event::CtrlF = Event::Special("\x06");
const Event Event::CtrlG = Event::Special("\x07");
const Event Event::CtrlH = Event::Special("\x08");
const Event Event::CtrlI = Event::Special("\x09");
const Event Event::CtrlJ = Event::Special("\x0a");
const Event Event::CtrlK = Event::Special("\x0b");
const Event Event::CtrlL = Event::Special("\x0c");
const Event Event::CtrlM = Event::Special("\x0d");
const Event Event::CtrlN = Event::Special("\x0e");
const Event Event::CtrlO = Event::Special("\x0f");
const Event Event::CtrlP = Event::Special("\x10");
const Event Event::CtrlQ = Event::Special("\x11");
const Event Event::CtrlR = Event::Special("\x12");
const Event Event::CtrlS = Event::Special("\x13");
const Event Event::CtrlT = Event::Special("\x14");
const Event Event::CtrlU = Event::Special("\x15");
const Event Event::CtrlV = Event::Special("\x16");
const Event Event::CtrlW = Event::Special("\x17");
const Event Event::CtrlX = Event::Special("\x18");
const Event Event::CtrlY = Event::Special("\x19");
const Event Event::CtrlZ = Event::Special("\x1a");

const Event Event::AltA = Event::Special("\x1b""a");
const Event Event::AltB = Event::Special("\x1b""b");
const Event Event::AltC = Event::Special("\x1b""c");
const Event Event::AltD = Event::Special("\x1b""d");
const Event Event::AltE = Event::Special("\x1b""e");
const Event Event::AltF = Event::Special("\x1b""f");
const Event Event::AltG = Event::Special("\x1b""g");
const Event Event::AltH = Event::Special("\x1b""h");
const Event Event::AltI = Event::Special("\x1b""i");
const Event Event::AltJ = Event::Special("\x1b""j");
const Event Event::AltK = Event::Special("\x1b""k");
const Event Event::AltL = Event::Special("\x1b""l");
const Event Event::AltM = Event::Special("\x1b""m");
const Event Event::AltN = Event::Special("\x1b""n");
const Event Event::AltO = Event::Special("\x1b""o");
const Event Event::AltP = Event::Special("\x1b""p");
const Event Event::AltQ = Event::Special("\x1b""q");
const Event Event::AltR = Event::Special("\x1b""r");
const Event Event::AltS = Event::Special("\x1b""s");
const Event Event::AltT = Event::Special("\x1b""t");
const Event Event::AltU = Event::Special("\x1b""u");
const Event Event::AltV = Event::Special("\x1b""v");
const Event Event::AltW = Event::Special("\x1b""w");
const Event Event::AltX = Event::Special("\x1b""x");
const Event Event::AltY = Event::Special("\x1b""y");
const Event Event::AltZ = Event::Special("\x1b""z");

const Event Event::CtrlAltA = Event::Special("\x1b\x01");
const Event Event::CtrlAltB = Event::Special("\x1b\x02");
const Event Event::CtrlAltC = Event::Special("\x1b\x03");
const Event Event::CtrlAltD = Event::Special("\x1b\x04");
const Event Event::CtrlAltE = Event::Special("\x1b\x05");
const Event Event::CtrlAltF = Event::Special("\x1b\x06");
const Event Event::CtrlAltG = Event::Special("\x1b\x07");
const Event Event::CtrlAltH = Event::Special("\x1b\x08");
const Event Event::CtrlAltI = Event::Special("\x1b\x09");
const Event Event::CtrlAltJ = Event::Special("\x1b\x0a");
const Event Event::CtrlAltK = Event::Special("\x1b\x0b");
const Event Event::CtrlAltL = Event::Special("\x1b\x0c");
const Event Event::CtrlAltM = Event::Special("\x1b\x0d");
const Event Event::CtrlAltN = Event::Special("\x1b\x0e");
const Event Event::CtrlAltO = Event::Special("\x1b\x0f");
const Event Event::CtrlAltP = Event::Special("\x1b\x10");
const Event Event::CtrlAltQ = Event::Special("\x1b\x11");
const Event Event::CtrlAltR = Event::Special("\x1b\x12");
const Event Event::CtrlAltS = Event::Special("\x1b\x13");
const Event Event::CtrlAltT = Event::Special("\x1b\x14");
const Event Event::CtrlAltU = Event::Special("\x1b\x15");
const Event Event::CtrlAltV = Event::Special("\x1b\x16");
const Event Event::CtrlAltW = Event::Special("\x1b\x17");
const Event Event::CtrlAltX = Event::Special("\x1b\x18");
const Event Event::CtrlAltY = Event::Special("\x1b\x19");
const Event Event::CtrlAltZ = Event::Special("\x1b\x1a");

// NOLINTEND
// clang-format on

}  // namespace ftxui

// ---- component\hoverable.cpp -------------------------------------


namespace ftxui {

namespace {}  // namespace

/// @brief Wrap a component. Gives the ability to know if it is hovered by the
/// mouse.
/// @param component The wrapped component.
/// @param hover The value to reflect whether the component is hovered or not.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto button = Button("exit", screen.ExitLoopClosure());
/// bool hover = false;
/// auto button_hover = Hoverable(button, &hover);
/// ```
// NOLINTNEXTLINE
Component Hoverable(Component component, bool* hover) {
  class Impl : public ComponentBase {
   public:
    Impl(Component component, bool* hover)
        : component_(std::move(component)), hover_(hover) {
      Add(component_);
    }

   private:
    Element OnRender() override {
      return ComponentBase::OnRender() | reflect(box_);
    }

    bool OnEvent(Event event) override {
      if (event.is_mouse()) {
        *hover_ = box_.Contain(event.mouse().x, event.mouse().y) &&
                  CaptureMouse(event);
      }

      return ComponentBase::OnEvent(event);
    }

    Component component_;
    bool* hover_;
    Box box_;
  };

  return Make<Impl>(component, hover);
}

/// @brief Wrap a component. Uses callbacks.
/// @param component The wrapped component.
/// @param on_enter Callback OnEnter
/// @param on_leave Callback OnLeave
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto button = Button("exit", screen.ExitLoopClosure());
/// bool hover = false;
/// auto button_hover = Hoverable(button, &hover);
/// ```
Component Hoverable(Component component,
                    std::function<void()> on_enter,
                    std::function<void()> on_leave) {
  class Impl : public ComponentBase {
   public:
    Impl(Component component,
         std::function<void()> on_enter,
         std::function<void()> on_leave)
        : component_(std::move(component)),
          on_enter_(std::move(on_enter)),
          on_leave_(std::move(on_leave)) {
      Add(component_);
    }

   private:
    Element OnRender() override {
      return ComponentBase::OnRender() | reflect(box_);
    }

    bool OnEvent(Event event) override {
      if (event.is_mouse()) {
        const bool hover = box_.Contain(event.mouse().x, event.mouse().y) &&
                           CaptureMouse(event);
        if (hover != hover_) {
          App::PostEventOrExecute(hover ? on_enter_ : on_leave_);
        }
        hover_ = hover;
      }

      return ComponentBase::OnEvent(event);
    }

    Component component_;
    Box box_;
    bool hover_ = false;
    std::function<void()> on_enter_;
    std::function<void()> on_leave_;
  };

  return Make<Impl>(std::move(component), std::move(on_enter),
                    std::move(on_leave));
}

/// @brief Wrap a component. Gives the ability to know if it is hovered by the
/// mouse.
/// @param hover The value to reflect whether the component is hovered or not.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// bool hover = false;
/// auto button = Button("exit", screen.ExitLoopClosure());
/// button |= Hoverable(&hover);
/// ```
ComponentDecorator Hoverable(bool* hover) {
  return [hover](Component component) {
    return Hoverable(std::move(component), hover);
  };
}

/// @brief Wrap a component. Gives the ability to know if it is hovered by the
/// mouse.
/// @param on_enter is called when the mouse hover the component.
/// @param on_leave is called when the mouse leave the component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto button = Button("exit", screen.ExitLoopClosure());
/// int on_enter_cnt = 0;
/// int on_leave_cnt = 0;
/// button |= Hoverable(
///   [&]{ on_enter_cnt++; },
///   [&]{ on_leave_cnt++; }
/// );
/// ```
// NOLINTNEXTLINE
ComponentDecorator Hoverable(std::function<void()> on_enter,
                             // NOLINTNEXTLINE
                             std::function<void()> on_leave) {
  return [on_enter, on_leave](Component component) {
    return Hoverable(std::move(component), on_enter, on_leave);
  };
}

/// @brief Wrap a component. Gives the ability to know if it is hovered by the
/// mouse.
/// @param component the wrapped component.
/// @param on_change is called when the mouse enter or leave the component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto button = Button("exit", screen.ExitLoopClosure());
/// bool hovered = false;
/// auto button_hoverable = Hoverable(button,
///                                   [&](bool hover) { hovered = hover;});
/// ```
// NOLINTNEXTLINE
Component Hoverable(Component component, std::function<void(bool)> on_change) {
  return Hoverable(
      std::move(component),              //
      [on_change] { on_change(true); },  //
      [on_change] { on_change(false); }  //
  );
}

/// @brief Wrap a component. Gives the ability to know if it is hovered by the
/// mouse.
/// @param on_change is called when the mouse enter or leave the component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto button = Button("exit", screen.ExitLoopClosure());
/// bool hovered = false;
/// button |= Hoverable([&](bool hover) { hovered = hover;});
/// ```
// NOLINTNEXTLINE
ComponentDecorator Hoverable(std::function<void(bool)> on_change) {
  return [on_change](Component component) {
    return Hoverable(std::move(component), on_change);
  };
}

}  // namespace ftxui

// ---- component\input.cpp -----------------------------------------




namespace ftxui {

// Internal functions exported for the 'component' library.
FTXUI_EXPORT(SCREEN)
bool EatCodePoint(std::string_view input,
                  size_t start,
                  size_t* end,
                  uint32_t* ucs);
FTXUI_EXPORT(SCREEN)
bool EatCodePoint(std::wstring_view input,
                  size_t start,
                  size_t* end,
                  uint32_t* ucs);

FTXUI_EXPORT(SCREEN) bool IsCombining(uint32_t ucs);
FTXUI_EXPORT(SCREEN) bool IsFullWidth(uint32_t ucs);
FTXUI_EXPORT(SCREEN) bool IsControl(uint32_t ucs);

FTXUI_EXPORT(SCREEN) size_t GlyphPrevious(std::string_view input, size_t start);
FTXUI_EXPORT(SCREEN) size_t GlyphNext(std::string_view input, size_t start);

// Return the index in the |input| string of the glyph at |glyph_offset|,
// starting at |start|
FTXUI_EXPORT(SCREEN)
size_t GlyphIterate(std::string_view input, int glyph_offset, size_t start = 0);

// Returns the number of glyphs in |input|.
FTXUI_EXPORT(SCREEN) int GlyphCount(std::string_view input);

// Properties from:
// https://www.unicode.org/Public/UCD/latest/ucd/auxiliary/WordBreakProperty.txt
enum class WordBreakProperty : int8_t {
  ALetter,
  CR,
  Double_Quote,
  Extend,
  ExtendNumLet,
  Format,
  Hebrew_Letter,
  Katakana,
  LF,
  MidLetter,
  MidNum,
  MidNumLet,
  Newline,
  Numeric,
  Regional_Indicator,
  Single_Quote,
  WSegSpace,
  ZWJ,
};
FTXUI_EXPORT(SCREEN)
WordBreakProperty CodepointToWordBreakProperty(uint32_t codepoint);
FTXUI_EXPORT(SCREEN)
std::vector<WordBreakProperty> Utf8ToWordBreakProperty(std::string_view input);

FTXUI_EXPORT(SCREEN)
bool IsWordBreakingCharacter(std::string_view input, size_t glyph_index);
}  // namespace ftxui


namespace ftxui {

namespace {

std::vector<std::string> SplitLines(std::string_view input) {
  std::vector<std::string> output;
  size_t start = 0;
  size_t end = input.find('\n');
  while (end != std::string_view::npos) {
    output.push_back(std::string(input.substr(start, end - start)));
    start = end + 1;
    end = input.find('\n', start);
  }
  output.push_back(std::string(input.substr(start)));
  return output;
}

size_t GlyphWidth(std::string_view input, size_t iter) {
  uint32_t ucs = 0;
  if (!EatCodePoint(input, iter, &iter, &ucs)) {
    return 0;
  }
  if (IsFullWidth(ucs)) {
    return 2;
  }
  return 1;
}

bool IsWordCodePoint(uint32_t codepoint) {
  switch (CodepointToWordBreakProperty(codepoint)) {
    case WordBreakProperty::ALetter:
    case WordBreakProperty::Hebrew_Letter:
    case WordBreakProperty::Katakana:
    case WordBreakProperty::Numeric:
      return true;

    case WordBreakProperty::CR:
    case WordBreakProperty::Double_Quote:
    case WordBreakProperty::LF:
    case WordBreakProperty::MidLetter:
    case WordBreakProperty::MidNum:
    case WordBreakProperty::MidNumLet:
    case WordBreakProperty::Newline:
    case WordBreakProperty::Single_Quote:
    case WordBreakProperty::WSegSpace:
    // Unexpected/Unsure
    case WordBreakProperty::Extend:
    case WordBreakProperty::ExtendNumLet:
    case WordBreakProperty::Format:
    case WordBreakProperty::Regional_Indicator:
    case WordBreakProperty::ZWJ:
      return false;
  }
  return false;  // NOT_REACHED();
}

bool IsWordCharacter(std::string_view input, size_t iter) {
  uint32_t ucs = 0;
  if (!EatCodePoint(input, iter, &iter, &ucs)) {
    return false;
  }

  return IsWordCodePoint(ucs);
}

// An input box. The user can type text into it.
class InputBase : public ComponentBase, public InputOption {
 public:
  // NOLINTNEXTLINE
  InputBase(InputOption option) : InputOption(std::move(option)) {}

 private:
  // Component implementation:
  Element OnRender() override {
    const bool is_focused = Focused();
    const auto focused = (!is_focused && !hovered_) ? focus
                         : insert()                 ? focusCursorBarBlinking
                                                    : focusCursorBlockBlinking;

    auto transform_func =
        transform ? transform : InputOption::Default().transform;

    // placeholder.
    if (content->empty()) {
      auto element = text(placeholder()) | focused | xflex | frame;

      return transform_func({
                 std::move(element), hovered_, is_focused,
                 true  // placeholder
             }) |
             reflect(box_);
    }

    Elements elements;
    const std::vector<std::string> lines = SplitLines(*content);

    cursor_position() = util::clamp(cursor_position(), 0, (int)content->size());

    // Find the line and index of the cursor.
    int cursor_line = 0;
    int cursor_char_index = cursor_position();
    for (const auto& line : lines) {
      if (cursor_char_index <= (int)line.size()) {
        break;
      }

      cursor_char_index -= static_cast<int>(line.size() + 1);
      cursor_line++;
    }

    if (lines.empty()) {
      elements.push_back(text("") | focused);
    }

    elements.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
      const std::string& line = lines[i];

      // This is not the cursor line.
      if (int(i) != cursor_line) {
        elements.push_back(Text(line));
        continue;
      }

      // The cursor is at the end of the line.
      const std::string cursor_cell = is_focused ? " " : "";
      if (cursor_char_index >= (int)line.size()) {
        elements.push_back(
            hbox({
                Text(line),
                text(cursor_cell) | focused | reflect(cursor_box_),
            }) |
            xflex);
        continue;
      }

      // The cursor is on this line.
      const int glyph_start = cursor_char_index;
      const int glyph_end = static_cast<int>(GlyphNext(line, glyph_start));
      const std::string part_before_cursor = line.substr(0, glyph_start);
      const std::string part_at_cursor =
          line.substr(glyph_start, glyph_end - glyph_start);
      const std::string part_after_cursor = line.substr(glyph_end);
      auto element = hbox({
                         Text(part_before_cursor),
                         Text(part_at_cursor) | focused | reflect(cursor_box_),
                         Text(part_after_cursor),
                     }) |
                     xflex;
      elements.push_back(element);
    }

    auto element = vbox(std::move(elements)) | frame;
    return transform_func({
               std::move(element), hovered_, is_focused,
               false  // placeholder
           }) |
           xflex | reflect(box_);
  }

  Element Text(const std::string& input) {
    if (!password()) {
      return text(input);
    }

    const size_t glyph_count = GlyphCount(input);
    std::string out;
    out.reserve(glyph_count * 3);
    for (size_t i = 0; i < glyph_count; ++i) {
      out += "•";
    }
    return text(out);
  }

  bool HandleBackspace() {
    if (cursor_position() == 0) {
      return false;
    }
    const size_t start = GlyphPrevious(content(), cursor_position());
    const size_t end = cursor_position();
    content->erase(start, end - start);
    cursor_position() = static_cast<int>(start);
    App::PostEventOrExecute(on_change);
    return true;
  }

  bool HandleBackspaceWord() {
    if (cursor_position() == 0) {
      return false;
    }

    size_t start = cursor_position();
    const size_t previous = GlyphPrevious(content(), start);
    if (content()[previous] == '\n') {
      start = previous;
    } else {
      // Phase 1: Skip non-word characters going backward (stopping at '\n').
      while (start > 0) {
        const size_t prev = GlyphPrevious(content(), start);
        if (content()[prev] == '\n' || IsWordCharacter(content(), prev)) {
          break;
        }
        start = prev;
      }

      // Phase 2: Skip word characters going backward (stopping at '\n').
      while (start > 0) {
        const size_t prev = GlyphPrevious(content(), start);
        if (content()[prev] == '\n' || !IsWordCharacter(content(), prev)) {
          break;
        }
        start = prev;
      }
    }

    const size_t end = cursor_position();
    content->erase(start, end - start);
    cursor_position() = static_cast<int>(start);
    App::PostEventOrExecute(on_change);
    return true;
  }

  bool HandleDeleteWord() {
    if (cursor_position() == (int)content->size()) {
      return false;
    }

    size_t end = cursor_position();
    if (content()[end] == '\n') {
      end++;
    } else {
      // Phase 1: Skip non-word characters going forward (stopping at '\n').
      while (end < content->size() && content()[end] != '\n' &&
             !IsWordCharacter(content(), end)) {
        end = GlyphNext(content(), end);
      }

      // Phase 2: Skip word characters going forward (stopping at '\n').
      while (end < content->size() && content()[end] != '\n' &&
             IsWordCharacter(content(), end)) {
        end = GlyphNext(content(), end);
      }
    }

    const size_t start = cursor_position();
    content->erase(start, end - start);
    App::PostEventOrExecute(on_change);
    return true;
  }

  // Delete from the beginning of the line to the cursor.
  bool HandleBackspaceLine() {
    if (cursor_position() == 0) {
      return false;
    }

    size_t start = cursor_position();
    const size_t previous = GlyphPrevious(content(), start);
    if (content()[previous] == '\n') {
      start = previous;
    } else {
      while (start > 0) {
        const size_t prev = GlyphPrevious(content(), start);
        if (content()[prev] == '\n') {
          break;
        }
        start = prev;
      }
    }

    const size_t end = cursor_position();
    content->erase(start, end - start);
    cursor_position() = static_cast<int>(start);
    App::PostEventOrExecute(on_change);
    return true;
  }

  bool DeleteImpl() {
    if (cursor_position() == (int)content->size()) {
      return false;
    }
    const size_t start = cursor_position();
    const size_t end = GlyphNext(content(), cursor_position());
    content->erase(start, end - start);
    return true;
  }

  bool HandleDelete() {
    if (DeleteImpl()) {
      App::PostEventOrExecute(on_change);
      return true;
    }
    return false;
  }

  bool HandleArrowLeft() {
    if (cursor_position() == 0) {
      return false;
    }

    cursor_position() =
        static_cast<int>(GlyphPrevious(content(), cursor_position()));
    return true;
  }

  bool HandleArrowRight() {
    if (cursor_position() == (int)content->size()) {
      return false;
    }

    cursor_position() =
        static_cast<int>(GlyphNext(content(), cursor_position()));
    return true;
  }

  size_t CursorColumn() {
    size_t iter = cursor_position();
    int width = 0;
    while (true) {
      if (iter == 0) {
        break;
      }
      iter = GlyphPrevious(content(), iter);
      if (content()[iter] == '\n') {
        break;
      }
      if (password()) {
        width += 1;
      } else {
        width += static_cast<int>(GlyphWidth(content(), iter));
      }
    }
    return width;
  }

  // Move the cursor `columns` on the right, if possible.
  void MoveCursorColumn(int columns) {
    while (columns > 0) {
      if (cursor_position() == (int)content().size() ||
          content()[cursor_position()] == '\n') {
        return;
      }

      if (password()) {
        columns -= 1;
      } else {
        columns -= static_cast<int>(GlyphWidth(content(), cursor_position()));
      }
      cursor_position() =
          static_cast<int>(GlyphNext(content(), cursor_position()));
    }
  }

  bool HandleArrowUp() {
    if (cursor_position() == 0) {
      return false;
    }

    const size_t columns = CursorColumn();

    // Move cursor at the beginning of 2 lines above.
    while (true) {
      if (cursor_position() == 0) {
        return true;
      }
      const size_t previous = GlyphPrevious(content(), cursor_position());
      if (content()[previous] == '\n') {
        break;
      }
      cursor_position() = static_cast<int>(previous);
    }
    cursor_position() =
        static_cast<int>(GlyphPrevious(content(), cursor_position()));
    while (true) {
      if (cursor_position() == 0) {
        break;
      }
      const size_t previous = GlyphPrevious(content(), cursor_position());
      if (content()[previous] == '\n') {
        break;
      }
      cursor_position() = static_cast<int>(previous);
    }

    MoveCursorColumn(static_cast<int>(columns));
    return true;
  }

  bool HandleArrowDown() {
    if (cursor_position() == (int)content->size()) {
      return false;
    }

    const size_t columns = CursorColumn();

    // Move cursor at the beginning of the next line
    while (true) {
      if (content()[cursor_position()] == '\n') {
        break;
      }
      cursor_position() =
          static_cast<int>(GlyphNext(content(), cursor_position()));
      if (cursor_position() == (int)content().size()) {
        return true;
      }
    }
    cursor_position() =
        static_cast<int>(GlyphNext(content(), cursor_position()));

    MoveCursorColumn(static_cast<int>(columns));
    return true;
  }

  bool HandleHome() {
    cursor_position() = 0;
    return true;
  }

  bool HandleEnd() {
    cursor_position() = static_cast<int>(content->size());
    return true;
  }

  bool HandleLineStart() {
    while (cursor_position() > 0) {
      const size_t previous = GlyphPrevious(content(), cursor_position());
      if (content()[previous] == '\n') {
        break;
      }
      cursor_position() = static_cast<int>(previous);
    }
    return true;
  }

  bool HandleLineEnd() {
    while (cursor_position() < (int)content->size() &&
           content()[cursor_position()] != '\n') {
      cursor_position() =
          static_cast<int>(GlyphNext(content(), cursor_position()));
    }
    return true;
  }

  bool HandleReturn() {
    if (multiline()) {
      HandleCharacter("\n");
    }
    App::PostEventOrExecute(on_enter);
    return true;
  }

  bool HandleCharacter(const std::string& character) {
    if (!insert() && cursor_position() < (int)content->size() &&
        content()[cursor_position()] != '\n') {
      DeleteImpl();
    }
    content->insert(cursor_position(), character);
    cursor_position() += static_cast<int>(character.size());
    App::PostEventOrExecute(on_change);
    return true;
  }

  bool OnEvent(Event event) override {
    cursor_position() = util::clamp(cursor_position(), 0, (int)content->size());

    if (event == Event::Return) {
      return HandleReturn();
    }
    if (event.is_character()) {
      return HandleCharacter(event.character());
    }
    if (event.is_mouse()) {
      return HandleMouse(event);
    }
    if (event == Event::Backspace) {
      return HandleBackspace();
    }
    if (event == Event::BackspaceAlt || event == Event::AltBackspace ||
        event == Event::CtrlW) {
      return HandleBackspaceWord();
    }
    if (event == Event::CtrlU) {
      return HandleBackspaceLine();
    }
    if (event == Event::Delete) {
      return HandleDelete();
    }
    if (event == Event::DeleteAlt || event == Event::DeleteCtrl ||
        event == Event::AltD) {
      return HandleDeleteWord();
    }
    if (event == Event::ArrowLeft) {
      return HandleArrowLeft();
    }
    if (event == Event::ArrowRight) {
      return HandleArrowRight();
    }
    if (event == Event::ArrowUp) {
      return HandleArrowUp();
    }
    if (event == Event::ArrowDown) {
      return HandleArrowDown();
    }
    if (event == Event::Home) {
      return HandleHome();
    }
    if (event == Event::End) {
      return HandleEnd();
    }
    if (event == Event::CtrlA) {
      return HandleLineStart();
    }
    if (event == Event::CtrlE) {
      return HandleLineEnd();
    }
    if (event == Event::ArrowLeftCtrl || event == Event::ArrowLeftAlt ||
        event == Event::AltB) {
      return HandleLeftCtrl();
    }
    if (event == Event::ArrowRightCtrl || event == Event::ArrowRightAlt ||
        event == Event::AltF) {
      return HandleRightCtrl();
    }
    if (event == Event::ArrowUpAlt) {
      return HandleArrowUp();
    }
    if (event == Event::ArrowDownAlt) {
      return HandleArrowDown();
    }
    if (event == Event::Insert) {
      return HandleInsert();
    }
    return false;
  }

  bool HandleLeftCtrl() {
    if (cursor_position() == 0) {
      return false;
    }

    // Move left, as long as left it not a word.
    while (cursor_position()) {
      const size_t previous = GlyphPrevious(content(), cursor_position());
      if (IsWordCharacter(content(), previous)) {
        break;
      }
      cursor_position() = static_cast<int>(previous);
    }
    // Move left, as long as left is a word character:
    while (cursor_position()) {
      const size_t previous = GlyphPrevious(content(), cursor_position());
      if (!IsWordCharacter(content(), previous)) {
        break;
      }
      cursor_position() = static_cast<int>(previous);
    }
    return true;
  }

  bool HandleRightCtrl() {
    if (cursor_position() == (int)content().size()) {
      return false;
    }

    // Move right, until entering a word.
    while (cursor_position() < (int)content().size()) {
      cursor_position() =
          static_cast<int>(GlyphNext(content(), cursor_position()));
      if (IsWordCharacter(content(), cursor_position())) {
        break;
      }
    }
    // Move right, as long as right is a word character:
    while (cursor_position() < (int)content().size()) {
      const size_t next = GlyphNext(content(), cursor_position());
      if (!IsWordCharacter(content(), cursor_position())) {
        break;
      }
      cursor_position() = static_cast<int>(next);
    }

    return true;
  }

  bool HandleMouse(Event event) {
    hovered_ = box_.Contain(event.mouse().x,  //
                            event.mouse().y) &&
               CaptureMouse(event);
    if (!hovered_) {
      return false;
    }

    if (event.mouse().button != Mouse::Left) {
      return false;
    }
    if (event.mouse().motion != Mouse::Pressed) {
      return false;
    }

    TakeFocus();

    if (content->empty()) {
      cursor_position() = 0;
      return true;
    }

    // Find the line and index of the cursor.
    std::vector<std::string> lines = SplitLines(*content);
    int cursor_line = 0;
    int cursor_char_index = cursor_position();
    for (const auto& line : lines) {
      if (cursor_char_index <= (int)line.size()) {
        break;
      }

      cursor_char_index -= static_cast<int>(line.size() + 1);
      cursor_line++;
    }
    const int cursor_column =
        password()
            ? GlyphCount(lines[cursor_line].substr(0, cursor_char_index))
            : string_width(lines[cursor_line].substr(0, cursor_char_index));

    int new_cursor_column = cursor_column + event.mouse().x - cursor_box_.x_min;
    int new_cursor_line = cursor_line + event.mouse().y - cursor_box_.y_min;

    // Fix the new cursor position:
    new_cursor_line = std::max(std::min(new_cursor_line, (int)lines.size()), 0);

    const std::string empty_string;
    const std::string& line = new_cursor_line < (int)lines.size()
                                  ? lines[new_cursor_line]
                                  : empty_string;
    new_cursor_column =
        util::clamp(new_cursor_column, 0,
                    password() ? GlyphCount(line) : string_width(line));

    if (new_cursor_column == cursor_column &&  //
        new_cursor_line == cursor_line) {
      return false;
    }

    // Convert back the new_cursor_{line,column} toward cursor_position:
    cursor_position() = 0;
    for (int i = 0; i < new_cursor_line; ++i) {
      cursor_position() += static_cast<int>(lines[i].size() + 1);
    }
    while (new_cursor_column > 0) {
      if (password()) {
        new_cursor_column -= 1;
      } else {
        new_cursor_column -=
            static_cast<int>(GlyphWidth(content(), cursor_position()));
      }
      cursor_position() =
          static_cast<int>(GlyphNext(content(), cursor_position()));
    }

    App::PostEventOrExecute(on_change);
    return true;
  }

  bool HandleInsert() {
    insert() = !insert();
    return true;
  }

  bool Focusable() const final { return true; }

  bool hovered_ = false;

  Box box_;
  Box cursor_box_;
};

}  // namespace

/// @brief An input box for editing text.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see InputBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// std::string content= "";
/// std::string placeholder = "placeholder";
/// Component input = Input({
///   .content = &content,
///   .placeholder = &placeholder,
/// })
/// screen.Loop(input);
/// ```
///
/// ### Output
///
/// ```bash
/// placeholder
/// ```
Component Input(InputOption option) {
  return Make<InputBase>(std::move(option));
}

/// @brief An input box for editing text.
/// @param content The editable content.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see InputBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// std::string content= "";
/// std::string placeholder = "placeholder";
/// Component input = Input(content, {
///   .placeholder = &placeholder,
///   .password = true,
/// })
/// screen.Loop(input);
/// ```
///
/// ### Output
///
/// ```bash
/// placeholder
/// ```
Component Input(StringRef content, InputOption option) {
  option.content = std::move(content);
  return Make<InputBase>(std::move(option));
}

/// @brief An input box for editing text.
/// @param content The editable content.
/// @param placeholder The placeholder text.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see InputBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::FitComponent();
/// std::string content= "";
/// std::string placeholder = "placeholder";
/// Component input = Input(content, placeholder);
/// screen.Loop(input);
/// ```
///
/// ### Output
///
/// ```bash
/// placeholder
/// ```
Component Input(StringRef content, StringRef placeholder, InputOption option) {
  option.content = std::move(content);
  option.placeholder = std::move(placeholder);
  return Make<InputBase>(std::move(option));
}

}  // namespace ftxui

// ---- component\maybe.cpp -----------------------------------------


namespace ftxui {

/// @brief Decorate a component |child|. It is shown only when |show| returns
/// true.
/// @param child the component to decorate.
/// @param show a function returning whether |child| should shown.
/// @ingroup component
Component Maybe(Component child, std::function<bool()> show) {
  class Impl : public ComponentBase {
   public:
    explicit Impl(std::function<bool()> show) : show_(std::move(show)) {}

   private:
    Element OnRender() override {
      return show_() ? ComponentBase::OnRender() : std::make_unique<Node>();
    }
    bool Focusable() const override {
      return show_() && ComponentBase::Focusable();
    }
    bool OnEvent(Event event) override {
      return show_() && ComponentBase::OnEvent(event);
    }

    std::function<bool()> show_;
  };

  auto maybe = Make<Impl>(std::move(show));
  maybe->Add(std::move(child));
  return maybe;
}

/// @brief Decorate a component. It is shown only when the |show| function
/// returns true.
/// @param show a function returning whether the decorated component should
/// be shown.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto component = Renderer([]{ return text("Hello World!"); });
/// auto maybe_component = component | Maybe([&]{ return counter == 42; });
/// ```
ComponentDecorator Maybe(std::function<bool()> show) {
  return [show = std::move(show)](Component child) mutable {
    return Maybe(std::move(child), std::move(show));
  };
}

/// @brief Decorate a component |child|. It is shown only when |show| is true.
/// @param child the component to decorate.
/// @param show a boolean. |child| is shown when |show| is true.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto component = Renderer([]{ return text("Hello World!"); });
/// auto maybe_component = Maybe(component, &show);
/// ```
Component Maybe(Component child, const bool* show) {
  return Maybe(std::move(child), [show] { return *show; });
}

/// @brief Decorate a component. It is shown only when |show| is true.
/// @param show a boolean. |child| is shown when |show| is true.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto component = Renderer([]{ return text("Hello World!"); });
/// auto maybe_component = component | Maybe(&show);
/// ```
ComponentDecorator Maybe(const bool* show) {
  return [show](Component child) { return Maybe(std::move(child), show); };
}

}  // namespace ftxui

// ---- component\menu.cpp ------------------------------------------


namespace ftxui {

namespace {

Element DefaultOptionTransform(const EntryState& state) {
  std::string label = (state.active ? "> " : "  ") + state.label;  // NOLINT
  Element e = text(std::move(label));
  if (state.focused) {
    e = e | inverted;
  }
  if (state.active) {
    e = e | bold;
  }
  return e;
}

bool IsInverted(Direction direction) {
  switch (direction) {
    case Direction::Up:
    case Direction::Left:
      return true;
    case Direction::Down:
    case Direction::Right:
      return false;
  }
  return false;  // NOT_REACHED()
}

bool IsHorizontal(Direction direction) {
  switch (direction) {
    case Direction::Left:
    case Direction::Right:
      return true;
    case Direction::Down:
    case Direction::Up:
      return false;
  }
  return false;  // NOT_REACHED()
}

}  // namespace

/// @brief A list of items. The user can navigate through them.
/// @ingroup component
class MenuBase : public ComponentBase, public MenuOption {
 public:
  explicit MenuBase(const MenuOption& option) : MenuOption(option) {
    focused_entry() = selected();
  }

  bool IsHorizontal() { return ftxui::IsHorizontal(direction); }
  void OnChange() { App::PostEventOrExecute(on_change); }

  void OnEnter() { App::PostEventOrExecute(on_enter); }

  void Clamp() {
    if (selected() != selected_previous_) {
      SelectedTakeFocus();
    }
    boxes_.resize(size());
    selected() = util::clamp(selected(), 0, size() - 1);
    selected_previous_ = util::clamp(selected_previous_, 0, size() - 1);
    selected_focus_ = util::clamp(selected_focus_, 0, size() - 1);
    focused_entry() = util::clamp(focused_entry(), 0, size() - 1);
  }

  void OnAnimation(animation::Params& params) override {
    animator_first_.OnAnimation(params);
    animator_second_.OnAnimation(params);
    for (auto& animator : animator_background_) {
      animator.OnAnimation(params);
    }
    for (auto& animator : animator_foreground_) {
      animator.OnAnimation(params);
    }
  }

  Element OnRender() override {
    Clamp();
    UpdateAnimationTarget();

    Elements elements;
    const bool is_menu_focused = Focused();
    if (elements_prefix) {
      elements.push_back(elements_prefix());
    }
    elements.reserve(size());
    for (int i = 0; i < size(); ++i) {
      if (i != 0 && elements_infix) {
        elements.push_back(elements_infix());
      }
      const bool is_focused = (focused_entry() == i) && is_menu_focused;
      const bool is_selected = (selected() == i);

      const EntryState state = {
          std::string(entries[i]), false, is_selected, is_focused, i,
      };

      Element element = (entries_option.transform ? entries_option.transform
                                                  : DefaultOptionTransform)  //
          (state);
      if (selected_focus_ == i) {
        element |= focus;
      }
      element |= AnimatedColorStyle(i);
      element |= reflect(boxes_[i]);
      elements.push_back(element);
    }
    if (elements_postfix) {
      elements.push_back(elements_postfix());
    }

    if (IsInverted(direction)) {
      std::reverse(elements.begin(), elements.end());  // NOLINT
    }

    const Element bar =
        IsHorizontal() ? hbox(std::move(elements)) : vbox(std::move(elements));

    if (!underline.enabled) {
      return bar | reflect(box_);
    }

    if (IsHorizontal()) {
      return vbox({
                 bar | xflex,
                 separatorHSelector(first_, second_,  //
                                    underline.color_active,
                                    underline.color_inactive),
             }) |
             reflect(box_);
    } else {
      return hbox({
                 separatorVSelector(first_, second_,  //
                                    underline.color_active,
                                    underline.color_inactive),
                 bar | yflex,
             }) |
             reflect(box_);
    }
  }

  void SelectedTakeFocus() {
    selected_previous_ = selected();
    selected_focus_ = selected();
  }

  void OnUp() {
    switch (direction) {
      case Direction::Up:
        selected()++;
        break;
      case Direction::Down:
        selected()--;
        break;
      case Direction::Left:
      case Direction::Right:
        break;
    }
  }

  void OnDown() {
    switch (direction) {
      case Direction::Up:
        selected()--;
        break;
      case Direction::Down:
        selected()++;
        break;
      case Direction::Left:
      case Direction::Right:
        break;
    }
  }

  void OnLeft() {
    switch (direction) {
      case Direction::Left:
        selected()++;
        break;
      case Direction::Right:
        selected()--;
        break;
      case Direction::Down:
      case Direction::Up:
        break;
    }
  }

  void OnRight() {
    switch (direction) {
      case Direction::Left:
        selected()--;
        break;
      case Direction::Right:
        selected()++;
        break;
      case Direction::Down:
      case Direction::Up:
        break;
    }
  }

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  bool OnEvent(Event event) override {
    Clamp();
    if (!CaptureMouse(event)) {
      return false;
    }

    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    if (Focused()) {
      const int old_selected = selected();
      if (event == Event::ArrowUp || event == Event::Character('k')) {
        OnUp();
      }
      if (event == Event::ArrowDown || event == Event::Character('j')) {
        OnDown();
      }
      if (event == Event::ArrowLeft || event == Event::Character('h')) {
        OnLeft();
      }
      if (event == Event::ArrowRight || event == Event::Character('l')) {
        OnRight();
      }
      if (event == Event::PageUp) {
        selected() -= box_.y_max - box_.y_min;
      }
      if (event == Event::PageDown) {
        selected() += box_.y_max - box_.y_min;
      }
      if (event == Event::Home) {
        selected() = 0;
      }
      if (event == Event::End) {
        selected() = size() - 1;
      }
      if (event == Event::Tab && size()) {
        selected() = (selected() + 1) % size();
      }
      if (event == Event::TabReverse && size()) {
        selected() = (selected() + size() - 1) % size();
      }

      selected() = util::clamp(selected(), 0, size() - 1);

      if (selected() != old_selected) {
        focused_entry() = selected();
        SelectedTakeFocus();
        OnChange();
        return true;
      }
    }

    if (event == Event::Return) {
      OnEnter();
      return true;
    }

    return false;
  }

  bool OnMouseEvent(Event event) {
    if (event.mouse().button == Mouse::WheelDown ||
        event.mouse().button == Mouse::WheelUp) {
      return OnMouseWheel(event);
    }

    if (event.mouse().button != Mouse::None &&
        event.mouse().button != Mouse::Left) {
      return false;
    }
    if (!CaptureMouse(event)) {
      return false;
    }
    for (int i = 0; i < size(); ++i) {
      if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) {
        continue;
      }

      TakeFocus();
      focused_entry() = i;

      if (event.mouse().button == Mouse::Left &&
          event.mouse().motion == Mouse::Pressed) {
        if (selected() != i) {
          selected() = i;
          selected_previous_ = selected();
          OnChange();
        }
        return true;
      }
    }
    return false;
  }

  bool OnMouseWheel(Event event) {
    if (!box_.Contain(event.mouse().x, event.mouse().y)) {
      return false;
    }
    const int old_selected = selected();

    if (event.mouse().button == Mouse::WheelUp) {
      selected()--;
    }
    if (event.mouse().button == Mouse::WheelDown) {
      selected()++;
    }

    selected() = util::clamp(selected(), 0, size() - 1);

    if (selected() != old_selected) {
      SelectedTakeFocus();
      OnChange();
    }
    return true;
  }

  void UpdateAnimationTarget() {
    UpdateColorTarget();
    UpdateUnderlineTarget();
  }

  void UpdateColorTarget() {
    if (size() != int(animation_background_.size())) {
      animation_background_.resize(size());
      animation_foreground_.resize(size());
      animator_background_.clear();
      animator_foreground_.clear();

      const int len = size();
      animator_background_.reserve(len);
      animator_foreground_.reserve(len);
      for (int i = 0; i < len; ++i) {
        animation_background_[i] = 0.F;
        animation_foreground_[i] = 0.F;
        animator_background_.emplace_back(&animation_background_[i], 0.F,
                                          std::chrono::milliseconds(0),
                                          animation::easing::Linear);
        animator_foreground_.emplace_back(&animation_foreground_[i], 0.F,
                                          std::chrono::milliseconds(0),
                                          animation::easing::Linear);
      }
    }

    const bool is_menu_focused = Focused();
    for (int i = 0; i < size(); ++i) {
      const bool is_focused = (focused_entry() == i) && is_menu_focused;
      const bool is_selected = (selected() == i);
      float target = is_selected ? 1.F : is_focused ? 0.5F : 0.F;  // NOLINT
      if (animator_background_[i].to() != target) {
        animator_background_[i] = animation::Animator(
            &animation_background_[i], target,
            entries_option.animated_colors.background.duration,
            entries_option.animated_colors.background.function);
        animator_foreground_[i] = animation::Animator(
            &animation_foreground_[i], target,
            entries_option.animated_colors.foreground.duration,
            entries_option.animated_colors.foreground.function);
      }
    }
  }

  Decorator AnimatedColorStyle(int i) {
    Decorator style = nothing;
    if (entries_option.animated_colors.foreground.enabled) {
      style = style | color(Color::Interpolate(
                          animation_foreground_[i],
                          entries_option.animated_colors.foreground.inactive,
                          entries_option.animated_colors.foreground.active));
    }

    if (entries_option.animated_colors.background.enabled) {
      style = style | bgcolor(Color::Interpolate(
                          animation_background_[i],
                          entries_option.animated_colors.background.inactive,
                          entries_option.animated_colors.background.active));
    }
    return style;
  }

  void UpdateUnderlineTarget() {
    if (!underline.enabled) {
      return;
    }

    if (FirstTarget() == animator_first_.to() &&
        SecondTarget() == animator_second_.to()) {
      return;
    }

    if (FirstTarget() >= animator_first_.to()) {
      animator_first_ = animation::Animator(
          &first_, FirstTarget(), underline.follower_duration,
          underline.follower_function, underline.follower_delay);

      animator_second_ = animation::Animator(
          &second_, SecondTarget(), underline.leader_duration,
          underline.leader_function, underline.leader_delay);
    } else {
      animator_first_ = animation::Animator(
          &first_, FirstTarget(), underline.leader_duration,
          underline.leader_function, underline.leader_delay);

      animator_second_ = animation::Animator(
          &second_, SecondTarget(), underline.follower_duration,
          underline.follower_function, underline.follower_delay);
    }
  }

  bool Focusable() const final { return entries.size(); }
  int size() const { return int(entries.size()); }
  float FirstTarget() {
    if (boxes_.empty()) {
      return 0.F;
    }
    const int value = IsHorizontal() ? boxes_[selected()].x_min - box_.x_min
                                     : boxes_[selected()].y_min - box_.y_min;
    return float(value);
  }
  float SecondTarget() {
    if (boxes_.empty()) {
      return 0.F;
    }
    const int value = IsHorizontal() ? boxes_[selected()].x_max - box_.x_min
                                     : boxes_[selected()].y_max - box_.y_min;
    return float(value);
  }

 protected:
  int selected_previous_ = selected();
  int selected_focus_ = selected();

  // Mouse click support:
  std::vector<Box> boxes_;
  Box box_;

  // Animation support:
  float first_ = 0.F;
  float second_ = 0.F;
  animation::Animator animator_first_ = animation::Animator(&first_, 0.F);
  animation::Animator animator_second_ = animation::Animator(&second_, 0.F);
  std::vector<animation::Animator> animator_background_;
  std::vector<animation::Animator> animator_foreground_;
  std::vector<float> animation_background_;
  std::vector<float> animation_foreground_;
};

/// @brief A list of text. The focused element is selected.
/// @param option a structure containing all the parameters.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// std::vector<std::string> entries = {
///     "entry 1",
///     "entry 2",
///     "entry 3",
/// };
/// int selected = 0;
/// auto menu = Menu({
///   .entries = &entries,
///   .selected = &selected,
/// });
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// > entry 1
///   entry 2
///   entry 3
/// ```
// NOLINTNEXTLINE
Component Menu(MenuOption option) {
  return Make<MenuBase>(std::move(option));
}

/// @brief A list of text. The focused element is selected.
/// @param entries The list of entries in the menu.
/// @param selected The index of the currently selected element.
/// @param option Additional optional parameters.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// std::vector<std::string> entries = {
///     "entry 1",
///     "entry 2",
///     "entry 3",
/// };
/// int selected = 0;
/// auto menu = Menu(&entries, &selected);
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// > entry 1
///   entry 2
///   entry 3
/// ```
Component Menu(ConstStringListRef entries, int* selected, MenuOption option) {
  option.entries = std::move(entries);
  option.selected = selected;
  return Menu(option);
}

/// @brief An horizontal list of elements. The user can navigate through them.
/// @param entries The list of selectable entries to display.
/// @param selected Reference the selected entry.
/// See also |Menu|.
/// @ingroup component
Component Toggle(ConstStringListRef entries, int* selected) {
  return Menu(std::move(entries), selected, MenuOption::Toggle());
}

/// @brief A specific menu entry. They can be put into a Container::Vertical to
/// form a menu.
/// @param label The text drawn representing this element.
/// @param option Additional optional parameters.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// int selected = 0;
/// auto menu = Container::Vertical({
///    MenuEntry("entry 1"),
///    MenuEntry("entry 2"),
///    MenuEntry("entry 3"),
/// }, &selected);
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// > entry 1
///   entry 2
///   entry 3
/// ```
Component MenuEntry(ConstStringRef label, MenuEntryOption option) {
  option.label = std::move(label);
  return MenuEntry(std::move(option));
}

/// @brief A specific menu entry. They can be put into a Container::Vertical to
/// form a menu.
/// @param option The parameters.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// int selected = 0;
/// auto menu = Container::Vertical({
///    MenuEntry({.label = "entry 1"}),
///    MenuEntry({.label = "entry 2"}),
///    MenuEntry({.label = "entry 3"}),
/// }, &selected);
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// > entry 1
///   entry 2
///   entry 3
/// ```
Component MenuEntry(MenuEntryOption option) {
  class Impl : public ComponentBase, public MenuEntryOption {
   public:
    explicit Impl(MenuEntryOption option)
        : MenuEntryOption(std::move(option)) {}

   private:
    Element OnRender() override {
      const bool is_focused = Focused();
      UpdateAnimationTarget();

      const EntryState state{
          std::string(label()), false, hovered_, is_focused, Index(),
      };

      Element element = (transform ? transform : DefaultOptionTransform)  //
          (state);

      if (is_focused) {
        element |= focus;
      }

      return element | AnimatedColorStyle() | reflect(box_);
    }

    void UpdateAnimationTarget() {
      const bool focused = Focused();
      float target = focused ? 1.F : hovered_ ? 0.5F : 0.F;  // NOLINT
      if (target == animator_background_.to()) {
        return;
      }
      animator_background_ = animation::Animator(
          &animation_background_, target, animated_colors.background.duration,
          animated_colors.background.function);
      animator_foreground_ = animation::Animator(
          &animation_foreground_, target, animated_colors.foreground.duration,
          animated_colors.foreground.function);
    }

    Decorator AnimatedColorStyle() {
      Decorator style = nothing;
      if (animated_colors.foreground.enabled) {
        style = style |
                color(Color::Interpolate(animation_foreground_,
                                         animated_colors.foreground.inactive,
                                         animated_colors.foreground.active));
      }

      if (animated_colors.background.enabled) {
        style = style |
                bgcolor(Color::Interpolate(animation_background_,
                                           animated_colors.background.inactive,
                                           animated_colors.background.active));
      }
      return style;
    }

    bool Focusable() const override { return true; }
    bool OnEvent(Event event) override {
      if (!event.is_mouse()) {
        return false;
      }

      hovered_ = box_.Contain(event.mouse().x, event.mouse().y);

      if (!hovered_) {
        return false;
      }

      if (event.mouse().button == Mouse::Left &&
          event.mouse().motion == Mouse::Pressed) {
        TakeFocus();
        return true;
      }

      return false;
    }

    void OnAnimation(animation::Params& params) override {
      animator_background_.OnAnimation(params);
      animator_foreground_.OnAnimation(params);
    }

    Box box_;
    bool hovered_ = false;

    float animation_background_ = 0.F;
    float animation_foreground_ = 0.F;
    animation::Animator animator_background_ =
        animation::Animator(&animation_background_, 0.F);
    animation::Animator animator_foreground_ =
        animation::Animator(&animation_foreground_, 0.F);
  };

  return Make<Impl>(std::move(option));
}

}  // namespace ftxui

// ---- component\modal.cpp -----------------------------------------


namespace ftxui {

// Add a |modal| window on top of the |main| component. It is shown one on the
// top of the other when |show_modal| is true.
/// @ingroup component
// NOLINTNEXTLINE
Component Modal(Component main, Component modal, const bool* show_modal) {
  class Impl : public ComponentBase {
   public:
    explicit Impl(Component main, Component modal, const bool* show_modal)
        : main_(std::move(main)),
          modal_(std::move(modal)),
          show_modal_(show_modal) {
      Add(Container::Tab({main_, modal_}, &selector_));
    }

   private:
    Element OnRender() override {
      selector_ = *show_modal_;
      auto document = main_->Render();
      if (*show_modal_) {
        document = dbox({
            document,
            modal_->Render() | clear_under | center,
        });
      }
      return document;
    }

    bool OnEvent(Event event) override {
      selector_ = *show_modal_;
      return ComponentBase::OnEvent(event);
    }

    Component main_;
    Component modal_;
    const bool* show_modal_;
    int selector_ = *show_modal_;
  };
  return Make<Impl>(main, modal, show_modal);
}

// Decorate a component. Add a |modal| window on top of it. It is shown one on
// the top of the other when |show_modal| is true.
/// @ingroup component
// NOLINTNEXTLINE
ComponentDecorator Modal(Component modal, const bool* show_modal) {
  return [modal, show_modal](Component main) {
    return Modal(std::move(main), modal, show_modal);
  };
}

}  // namespace ftxui

// ---- component\radiobox.cpp --------------------------------------


namespace ftxui {

namespace {
/// @brief A list of selectable element. One and only one can be selected at
/// the same time.
/// @ingroup component
class RadioboxBase : public ComponentBase, public RadioboxOption {
 public:
  explicit RadioboxBase(const RadioboxOption& option)
      : RadioboxOption(option) {}

 private:
  Element OnRender() override {
    Clamp();
    Elements elements;
    const bool is_menu_focused = Focused();
    elements.reserve(size());
    for (int i = 0; i < size(); ++i) {
      const bool is_focused = (focused_entry() == i) && is_menu_focused;
      const bool is_selected = (hovered_ == i);
      auto state = EntryState{
          std::string(entries[i]), selected() == i, is_selected, is_focused, i,
      };
      auto element =
          (transform ? transform : RadioboxOption::Simple().transform)(state);
      if (is_selected) {
        element |= focus;
      }
      elements.push_back(element | reflect(boxes_[i]));
    }
    return vbox(std::move(elements)) | reflect(box_);
  }

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  bool OnEvent(Event event) override {
    Clamp();
    if (!CaptureMouse(event)) {
      return false;
    }

    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    if (Focused()) {
      const int old_hovered = hovered_;
      if (event == Event::ArrowUp || event == Event::Character('k')) {
        (hovered_)--;
      }
      if (event == Event::ArrowDown || event == Event::Character('j')) {
        (hovered_)++;
      }
      if (event == Event::PageUp) {
        (hovered_) -= box_.y_max - box_.y_min;
      }
      if (event == Event::PageDown) {
        (hovered_) += box_.y_max - box_.y_min;
      }
      if (event == Event::Home) {
        (hovered_) = 0;
      }
      if (event == Event::End) {
        (hovered_) = size() - 1;
      }
      if (event == Event::Tab && size()) {
        hovered_ = (hovered_ + 1) % size();
      }
      if (event == Event::TabReverse && size()) {
        hovered_ = (hovered_ + size() - 1) % size();
      }

      hovered_ = util::clamp(hovered_, 0, size() - 1);

      if (hovered_ != old_hovered) {
        focused_entry() = hovered_;
        App::PostEventOrExecute(on_change);
        return true;
      }
    }

    if (event == Event::Character(' ') || event == Event::Return) {
      selected() = hovered_;
      App::PostEventOrExecute(on_change);
      return true;
    }

    return false;
  }

  bool OnMouseEvent(Event event) {
    if (event.mouse().button == Mouse::WheelDown ||
        event.mouse().button == Mouse::WheelUp) {
      return OnMouseWheel(event);
    }

    for (int i = 0; i < size(); ++i) {
      if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) {
        continue;
      }

      TakeFocus();
      focused_entry() = i;
      if (event.mouse().button == Mouse::Left &&
          event.mouse().motion == Mouse::Pressed) {
        if (selected() != i) {
          selected() = i;
          App::PostEventOrExecute(on_change);
        }

        return true;
      }
    }
    return false;
  }

  bool OnMouseWheel(Event event) {
    if (!box_.Contain(event.mouse().x, event.mouse().y)) {
      return false;
    }

    const int old_hovered = hovered_;

    if (event.mouse().button == Mouse::WheelUp) {
      (hovered_)--;
    }
    if (event.mouse().button == Mouse::WheelDown) {
      (hovered_)++;
    }

    hovered_ = util::clamp(hovered_, 0, size() - 1);

    if (hovered_ != old_hovered) {
      App::PostEventOrExecute(on_change);
    }

    return true;
  }

  void Clamp() {
    boxes_.resize(size());
    selected() = util::clamp(selected(), 0, size() - 1);
    focused_entry() = util::clamp(focused_entry(), 0, size() - 1);
    hovered_ = util::clamp(hovered_, 0, size() - 1);
  }

  bool Focusable() const final { return entries.size(); }
  int size() const { return int(entries.size()); }

  int hovered_ = selected();
  std::vector<Box> boxes_;
  Box box_;
};

}  // namespace

/// @brief A list of element, where only one can be selected.
/// @param option The parameters
/// @ingroup component
/// @see RadioboxBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// std::vector<std::string> entries = {
///     "entry 1",
///     "entry 2",
///     "entry 3",
/// };
/// int selected = 0;
/// auto menu = Radiobox({
///   .entries = entries,
///   .selected = &selected,
/// });
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// ◉ entry 1
/// ○ entry 2
/// ○ entry 3
/// ```
/// NOLINTNEXTLINE
Component Radiobox(RadioboxOption option) {
  return Make<RadioboxBase>(std::move(option));
}

/// @brief A list of element, where only one can be selected.
/// @param entries The list of entries in the list.
/// @param selected The index of the currently selected element.
/// @param option Additional optional parameters.
/// @ingroup component
/// @see RadioboxBase
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// std::vector<std::string> entries = {
///     "entry 1",
///     "entry 2",
///     "entry 3",
/// };
/// int selected = 0;
/// auto menu = Radiobox(&entries, &selected);
/// screen.Loop(menu);
/// ```
///
/// ### Output
///
/// ```bash
/// ◉ entry 1
/// ○ entry 2
/// ○ entry 3
/// ```
Component Radiobox(ConstStringListRef entries,
                   int* selected,
                   RadioboxOption option) {
  option.entries = std::move(entries);
  option.selected = selected;
  return Make<RadioboxBase>(std::move(option));
}

}  // namespace ftxui

// ---- component\renderer.cpp --------------------------------------


namespace ftxui {

/// @brief Return a component, using |render| to render its interface.
/// @param render The function drawing the interface.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// auto renderer = Renderer([] {
///   return text("My interface");
/// });
/// screen.Loop(renderer);
/// ```
Component Renderer(std::function<Element()> render) {
  class Impl : public ComponentBase {
   public:
    explicit Impl(std::function<Element()> render)
        : render_(std::move(render)) {}
    Element OnRender() override { return render_(); }
    std::function<Element()> render_;
  };

  return Make<Impl>(std::move(render));
}

/// @brief Return a new Component, similar to |child|, but using |render| as the
/// Component::Render() event.
/// @param child The component to forward events to.
/// @param render The function drawing the interface.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// std::string label = "Click to quit";
/// auto button = Button(&label, screen.ExitLoopClosure());
/// auto renderer = Renderer(button, [&] {
///   return hbox({
///     text("A button:"),
///     button->Render(),
///   });
/// });
/// screen.Loop(renderer);
/// ```
Component Renderer(Component child, std::function<Element()> render) {
  Component renderer = Renderer(std::move(render));
  renderer->Add(std::move(child));
  return renderer;
}

/// @brief Return a focusable component, using |render| to render its interface.
/// @param render The function drawing the interface, taking a boolean telling
/// whether the component is focused or not.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// auto renderer = Renderer([] (bool focused) {
///   if (focused)
///     return text("My interface") | inverted;
///   else
///     return text("My interface");
/// });
/// screen.Loop(renderer);
/// ```
Component Renderer(std::function<Element(bool)> render) {
  class Impl : public ComponentBase {
   public:
    explicit Impl(std::function<Element(bool)> render)
        : render_(std::move(render)) {}

   private:
    Element OnRender() override { return render_(Focused()) | reflect(box_); }
    bool Focusable() const override { return true; }
    bool OnEvent(Event event) override {
      if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y)) {
        if (!CaptureMouse(event)) {
          return false;
        }

        TakeFocus();
      }

      return false;
    }
    Box box_;

    std::function<Element(bool)> render_;
  };
  return Make<Impl>(std::move(render));
}

/// @brief Decorate a component, by decorating what it renders.
/// @param decorator the function modifying the element it renders.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// auto renderer =
///   Renderer([] { return text("Hello"); })
///   | Renderer(bold)
///   | Renderer(inverted);
/// screen.Loop(renderer);
/// ```
ComponentDecorator Renderer(ElementDecorator decorator) {  // NOLINT
  return [decorator](Component component) {                // NOLINT
    return Renderer(component, [component, decorator] {
      return component->Render() | decorator;
    });
  };
}

}  // namespace ftxui

// ---- component\resizable_split.cpp -------------------------------


namespace ftxui {
namespace {

class ResizableSplitBase : public ComponentBase, public ResizableSplitOption {
 public:
  explicit ResizableSplitBase(ResizableSplitOption options)
      : ResizableSplitOption(std::move(options)) {
    switch (direction()) {
      case Direction::Left:
        Add(Container::Horizontal({main, back}));
        break;
      case Direction::Right:
        Add(Container::Horizontal({back, main}));
        break;
      case Direction::Up:
        Add(Container::Vertical({main, back}));
        break;
      case Direction::Down:
        Add(Container::Vertical({back, main}));
        break;
    }
  }

  bool OnEvent(Event event) final {
    if (event.is_mouse()) {
      return OnMouseEvent(std::move(event));
    }
    return ComponentBase::OnEvent(std::move(event));
  }

  bool OnMouseEvent(Event event) {
    if (captured_mouse_ && event.mouse().motion == Mouse::Released) {
      captured_mouse_.reset();
      return true;
    }

    if (event.mouse().button == Mouse::Left &&
        event.mouse().motion == Mouse::Pressed &&
        separator_box_.Contain(event.mouse().x, event.mouse().y) &&
        !captured_mouse_) {
      captured_mouse_ = CaptureMouse(event);
      return true;
    }

    if (!captured_mouse_) {
      return ComponentBase::OnEvent(event);
    }

    switch (direction()) {
      case Direction::Left:
        main_size() = std::max(0, event.mouse().x - box_.x_min);
        break;
      case Direction::Right:
        main_size() = std::max(0, box_.x_max - event.mouse().x);
        break;
      case Direction::Up:
        main_size() = std::max(0, event.mouse().y - box_.y_min);
        break;
      case Direction::Down:
        main_size() = std::max(0, box_.y_max - event.mouse().y);
        break;
    }

    main_size() = std::clamp(main_size(), min(), max());
    return true;
  }

  Element OnRender() final {
    switch (direction()) {
      case Direction::Left:
        return RenderLeft();
      case Direction::Right:
        return RenderRight();
      case Direction::Up:
        return RenderTop();
      case Direction::Down:
        return RenderBottom();
    }
    // NOTREACHED()
    return text("unreachable");
  }

  Element RenderLeft() {
    return hbox({
               main->Render() | size(WIDTH, EQUAL, main_size()),
               separator_func() | reflect(separator_box_),
               back->Render() | xflex,
           }) |
           reflect(box_);
  }

  Element RenderRight() {
    return hbox({
               back->Render() | xflex,
               separator_func() | reflect(separator_box_),
               main->Render() | size(WIDTH, EQUAL, main_size()),
           }) |
           reflect(box_);
  }

  Element RenderTop() {
    return vbox({
               main->Render() | size(HEIGHT, EQUAL, main_size()),
               separator_func() | reflect(separator_box_),
               back->Render() | yflex,
           }) |
           reflect(box_);
  }

  Element RenderBottom() {
    return vbox({
               back->Render() | yflex,
               separator_func() | reflect(separator_box_),
               main->Render() | size(HEIGHT, EQUAL, main_size()),
           }) |
           reflect(box_);
  }

 private:
  CapturedMouse captured_mouse_;
  Box separator_box_;
  Box box_;
};

}  // namespace

/// @brief A split in between two components.
/// @param options all the parameters.
///
/// ### Example
///
/// ```cpp
/// auto left = Renderer([] { return text("Left") | center;});
/// auto right = Renderer([] { return text("right") | center;});
/// int left_size = 10;
/// auto component = ResizableSplit({
///   .main = left,
///   .back = right,
///   .direction = Direction::Left,
///   .main_size = &left_size,
///   .separator_func = [] { return separatorDouble(); },
/// });
/// ```
///
/// ### Output
///
/// ```bash
///           ║
///    left   ║   right
///           ║
/// ```
Component ResizableSplit(ResizableSplitOption options) {
  return Make<ResizableSplitBase>(std::move(options));
}

/// @brief An horizontal split in between two components, configurable using the
/// mouse.
/// @param main The main component of size |main_size|, on the left.
/// @param back The back component taking the remaining size, on the right.
/// @param main_size The size of the |main| component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::Fullscreen();
/// int left_size = 10;
/// auto left = Renderer([] { return text("Left") | center;});
/// auto right = Renderer([] { return text("right") | center;});
/// auto split = ResizableSplitLeft(left, right, &left_size);
/// screen.Loop(split);
/// ```
///
/// ### Output
///
/// ```bash
///           │
///    left   │   right
///           │
/// ```
Component ResizableSplitLeft(Component main, Component back, int* main_size) {
  return ResizableSplit({
      std::move(main),
      std::move(back),
      Direction::Left,
      main_size,
  });
}

/// @brief An horizontal split in between two components, configurable using the
/// mouse.
/// @param main The main component of size |main_size|, on the right.
/// @param back The back component taking the remaining size, on the left.
/// @param main_size The size of the |main| component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::Fullscreen();
/// int right_size = 10;
/// auto left = Renderer([] { return text("Left") | center;});
/// auto right = Renderer([] { return text("right") | center;});
/// auto split = ResizableSplitRight(right, left, &right_size)
/// screen.Loop(split);
/// ```
///
/// ### Output
///
/// ```bash
///           │
///    left   │   right
///           │
/// ```
Component ResizableSplitRight(Component main, Component back, int* main_size) {
  return ResizableSplit({
      std::move(main),
      std::move(back),
      Direction::Right,
      main_size,
  });
}

/// @brief An vertical split in between two components, configurable using the
/// mouse.
/// @param main The main component of size |main_size|, on the top.
/// @param back The back component taking the remaining size, on the bottom.
/// @param main_size The size of the |main| component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::Fullscreen();
/// int top_size = 1;
/// auto top = Renderer([] { return text("Top") | center;});
/// auto bottom = Renderer([] { return text("Bottom") | center;});
/// auto split = ResizableSplitTop(top, bottom, &top_size)
/// screen.Loop(split);
/// ```
///
/// ### Output
///
/// ```bash
///    top
/// ────────────
///    bottom
/// ```
Component ResizableSplitTop(Component main, Component back, int* main_size) {
  return ResizableSplit({
      std::move(main),
      std::move(back),
      Direction::Up,
      main_size,
  });
}

/// @brief An vertical split in between two components, configurable using the
/// mouse.
/// @param main The main component of size |main_size|, on the bottom.
/// @param back The back component taking the remaining size, on the top.
/// @param main_size The size of the |main| component.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::Fullscreen();
/// int bottom_size = 1;
/// auto top = Renderer([] { return text("Top") | center;});
/// auto bottom = Renderer([] { return text("Bottom") | center;});
/// auto split = ResizableSplit::Bottom(bottom, top, &bottom_size)
/// screen.Loop(split);
/// ```
///
/// ### Output
///
/// ```bash
///    top
/// ────────────
///    bottom
/// ```
Component ResizableSplitBottom(Component main, Component back, int* main_size) {
  return ResizableSplit({
      std::move(main),
      std::move(back),
      Direction::Down,
      main_size,
  });
}

}  // namespace ftxui

// ---- component\slider.cpp ----------------------------------------


namespace ftxui {

namespace {
Decorator flexDirection(Direction direction) {
  switch (direction) {
    case Direction::Up:
    case Direction::Down:
      return yflex;
    case Direction::Left:
    case Direction::Right:
      return xflex;
  }
  return xflex;  // NOT_REACHED()
}

Direction Opposite(Direction d) {
  switch (d) {
    case Direction::Up:
      return Direction::Down;
    case Direction::Down:
      return Direction::Up;
    case Direction::Left:
      return Direction::Right;
    case Direction::Right:
      return Direction::Left;
  }
  return d;  // NOT_REACHED()
}

template <class T>
class SliderBase : public SliderOption<T>, public ComponentBase {
 public:
  explicit SliderBase(SliderOption<T> options)
      : SliderOption<T>(std::move(options)) {}

  Element OnRender() override {
    auto gauge_color =
        Focused() ? color(this->color_active) : color(this->color_inactive);
    const float percent =
        float(this->value() - this->min()) / float(this->max() - this->min());
    return gaugeDirection(percent, this->direction) |
           flexDirection(this->direction) | reflect(gauge_box_) | gauge_color;
  }

  void OnDirection(Direction pressed) {
    if (pressed == this->direction) {
      this->value() += this->increment();
      return;
    }

    if (pressed == Opposite(this->direction)) {
      this->value() -= this->increment();
      return;
    }
  }

  bool OnEvent(Event event) final {
    if (event.is_mouse()) {
      return OnMouseEvent(event);
    }

    T old_value = this->value();
    if (event == Event::ArrowLeft || event == Event::Character('h')) {
      OnDirection(Direction::Left);
    }
    if (event == Event::ArrowRight || event == Event::Character('l')) {
      OnDirection(Direction::Right);
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      OnDirection(Direction::Up);
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      OnDirection(Direction::Down);
    }

    this->value() = std::max(this->min(), std::min(this->max(), this->value()));
    if (old_value != this->value()) {
      App::PostEventOrExecute(this->on_change);
      return true;
    }

    return ComponentBase::OnEvent(event);
  }

  bool OnCapturedMouseEvent(Event event) {
    if (event.mouse().motion == Mouse::Released) {
      captured_mouse_ = nullptr;
      return true;
    }

    T old_value = this->value();
    switch (this->direction) {
      case Direction::Right: {
        this->value() = this->min() + (event.mouse().x - gauge_box_.x_min) *
                                          (this->max() - this->min()) /
                                          (gauge_box_.x_max - gauge_box_.x_min);

        break;
      }
      case Direction::Left: {
        this->value() = this->max() - (event.mouse().x - gauge_box_.x_min) *
                                          (this->max() - this->min()) /
                                          (gauge_box_.x_max - gauge_box_.x_min);
        break;
      }
      case Direction::Down: {
        this->value() = this->min() + (event.mouse().y - gauge_box_.y_min) *
                                          (this->max() - this->min()) /
                                          (gauge_box_.y_max - gauge_box_.y_min);
        break;
      }
      case Direction::Up: {
        this->value() = this->max() - (event.mouse().y - gauge_box_.y_min) *
                                          (this->max() - this->min()) /
                                          (gauge_box_.y_max - gauge_box_.y_min);
        break;
      }
    }

    this->value() = std::max(this->min(), std::min(this->max(), this->value()));

    if (old_value != this->value()) {
      App::PostEventOrExecute(this->on_change);
    }
    return true;
  }

  bool OnMouseEvent(Event event) {
    if (captured_mouse_) {
      return OnCapturedMouseEvent(event);
    }

    if (event.mouse().button != Mouse::Left) {
      return false;
    }
    if (event.mouse().motion != Mouse::Pressed) {
      return false;
    }

    if (!gauge_box_.Contain(event.mouse().x, event.mouse().y)) {
      return false;
    }

    captured_mouse_ = CaptureMouse(event);

    if (captured_mouse_) {
      TakeFocus();
      return OnCapturedMouseEvent(event);
    }

    return false;
  }

  bool Focusable() const final { return true; }

 private:
  Box gauge_box_;
  CapturedMouse captured_mouse_;
};

class SliderWithLabel : public ComponentBase {
 public:
  SliderWithLabel(ConstStringRef label, Component inner)
      : label_(std::move(label)) {
    Add(std::move(inner));
    SetActiveChild(ChildAt(0));
  }

 private:
  bool OnEvent(Event event) final {
    if (ComponentBase::OnEvent(event)) {
      return true;
    }

    if (!event.is_mouse()) {
      return false;
    }

    mouse_hover_ = box_.Contain(event.mouse().x, event.mouse().y);

    if (!mouse_hover_) {
      return false;
    }

    if (!CaptureMouse(event)) {
      return false;
    }

    return true;
  }

  Element OnRender() override {
    auto gauge_color = (Focused() || mouse_hover_) ? color(Color::White)
                                                   : color(Color::GrayDark);
    auto element = hbox({
                       text(label_()) | dim | vcenter,
                       hbox({
                           text("["),
                           ComponentBase::Render() | underlined,
                           text("]"),
                       }) | vcenter |
                           xflex,
                   }) |
                   gauge_color | xflex | reflect(box_);

    element |= focus;
    return element;
  }

  ConstStringRef label_;
  Box box_;
  bool mouse_hover_ = false;
};

}  // namespace

/// @brief An horizontal slider.
/// @param label The name of the slider.
/// @param value The current value of the slider.
/// @param min The minimum value.
/// @param max The maximum value.
/// @param increment The increment when used by the cursor.
/// @ingroup component
///
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// int value = 50;
/// auto slider = Slider("Value:", &value, 0, 100, 1);
/// screen.Loop(slider);
/// ```
///
/// ### Output
///
/// ```bash
/// Value:[██████████████████████████                          ]
/// ```
Component Slider(ConstStringRef label,
                 Ref<int> value,
                 ConstRef<int> min,
                 ConstRef<int> max,
                 ConstRef<int> increment) {
  SliderOption<int> option;
  option.value = value;
  option.min = min;
  option.max = max;
  option.increment = increment;
  auto slider = Make<SliderBase<int>>(option);
  return Make<SliderWithLabel>(std::move(label), slider);
}

Component Slider(ConstStringRef label,
                 Ref<float> value,
                 ConstRef<float> min,
                 ConstRef<float> max,
                 ConstRef<float> increment) {
  SliderOption<float> option;
  option.value = value;
  option.min = min;
  option.max = max;
  option.increment = increment;
  auto slider = Make<SliderBase<float>>(option);
  return Make<SliderWithLabel>(std::move(label), slider);
}
Component Slider(ConstStringRef label,
                 Ref<long> value,
                 ConstRef<long> min,
                 ConstRef<long> max,
                 ConstRef<long> increment) {
  SliderOption<long> option;
  option.value = value;
  option.min = min;
  option.max = max;
  option.increment = increment;
  auto slider = Make<SliderBase<long>>(option);
  return Make<SliderWithLabel>(std::move(label), slider);
}

/// @brief A slider in any direction.
/// @param options The options
/// ### Example
///
/// ```cpp
/// auto screen = App::TerminalOutput();
/// int value = 50;
/// auto slider = Slider({
///   .value = &value,
///   .min = 0,
///   .max = 100,
///   .increment= 20,
/// });
/// screen.Loop(slider);
/// ```
template <typename T>
Component Slider(SliderOption<T> options) {
  return Make<SliderBase<T>>(std::move(options));
}

template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int8_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int16_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int32_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<int64_t>);

template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<uint8_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<uint16_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<uint32_t>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<uint64_t>);

template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<float>);
template FTXUI_EXPORT(COMPONENT) Component Slider(SliderOption<double>);

}  // namespace ftxui

// ---- component\task.cpp ------------------------------------------

namespace ftxui::task {
bool PendingTask::operator<(const PendingTask& other) const {
  if (!time && !other.time) {
    return false;
  }
  if (!time) {
    return true;
  }
  if (!other.time) {
    return false;
  }
  return time.value() > other.time.value();
}
}  // namespace ftxui::task

// ---- component\task_queue.cpp ------------------------------------

namespace ftxui::task {

auto TaskQueue::PostTask(PendingTask task) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!task.time) {
    immediate_tasks_.push(task);
    return;
  }

  if (task.time.value() < std::chrono::steady_clock::now()) {
    immediate_tasks_.push(task);
    return;
  }

  delayed_tasks_.push(task);
}

auto TaskQueue::Get() -> MaybeTask {
  const std::lock_guard<std::mutex> lock(mutex_);
  // Attempt to execute a task immediately.
  if (!immediate_tasks_.empty()) {
    auto task = immediate_tasks_.front();
    immediate_tasks_.pop();
    return task.task;
  }

  // Move all tasks that can be executed to the immediate queue.
  auto now = std::chrono::steady_clock::now();
  while (!delayed_tasks_.empty()) {
    const auto& top = delayed_tasks_.top();
    if (!top.time.has_value() || top.time.value() > now) {
      break;
    }
    immediate_tasks_.push(top);
    delayed_tasks_.pop();
  }

  // Attempt to execute a task immediately.
  if (!immediate_tasks_.empty()) {
    auto task = immediate_tasks_.front();
    immediate_tasks_.pop();
    return task.task;
  }

  // If there are no tasks to execute, return the delay until the next task.
  if (!delayed_tasks_.empty()) {
    const auto& top = delayed_tasks_.top();
    if (top.time.has_value()) {
      return top.time.value() - now;
    }
  }

  // If there are no tasks to execute, return the maximum duration.
  return std::monostate{};
}

auto TaskQueue::HasImmediateTasks() const -> bool {
  const std::lock_guard<std::mutex> lock(mutex_);
  return !immediate_tasks_.empty();
}

}  // namespace ftxui::task

// ---- component\task_runner.cpp -----------------------------------


namespace ftxui::task {

static thread_local TaskRunner* current_task_runner = nullptr;  // NOLINT

TaskRunner::TaskRunner() {
  assert(!previous_task_runner_);
  previous_task_runner_ = current_task_runner;
  current_task_runner = this;
}

TaskRunner::~TaskRunner() {
  current_task_runner = previous_task_runner_;
}

// static
auto TaskRunner::Current() -> TaskRunner* {
  assert(current_task_runner);
  return current_task_runner;
}

auto TaskRunner::PostTask(Task task) -> void {
  queue_.PostTask(PendingTask{std::move(task)});
}

auto TaskRunner::PostDelayedTask(Task task,
                                 std::chrono::steady_clock::duration duration)
    -> void {
  queue_.PostTask(PendingTask{std::move(task), duration});
}

/// Runs the tasks in the queue.
auto TaskRunner::RunUntilIdle()
    -> std::optional<std::chrono::steady_clock::duration> {
  while (true) {
    auto maybe_task = queue_.Get();
    if (std::holds_alternative<std::monostate>(maybe_task)) {
      // No more tasks to execute, exit the loop.
      return std::nullopt;
    }

    if (std::holds_alternative<Task>(maybe_task)) {
      executed_tasks_++;
      std::get<Task>(maybe_task)();
      continue;
    }

    if (std::holds_alternative<std::chrono::steady_clock::duration>(
            maybe_task)) {
      return std::get<std::chrono::steady_clock::duration>(maybe_task);
    }
  }
}

auto TaskRunner::Run() -> void {
  while (true) {
    auto duration = RunUntilIdle();
    if (!duration) {
      // No more tasks to execute, exit the loop.
      return;
    }

    // Sleep for the duration until the next task can be executed.
    std::this_thread::sleep_for(duration.value());
  }
}

}  // namespace ftxui::task

// ---- component\terminal_input_parser.cpp -------------------------


namespace ftxui {

// NOLINTNEXTLINE
const std::map<std::string, std::string> g_uniformize = {
    // Microsoft's terminal uses a different new line character for the return
    // key. This also happens with linux with the `bind` command:
    // See https://github.com/ArthurSonzogni/FTXUI/issues/337
    // Here, we uniformize the new line character to `\n`.
    {"\r", "\n"},

    // See: https://github.com/ArthurSonzogni/FTXUI/issues/508
    {std::string({8}), std::string({127})},
    {std::string({27, 8}), std::string({27, 127})},

    // See: https://github.com/ArthurSonzogni/FTXUI/issues/626
    //
    // Depending on the Cursor Key Mode (DECCKM), the terminal sends different
    // escape sequences:
    //
    //   Key     Normal    Application
    //   -----   --------  -----------
    //   Up      ESC [ A   ESC O A
    //   Down    ESC [ B   ESC O B
    //   Right   ESC [ C   ESC O C
    //   Left    ESC [ D   ESC O D
    //   Home    ESC [ H   ESC O H
    //   End     ESC [ F   ESC O F
    //
    {"\x1BOA", "\x1B[A"},  // UP
    {"\x1BOB", "\x1B[B"},  // DOWN
    {"\x1BOC", "\x1B[C"},  // RIGHT
    {"\x1BOD", "\x1B[D"},  // LEFT
    {"\x1BOH", "\x1B[H"},  // HOME
    {"\x1BOF", "\x1B[F"},  // END

    // Common Home/End sequences from terminals and multiplexers.
    {"\x1B[1~", "\x1B[H"},  // HOME
    {"\x1B[4~", "\x1B[F"},  // END

    // Variations around the FN keys.
    // Internally, we are using:
    // vt220, xterm-vt200, xterm-xf86-v44, xterm-new, mgt, screen
    // See: https://invisible-island.net/xterm/xterm-function-keys.html

    // For linux OS console (CTRL+ALT+FN), who do not belong to any
    // real standard.
    // See: https://github.com/ArthurSonzogni/FTXUI/issues/685
    {"\x1B[[A", "\x1BOP"},    // F1
    {"\x1B[[B", "\x1BOQ"},    // F2
    {"\x1B[[C", "\x1BOR"},    // F3
    {"\x1B[[D", "\x1BOS"},    // F4
    {"\x1B[[E", "\x1B[15~"},  // F5

    // xterm-r5, xterm-r6, rxvt
    {"\x1B[11~", "\x1BOP"},  // F1
    {"\x1B[12~", "\x1BOQ"},  // F2
    {"\x1B[13~", "\x1BOR"},  // F3
    {"\x1B[14~", "\x1BOS"},  // F4

    // vt100
    {"\x1BOt", "\x1B[15~"},  // F5
    {"\x1BOu", "\x1B[17~"},  // F6
    {"\x1BOv", "\x1B[18~"},  // F7
    {"\x1BOl", "\x1B[19~"},  // F8
    {"\x1BOw", "\x1B[20~"},  // F9
    {"\x1BOx", "\x1B[21~"},  // F10

    // scoansi
    {"\x1B[M", "\x1BOP"},    // F1
    {"\x1B[N", "\x1BOQ"},    // F2
    {"\x1B[O", "\x1BOR"},    // F3
    {"\x1B[P", "\x1BOS"},    // F4
    {"\x1B[Q", "\x1B[15~"},  // F5
    {"\x1B[R", "\x1B[17~"},  // F6
    {"\x1B[S", "\x1B[18~"},  // F7
    {"\x1B[T", "\x1B[19~"},  // F8
    {"\x1B[U", "\x1B[20~"},  // F9
    {"\x1B[V", "\x1B[21~"},  // F10
    {"\x1B[W", "\x1B[23~"},  // F11
    {"\x1B[X", "\x1B[24~"},  // F12
};

TerminalInputParser::TerminalInputParser(std::function<void(Event)> out)
    : out_(std::move(out)) {}

void TerminalInputParser::Timeout(int time) {
  timeout_ += time;
  const int timeout_threshold = 50;
  if (timeout_ < timeout_threshold) {
    return;
  }
  timeout_ = 0;
  if (!pending_.empty()) {
    Send(SPECIAL);
  }
}

void TerminalInputParser::Add(char c) {
  pending_ += c;
  timeout_ = 0;
  position_ = -1;
  Send(Parse());
}

unsigned char TerminalInputParser::Current() {
  return pending_[position_];
}

bool TerminalInputParser::Eat() {
  position_++;
  return position_ < static_cast<int>(pending_.size());
}

void TerminalInputParser::Send(TerminalInputParser::Output output) {
  switch (output.type) {
    case UNCOMPLETED:
      return;

    case DROP:
      pending_.clear();
      return;

    case RESYNC: {
      // The bytes accumulated so far can't be continued by the one at
      // |position_|, which starts a new sequence. Emit the truncated prefix and
      // parse the remaining bytes again.
      std::string next = pending_.substr(position_);
      pending_.resize(position_);
      Send(SPECIAL);
      pending_ = std::move(next);
      position_ = -1;
      Send(Parse());
      return;
    }

    case CHARACTER:
      out_(Event::Character(std::move(pending_)));
      pending_.clear();
      return;

    case SPECIAL: {
      auto it = g_uniformize.find(pending_);
      if (it != g_uniformize.end()) {
        pending_ = it->second;
      }
      out_(Event::Special(std::move(pending_)));
      pending_.clear();
    }
      return;

    case MOUSE:
      out_(Event::Mouse(std::move(pending_), output.mouse));  // NOLINT
      pending_.clear();
      return;

    case CURSOR_POSITION:
      out_(Event::CursorPosition(std::move(pending_),  // NOLINT
                                 output.cursor.x,      // NOLINT
                                 output.cursor.y));    // NOLINT
      pending_.clear();
      return;

    case CURSOR_SHAPE:
      out_(Event::CursorShape(std::move(pending_), output.cursor_shape));
      pending_.clear();
      return;

    case TERMINAL_NAME_VERSION:
      out_(Event::TerminalNameVersion(std::move(pending_),
                                      std::move(output.terminal_name),
                                      output.terminal_version));
      pending_.clear();
      return;

    case TERMINAL_EMULATOR:
      out_(Event::TerminalEmulator(std::move(pending_),
                                   std::move(output.terminal_name),
                                   std::move(output.terminal_version_string)));
      pending_.clear();
      return;

    case TERMINAL_CAPABILITIES:
      out_(Event::TerminalCapabilities(
          std::move(pending_), std::move(output.terminal_capabilities)));
      pending_.clear();
      return;
  }
  // NOT_REACHED().
}

TerminalInputParser::Output TerminalInputParser::Parse() {
  if (!Eat()) {
    return UNCOMPLETED;
  }

  if (Current() == '\x1B') {
    return ParseESC();
  }

  if (Current() < 32) {  // C0 NOLINT
    return SPECIAL;
  }

  if (Current() == 127) {  // Delete // NOLINT
    return SPECIAL;
  }

  return ParseUTF8();
}

// Code point <-> UTF-8 conversion
//
// ┏━━━━━━━━┳━━━━━━━━┳━━━━━━━━┳━━━━━━━━┓
// ┃Byte 1  ┃Byte 2  ┃Byte 3  ┃Byte 4  ┃
// ┡━━━━━━━━╇━━━━━━━━╇━━━━━━━━╇━━━━━━━━┩
// │0xxxxxxx│        │        │        │
// ├────────┼────────┼────────┼────────┤
// │110xxxxx│10xxxxxx│        │        │
// ├────────┼────────┼────────┼────────┤
// │1110xxxx│10xxxxxx│10xxxxxx│        │
// ├────────┼────────┼────────┼────────┤
// │11110xxx│10xxxxxx│10xxxxxx│10xxxxxx│
// └────────┴────────┴────────┴────────┘
//
// Then some sequences are illegal if it exist a shorter representation of the
// same codepoint.
TerminalInputParser::Output TerminalInputParser::ParseUTF8() {
  auto head = Current();
  unsigned char selector = 0b1000'0000;  // NOLINT

  // The non code-point part of the first byte.
  unsigned char mask = selector;

  // Find the first zero in the first byte.
  unsigned int first_zero = 8;            // NOLINT
  for (unsigned int i = 0; i < 8; ++i) {  // NOLINT
    mask |= selector;
    if (!(head & selector)) {
      first_zero = i;
      break;
    }
    selector >>= 1U;
  }

  // Accumulate the value of the first byte.
  auto value = uint32_t(head & ~mask);  // NOLINT

  // Invalid UTF8, with more than 5 bytes.
  const unsigned int max_utf8_bytes = 5;
  if (first_zero == 1 || first_zero >= max_utf8_bytes) {
    return DROP;
  }

  // Multi byte UTF-8.
  for (unsigned int i = 2; i <= first_zero; ++i) {
    if (!Eat()) {
      return UNCOMPLETED;
    }

    // Invalid continuation byte.
    head = Current();
    if ((head & 0b1100'0000) != 0b1000'0000) {  // NOLINT
      return DROP;
    }
    value <<= 6;                  // NOLINT
    value += head & 0b0011'1111;  // NOLINT
  }

  // Check for overlong UTF8 encoding.
  int extra_byte = 0;
  if (value <= 0b000'0000'0111'1111) {                 // NOLINT
    extra_byte = 0;                                    // NOLINT
  } else if (value <= 0b000'0111'1111'1111) {          // NOLINT
    extra_byte = 1;                                    // NOLINT
  } else if (value <= 0b1111'1111'1111'1111) {         // NOLINT
    extra_byte = 2;                                    // NOLINT
  } else if (value <= 0b1'0000'1111'1111'1111'1111) {  // NOLINT
    extra_byte = 3;                                    // NOLINT
  } else {                                             // NOLINT
    return DROP;
  }

  if (extra_byte != position_) {
    return DROP;
  }

  return CHARACTER;
}

TerminalInputParser::Output TerminalInputParser::ParseESC() {
  if (!Eat()) {
    return UNCOMPLETED;
  }
  switch (Current()) {
    case 'P':
      return ParseDCS();
    case '[':
      return ParseCSI();
    case ']':
      return ParseOSC();

    // An ESC is not allowed inside a sequence. This one starts a new one.
    case '\x1B':
      return RESYNC;

    // Expecting 2 characters.
    case ' ':
    case '#':
    case '%':
    case '(':
    case ')':
    case '*':
    case '+':
    case 'O':
    case 'N': {
      if (!Eat()) {
        return UNCOMPLETED;
      }
      if (Current() == '\x1B') {
        return RESYNC;
      }
      return SPECIAL;
    }
    // Expecting 1 character:
    default:
      return SPECIAL;
  }
}

// ESC P ... ESC BACKSLASH
TerminalInputParser::Output TerminalInputParser::ParseDCS() {
  // Parse until the string terminator ST.
  while (true) {
    if (!Eat()) {
      return UNCOMPLETED;
    }

    if (Current() != '\x1B') {
      continue;
    }

    if (!Eat()) {
      return UNCOMPLETED;
    }

    if (Current() != '\\') {
      continue;
    }

    // XTVERSION: ESC P > | name version ST
    if (pending_.size() >= 5 && pending_[2] == '>' && pending_[3] == '|') {
      // ESC P > | name (version) ST
      // 0   1 2 3 4
      const std::string content = pending_.substr(4, pending_.size() - 6);
      Output output(TERMINAL_EMULATOR);
      const size_t space = content.find(' ');
      const size_t open_paren = content.find('(');
      if (space != std::string::npos) {
        output.terminal_name = content.substr(0, space);
        output.terminal_version_string = content.substr(space + 1);
      } else if (open_paren != std::string::npos) {
        output.terminal_name = content.substr(0, open_paren);
        output.terminal_version_string = content.substr(open_paren + 1);
        if (!output.terminal_version_string.empty() &&
            output.terminal_version_string.back() == ')') {
          output.terminal_version_string.pop_back();
        }
      } else {
        output.terminal_name = content;
        output.terminal_version_string = "unknown";
      }
      return output;
    }

    if (pending_.size() == 10 &&  //
        pending_[2] == '1' &&     //
        pending_[3] == '$' &&     //
        pending_[4] == 'r' &&     //
        true) {
      Output output(CURSOR_SHAPE);
      output.cursor_shape = pending_[5] - '0';
      return output;
    }

    return SPECIAL;
  }
}

TerminalInputParser::Output TerminalInputParser::ParseCSI() {
  bool altered_less = false;
  bool altered_greater = false;
  bool altered_question = false;
  int argument = 0;
  std::vector<int> arguments;
  while (true) {
    if (!Eat()) {
      return UNCOMPLETED;
    }

    if (Current() == '<') {
      altered_less = true;
      continue;
    }

    if (Current() == '>') {
      altered_greater = true;
      continue;
    }

    if (Current() == '?') {
      altered_question = true;
      continue;
    }

    if (Current() >= '0' && Current() <= '9') {
      argument *= 10;  // NOLINT
      argument += Current() - '0';
      continue;
    }

    if (Current() == ';') {
      arguments.push_back(argument);
      argument = 0;
      continue;
    }

    // CSI is terminated by a character in the range 0x40–0x7E
    // (ASCII @A–Z[\]^_`a–z{|}~),
    if (Current() >= '@' && Current() <= '~' &&
        // Note: I don't remember why we exclude '<'
        Current() != '<' &&
        // To handle F1-F4, we exclude '['.
        Current() != '[') {
      arguments.push_back(argument);
      argument = 0;  // NOLINT

      switch (Current()) {
        case 'M':
          return ParseMouse(altered_less, true, std::move(arguments));
        case 'm':
          return ParseMouse(altered_less, false, std::move(arguments));
        case 'R':
          return ParseCursorPosition(std::move(arguments));
        case 'c':
          return ParseDeviceAttributes(altered_greater, altered_question,
                                       std::move(arguments));
        default:
          return SPECIAL;
      }
    }

    // Invalid ESC in CSI. It starts a new sequence.
    if (Current() == '\x1B') {
      return RESYNC;
    }
  }
}

TerminalInputParser::Output TerminalInputParser::ParseOSC() {
  // Parse until the string terminator ST.
  while (true) {
    if (!Eat()) {
      return UNCOMPLETED;
    }
    if (Current() != '\x1B') {
      continue;
    }
    if (!Eat()) {
      return UNCOMPLETED;
    }
    if (Current() != '\\') {
      continue;
    }
    return SPECIAL;
  }
}

TerminalInputParser::Output TerminalInputParser::ParseMouse(  // NOLINT
    bool altered,
    bool pressed,
    std::vector<int> arguments) {
  if (arguments.size() != 3) {
    return SPECIAL;
  }

  (void)altered;

  Output output(MOUSE);
  output.mouse.motion = Mouse::Motion(pressed);  // NOLINT

  // Bits value Modifier  Comment
  // ---- ----- ------- ---------
  // 0 1  1 2   button   0 = Left, 1 = Middle, 2 = Right, 3 = Release
  // 2    4     Shift
  // 3    8     Meta
  // 4    16    Control
  // 5    32    Move
  // 6    64    Wheel

  // clang-format off
  const int button      = arguments[0] & (1 + 2); // NOLINT
  const bool is_shift   = arguments[0] & 4;       // NOLINT
  const bool is_meta    = arguments[0] & 8;       // NOLINT
  const bool is_control = arguments[0] & 16;      // NOLINT
  const bool is_move    = arguments[0] & 32;      // NOLINT
  const bool is_wheel   = arguments[0] & 64;      // NOLINT
  // clang-format on

  output.mouse.motion = is_move ? Mouse::Moved : Mouse::Motion(pressed);
  output.mouse.button = is_wheel ? Mouse::Button(Mouse::WheelUp + button)  //
                                 : Mouse::Button(button);
  output.mouse.shift = is_shift;
  output.mouse.meta = is_meta;
  output.mouse.control = is_control;
  output.mouse.x = arguments[1];  // NOLINT
  output.mouse.y = arguments[2];  // NOLINT

  // Motion event.
  return output;
}

// NOLINTNEXTLINE
TerminalInputParser::Output TerminalInputParser::ParseCursorPosition(
    std::vector<int> arguments) {
  if (arguments.size() != 2) {
    return SPECIAL;
  }
  Output output(CURSOR_POSITION);
  output.cursor.y = arguments[0];  // NOLINT
  output.cursor.x = arguments[1];  // NOLINT
  return output;
}

// NOLINTNEXTLINE
TerminalInputParser::Output TerminalInputParser::ParseDeviceAttributes(
    bool altered_greater,
    bool altered_question,
    std::vector<int> arguments) {
  if (altered_greater) {
    // Secondary Device Attributes (DA2)
    // ESC [ > Pp ; Pv ; Pc c
    if (arguments.size() >= 3) {
      // Pp: Terminal type
      // Pv: Firmware version
      // Pc: Hardware options
      Output output(TERMINAL_NAME_VERSION);
      output.terminal_version = arguments[1];
      switch (arguments[0]) {
        case 0:
          output.terminal_name = "xterm";
          break;
        case 1:
          output.terminal_name = "vt220";
          break;
        case 2:
          output.terminal_name = "vt240";
          break;
        case 18:
          output.terminal_name = "vt330";
          break;
        case 19:
          output.terminal_name = "vt340";
          break;
        case 24:
          output.terminal_name = "vt320";
          break;
        case 41:
          output.terminal_name = "vt420";
          break;
        case 61:
          output.terminal_name = "vt510";
          break;
        case 64:
          output.terminal_name = "vt520";
          break;
        case 65:
          output.terminal_name = "vt525";
          break;
        case 84:
          output.terminal_name = "tmux";
          break;
        case 85:
          output.terminal_name = "urxvt";
          break;
        default:
          output.terminal_name = "unknown";
          break;
      }
      // Special case for xterm which often returns 0;pv;0 or similar
      // but it's not strictly following DEC VT types.
      return output;
    }
  } else if (altered_question) {
    // Primary Device Attributes (DA1)
    // ESC [ ? Pp ; ... c
    Output output(TERMINAL_CAPABILITIES);
    output.terminal_capabilities = std::move(arguments);
    return output;
  }
  return SPECIAL;
}

}  // namespace ftxui

// ---- component\window.cpp ----------------------------------------
#define NOMINMAX



namespace ftxui {
struct Box;

// Helper class.
// Internal class exported for the 'component' library.
class FTXUI_EXPORT(DOM) NodeDecorator : public Node {
 public:
  explicit NodeDecorator(Element child) : Node(unpack(std::move(child))) {}
  void ComputeRequirement() override;
  void SetBox(Box box) override;
};

}  // namespace ftxui


namespace ftxui {

namespace {

Decorator PositionAndSize(int left, int top, int width, int height) {
  return [=](Element element) {
    element |= size(WIDTH, EQUAL, width);
    element |= size(HEIGHT, EQUAL, height);

    auto padding_left = emptyElement() | size(WIDTH, EQUAL, left);
    auto padding_top = emptyElement() | size(HEIGHT, EQUAL, top);

    return vbox({
        padding_top,
        hbox({
            padding_left,
            element,
        }),
    });
  };
}

class ResizeDecorator : public NodeDecorator {
 public:
  ResizeDecorator(Element child,
                  bool resize_left,
                  bool resize_right,
                  bool resize_top,
                  bool resize_down,
                  Color color)
      : NodeDecorator(std::move(child)),
        color_(color),
        resize_left_(resize_left),
        resize_right_(resize_right),
        resize_top_(resize_top),
        resize_down_(resize_down) {}

  void Render(Screen& screen) override {
    NodeDecorator::Render(screen);

    if (resize_left_) {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        auto& cell = screen.CellAt(box_.x_min, y);
        cell.foreground_color = color_;
        cell.automerge = false;
      }
    }
    if (resize_right_) {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        auto& cell = screen.CellAt(box_.x_max, y);
        cell.foreground_color = color_;
        cell.automerge = false;
      }
    }
    if (resize_top_) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        auto& cell = screen.CellAt(x, box_.y_min);
        cell.foreground_color = color_;
        cell.automerge = false;
      }
    }
    if (resize_down_) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        auto& cell = screen.CellAt(x, box_.y_max);
        cell.foreground_color = color_;
        cell.automerge = false;
      }
    }
  }

  Color color_;
  const bool resize_left_;
  const bool resize_right_;
  const bool resize_top_;
  const bool resize_down_;
};

Element DefaultRenderState(const WindowRenderState& state) {
  Element element = state.inner;
  if (!state.active) {
    element |= dim;
  }

  element = window(text(state.title), element);
  element |= clear_under;

  const Color color = Color::Red;

  element = std::make_shared<ResizeDecorator>(  //
      element,                                  //
      state.hover_left,                         //
      state.hover_right,                        //
      state.hover_top,                          //
      state.hover_down,                         //
      color                                     //
  );

  return element;
}

class WindowImpl : public ComponentBase, public WindowOptions {
 public:
  explicit WindowImpl(WindowOptions option) : WindowOptions(std::move(option)) {
    if (!inner) {
      inner = Make<ComponentBase>();
    }
    Add(inner);
  }

 private:
  Element OnRender() final {
    auto element = ComponentBase::Render();

    const bool captureable =
        captured_mouse_ || (App::Active() && App::Active()->CaptureMouse());

    const WindowRenderState state = {
        element,
        title(),
        Active(),
        drag_,
        resize_left_ || resize_right_ || resize_down_ || resize_top_,
        (resize_left_hover_ || resize_left_) && captureable,
        (resize_right_hover_ || resize_right_) && captureable,
        (resize_top_hover_ || resize_top_) && captureable,
        (resize_down_hover_ || resize_down_) && captureable,
    };

    element = render ? render(state) : DefaultRenderState(state);

    // Position and record the drawn area of the window.
    element |= reflect(box_window_);
    element |= PositionAndSize(left(), top(), width(), height());
    element |= reflect(box_);

    return element;
  }

  bool OnEvent(Event event) final {
    if (ComponentBase::OnEvent(event)) {
      return true;
    }

    if (!event.is_mouse()) {
      return false;
    }

    mouse_hover_ = box_window_.Contain(event.mouse().x, event.mouse().y);

    resize_down_hover_ = false;
    resize_top_hover_ = false;
    resize_left_hover_ = false;
    resize_right_hover_ = false;

    if (mouse_hover_) {
      resize_left_hover_ = event.mouse().x == left() + box_.x_min;
      resize_right_hover_ =
          event.mouse().x == left() + width() - 1 + box_.x_min;
      resize_top_hover_ = event.mouse().y == top() + box_.y_min;
      resize_down_hover_ = event.mouse().y == top() + height() - 1 + box_.y_min;

      // Apply the component options:
      resize_top_hover_ &= resize_top();
      resize_left_hover_ &= resize_left();
      resize_down_hover_ &= resize_down();
      resize_right_hover_ &= resize_right();
    }

    if (captured_mouse_) {
      if (event.mouse().motion == Mouse::Released) {
        captured_mouse_ = nullptr;
        return true;
      }

      if (resize_left_) {
        width() = left() + width() - event.mouse().x + box_.x_min;
        left() = event.mouse().x - box_.x_min;
      }

      if (resize_right_) {
        width() = event.mouse().x - resize_start_x - box_.x_min;
      }

      if (resize_top_) {
        height() = top() + height() - event.mouse().y + box_.y_min;
        top() = event.mouse().y - box_.y_min;
      }

      if (resize_down_) {
        height() = event.mouse().y - resize_start_y - box_.y_min;
      }

      if (drag_) {
        left() = event.mouse().x - drag_start_x - box_.x_min;
        top() = event.mouse().y - drag_start_y - box_.y_min;
      }

      // Clamp the window size.
      width() = std::max<int>(width(), static_cast<int>(title().size() + 2));
      height() = std::max<int>(height(), 2);

      return true;
    }

    resize_left_ = false;
    resize_right_ = false;
    resize_top_ = false;
    resize_down_ = false;

    if (!mouse_hover_) {
      return false;
    }

    if (!CaptureMouse(event)) {
      return true;
    }

    if (event.mouse().button != Mouse::Left) {
      return true;
    }
    if (event.mouse().motion != Mouse::Pressed) {
      return true;
    }

    TakeFocus();

    captured_mouse_ = CaptureMouse(event);
    if (!captured_mouse_) {
      return true;
    }

    resize_left_ = resize_left_hover_;
    resize_right_ = resize_right_hover_;
    resize_top_ = resize_top_hover_;
    resize_down_ = resize_down_hover_;

    resize_start_x = event.mouse().x - width() - box_.x_min;
    resize_start_y = event.mouse().y - height() - box_.y_min;
    drag_start_x = event.mouse().x - left() - box_.x_min;
    drag_start_y = event.mouse().y - top() - box_.y_min;

    // Drag only if we are not resizeing a border yet:
    drag_ = !resize_right_ && !resize_down_ && !resize_top_ && !resize_left_;
    return true;
  }

  Box box_;
  Box box_window_;

  CapturedMouse captured_mouse_;
  int drag_start_x = 0;
  int drag_start_y = 0;
  int resize_start_x = 0;
  int resize_start_y = 0;

  bool mouse_hover_ = false;
  bool drag_ = false;
  bool resize_top_ = false;
  bool resize_left_ = false;
  bool resize_down_ = false;
  bool resize_right_ = false;

  bool resize_top_hover_ = false;
  bool resize_left_hover_ = false;
  bool resize_down_hover_ = false;
  bool resize_right_hover_ = false;
};

}  // namespace

/// @brief A draggeable / resizeable window. To use multiple of them, they must
/// be stacked using `Container::Stacked({...})` component;
///
/// @param option A struct holding every parameters.
/// @ingroup component
/// @see Window
///
/// ### Example
///
/// ```cpp
/// auto window_1= Window({
///     .inner = DummyWindowContent(),
///     .title = "First window",
/// });
///
/// auto window_2= Window({
///     .inner = DummyWindowContent(),
///     .title = "Second window",
/// });
///
/// auto container = Container::Stacked({
///   window_1,
///   window_2,
/// });
/// ```
Component Window(WindowOptions option) {
  return Make<WindowImpl>(std::move(option));
}

};  // namespace ftxui

// ---- dom\automerge.cpp -------------------------------------------


namespace ftxui {

/// @brief Enable character to be automatically merged with others nearby.
/// @ingroup dom
Element automerge(Element child) {
  class Impl : public NodeDecorator {
   public:
    using NodeDecorator::NodeDecorator;

    void Render(Screen& screen) override {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).automerge = true;
        }
      }
      Node::Render(screen);
    }
  };

  return std::make_shared<Impl>(std::move(child));
}

}  // namespace ftxui

// ---- dom\blink.cpp -----------------------------------------------


namespace ftxui {

namespace {
class Blink : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    Node::Render(screen);
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).blink = true;
      }
    }
  }
};
}  // namespace

/// @brief The text drawn alternates in between visible and hidden.
/// @ingroup dom
Element blink(Element child) {
  return std::make_shared<Blink>(std::move(child));
}

}  // namespace ftxui

// ---- dom\bold.cpp ------------------------------------------------


namespace ftxui {

namespace {
class Bold : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).bold = true;
      }
    }
    Node::Render(screen);
  }
};
}  // namespace

/// @brief Use a bold font, for elements with more emphasis.
/// @ingroup dom
Element bold(Element child) {
  return std::make_shared<Bold>(std::move(child));
}

}  // namespace ftxui

// ---- dom\border.cpp ----------------------------------------------


namespace ftxui {

namespace {
using Charset = std::array<std::string, 6>;  // NOLINT
using Charsets = std::array<Charset, 6>;     // NOLINT
// NOLINTNEXTLINE
static Charsets simple_border_charset = {
    Charset{"┌", "┐", "└", "┘", "─", "│"},  // LIGHT
    Charset{"┏", "┓", "┗", "┛", "╍", "╏"},  // DASHED
    Charset{"┏", "┓", "┗", "┛", "━", "┃"},  // HEAVY
    Charset{"╔", "╗", "╚", "╝", "═", "║"},  // DOUBLE
    Charset{"╭", "╮", "╰", "╯", "─", "│"},  // ROUNDED
    Charset{" ", " ", " ", " ", " ", " "},  // EMPTY
};

// For reference, here is the charset for normal border:
class Border : public Node {
 public:
  Border(Elements children,
         BorderStyle style,
         std::optional<Color> foreground_color = std::nullopt)
      : Node(std::move(children)),
        charset_(simple_border_charset[style])  // NOLINT
        ,
        foreground_color_(foreground_color) {}  // NOLINT

  const Charset& charset_;  // NOLINT
  std::optional<Color> foreground_color_;

  void ComputeRequirement() override {
    Node::ComputeRequirement();
    requirement_ = children_[0]->requirement();
    requirement_.min_x += 2;
    requirement_.min_y += 2;
    if (children_.size() == 2) {
      requirement_.min_x =
          std::max(requirement_.min_x, children_[1]->requirement().min_x + 2);
    }
    requirement_.focused.box.x_min++;
    requirement_.focused.box.x_max++;
    requirement_.focused.box.y_min++;
    requirement_.focused.box.y_max++;
  }

  void SetBox(Box box) override {
    Node::SetBox(box);
    if (children_.size() == 2) {
      Box title_box;
      title_box.x_min = box.x_min + 1;
      title_box.x_max = box.x_max - 1;
      title_box.y_min = box.y_min;
      title_box.y_max = box.y_min;
      children_[1]->SetBox(title_box);
    }
    box.x_min++;
    box.x_max--;
    box.y_min++;
    box.y_max--;
    children_[0]->SetBox(box);
  }

  void Render(Screen& screen) override {
    // Draw content.
    children_[0]->Render(screen);

    // Draw the border.
    if (box_.x_min >= box_.x_max || box_.y_min >= box_.y_max) {
      return;
    }

    screen.at(box_.x_min, box_.y_min) = charset_[0];  // NOLINT
    screen.at(box_.x_max, box_.y_min) = charset_[1];  // NOLINT
    screen.at(box_.x_min, box_.y_max) = charset_[2];  // NOLINT
    screen.at(box_.x_max, box_.y_max) = charset_[3];  // NOLINT

    for (int x = box_.x_min + 1; x < box_.x_max; ++x) {
      Cell& p1 = screen.CellAt(x, box_.y_min);
      Cell& p2 = screen.CellAt(x, box_.y_max);
      p1.character = charset_[4];  // NOLINT
      p2.character = charset_[4];  // NOLINT
      p1.automerge = true;
      p2.automerge = true;
    }
    for (int y = box_.y_min + 1; y < box_.y_max; ++y) {
      Cell& p3 = screen.CellAt(box_.x_min, y);
      Cell& p4 = screen.CellAt(box_.x_max, y);
      p3.character = charset_[5];  // NOLINT
      p4.character = charset_[5];  // NOLINT
      p3.automerge = true;
      p4.automerge = true;
    }

    // Draw title.
    if (children_.size() == 2) {
      RenderTitle(screen);
    }

    // Draw the border color.
    if (foreground_color_) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, box_.y_min).foreground_color = *foreground_color_;
        screen.CellAt(x, box_.y_max).foreground_color = *foreground_color_;
      }
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        screen.CellAt(box_.x_min, y).foreground_color = *foreground_color_;
        screen.CellAt(box_.x_max, y).foreground_color = *foreground_color_;
      }
    }
  }

  // The title spans the whole top border, so that it can be aligned. Restore
  // the border cells it didn't draw on, so that its decorators (e.g. color)
  // only apply to its content.
  void RenderTitle(Screen& screen) {
    std::vector<Cell> top_border;
    for (int x = box_.x_min + 1; x < box_.x_max; ++x) {
      top_border.push_back(screen.CellAt(x, box_.y_min));
    }
    children_[1]->Render(screen);
    for (int x = box_.x_min + 1; x < box_.x_max; ++x) {
      Cell& cell = screen.CellAt(x, box_.y_min);
      const Cell& saved = top_border[x - box_.x_min - 1];
      if (cell.character == saved.character) {
        cell = saved;
      }
    }
  }
};

// For reference, here is the charset for normal border:
class BorderCell : public Node {
 public:
  BorderCell(Elements children, Cell pixel)
      : Node(std::move(children)), pixel_(std::move(pixel)) {}

 private:
  Cell pixel_;

  void ComputeRequirement() override {
    Node::ComputeRequirement();
    requirement_ = children_[0]->requirement();
    requirement_.min_x += 2;
    requirement_.min_y += 2;
    if (children_.size() == 2) {
      requirement_.min_x =
          std::max(requirement_.min_x, children_[1]->requirement().min_x + 2);
    }

    requirement_.focused.box.Shift(1, 1);
  }

  void SetBox(Box box) override {
    Node::SetBox(box);
    if (children_.size() == 2) {
      Box title_box;
      title_box.x_min = box.x_min + 1;
      title_box.x_max = box.x_max - 1;
      title_box.y_min = box.y_min;
      title_box.y_max = box.y_min;
      children_[1]->SetBox(title_box);
    }
    box.x_min++;
    box.x_max--;
    box.y_min++;
    box.y_max--;
    children_[0]->SetBox(box);
  }

  void Render(Screen& screen) override {
    // Draw content.
    children_[0]->Render(screen);

    // Draw the border.
    if (box_.x_min >= box_.x_max || box_.y_min >= box_.y_max) {
      return;
    }

    screen.CellAt(box_.x_min, box_.y_min) = pixel_;
    screen.CellAt(box_.x_max, box_.y_min) = pixel_;
    screen.CellAt(box_.x_min, box_.y_max) = pixel_;
    screen.CellAt(box_.x_max, box_.y_max) = pixel_;

    for (int x = box_.x_min + 1; x < box_.x_max; ++x) {
      screen.CellAt(x, box_.y_min) = pixel_;
      screen.CellAt(x, box_.y_max) = pixel_;
    }
    for (int y = box_.y_min + 1; y < box_.y_max; ++y) {
      screen.CellAt(box_.x_min, y) = pixel_;
      screen.CellAt(box_.x_max, y) = pixel_;
    }
  }
};
}  // namespace

/// @brief Draw a border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderEmpty
/// @see borderRounded
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = border(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | border;
/// ```
///
/// ### Output
///
/// ```bash
/// ┌───────────┐
/// │The element│
/// └───────────┘
/// ```
Element border(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), ROUNDED);
}

/// @brief Same as border but with a constant Cell around the element.
/// @ingroup dom
/// @see border
Decorator borderWith(const Cell& pixel) {
  return [pixel](Element child) {
    return std::make_shared<BorderCell>(unpack(std::move(child)), pixel);
  };
}

/// @brief Same as border but with different styles.
/// @ingroup dom
/// @see border
Decorator borderStyled(BorderStyle style) {
  return [style](Element child) {
    return std::make_shared<Border>(unpack(std::move(child)), style);
  };
}

/// @brief Same as border but with a foreground color.
/// @ingroup dom
/// @see border
Decorator borderStyled(Color foreground_color) {
  return [foreground_color](Element child) {
    return std::make_shared<Border>(unpack(std::move(child)), ROUNDED,
                                    foreground_color);
  };
}

/// @brief Same as border but with a foreground color and a different style
/// @ingroup dom
/// @see border
Decorator borderStyled(BorderStyle style, Color foreground_color) {
  return [style, foreground_color](Element child) {
    return std::make_shared<Border>(unpack(std::move(child)), style,
                                    foreground_color);
  };
}

/// @brief Draw a dashed border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderDash' as a function...
/// Element document = borderDash(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderDAsh;
/// ```
///
/// ### Output
///
/// ```bash
/// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
/// ╏The element   ╏
/// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
/// ```
Element borderDashed(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), DASHED);
}

/// @brief Draw a light border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderLight' as a function...
/// Element document = borderLight(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderLight;
/// ```
///
/// ### Output
///
/// ```bash
/// ┌──────────────┐
/// │The element   │
/// └──────────────┘
/// ```
Element borderLight(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), LIGHT);
}

/// @brief Draw a heavy border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderHeavy' as a function...
/// Element document = borderHeavy(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderHeavy;
/// ```
///
/// ### Output
///
/// ```bash
/// ┏━━━━━━━━━━━━━━┓
/// ┃The element   ┃
/// ┗━━━━━━━━━━━━━━┛
/// ```
Element borderHeavy(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), HEAVY);
}

/// @brief Draw a double border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderDouble' as a function...
/// Element document = borderDouble(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderDouble;
/// ```
///
/// ### Output
///
/// ```bash
/// ╔══════════════╗
/// ║The element   ║
/// ╚══════════════╝
/// ```
Element borderDouble(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), DOUBLE);
}

/// @brief Draw a rounded border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderRounded' as a function...
/// Element document = borderRounded(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderRounded;
/// ```
///
/// ### Output
///
/// ```bash
/// ╭──────────────╮
/// │The element   │
/// ╰──────────────╯
/// ```
Element borderRounded(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), ROUNDED);
}

/// @brief Draw an empty border around the element.
/// @ingroup dom
/// @see border
/// @see borderLight
/// @see borderDashed
/// @see borderDouble
/// @see borderHeavy
/// @see borderRounded
/// @see borderEmpty
/// @see borderStyled
/// @see borderWith
///
/// Add a border around an element
///
/// ### Example
///
/// ```cpp
/// // Use 'borderRounded' as a function...
/// Element document = borderRounded(text("The element"));
///
/// // ...Or as a 'pipe'.
/// Element document = text("The element") | borderRounded;
/// ```
///
/// ### Output
///
/// ```bash
///
///  The element
///
/// ```
Element borderEmpty(Element child) {
  return std::make_shared<Border>(unpack(std::move(child)), EMPTY);
}

/// @brief Draw window with a title and a border around the element.
/// @param title The title of the window.
/// @param content The element to be wrapped.
/// @param border The style of the border. Default is ROUNDED.
/// @ingroup dom
/// @see border
///
/// ### Example
///
/// ```cpp
/// Element document = window(text("Title"),
///                           text("content")
///                    );
///
/// // With specifying border
/// Element document = window(text("Title"),
///                           text("content"),
///                           ROUNDED
///                    );
/// ```
///
/// ### Output
///
/// ```bash
/// ┌Title──┐
/// │content│
/// └───────┘
/// ```
Element window(Element title, Element content, BorderStyle border) {
  return std::make_shared<Border>(unpack(std::move(content), std::move(title)),
                                  border);
}
}  // namespace ftxui

// ---- dom\box_helper.cpp ------------------------------------------


namespace ftxui::box_helper {

struct Element {
  // Input:
  int min_size = 0;
  int flex_grow = 0;
  int flex_shrink = 0;

  // Output;
  int size = 0;
};

void Compute(std::vector<Element>* elements, int target_size);
}  // namespace ftxui::box_helper



namespace ftxui::box_helper {

namespace {

int SafeRatio(int value, int numerator, int denominator) {
  return static_cast<int>(
      static_cast<int64_t>(value) * static_cast<int64_t>(numerator) /
      std::max(static_cast<int64_t>(denominator), static_cast<int64_t>(1)));
}

// Called when the size allowed is greater than the requested size. This
// distributes the extra spaces toward the flexible elements, in relative
// proportions.
void ComputeGrow(std::vector<Element>* elements,
                 int extra_space,
                 int flex_grow_sum) {
  for (Element& element : *elements) {
    const int added_space =
        SafeRatio(extra_space, element.flex_grow, flex_grow_sum);
    extra_space -= added_space;
    flex_grow_sum -= element.flex_grow;
    element.size = element.min_size + added_space;
  }
}

// Called when the size allowed is lower than the requested size, and the
// shrinkable element can absorbe the (negative) extra_space. This distribute
// the extra_space toward those.
void ComputeShrinkEasy(std::vector<Element>* elements,
                       int extra_space,
                       int flex_shrink_sum) {
  for (Element& element : *elements) {
    const int added_space = SafeRatio(
        extra_space, element.min_size * element.flex_shrink, flex_shrink_sum);
    extra_space -= added_space;
    flex_shrink_sum -= element.flex_shrink * element.min_size;
    element.size = element.min_size + added_space;
  }
}

// Called when the size allowed is lower than the requested size, and the
// shrinkable element can not absorb the (negative) extra_space. This assigns
// zero to shrinkable elements and distribute the remaining (negative)
// extra_space toward the other non shrinkable elements.
void ComputeShrinkHard(std::vector<Element>* elements,
                       int extra_space,
                       int size) {
  for (Element& element : *elements) {
    if (element.flex_shrink != 0) {
      element.size = 0;
      continue;
    }

    const int added_space = SafeRatio(extra_space, element.min_size, size);

    extra_space -= added_space;
    size -= element.min_size;

    element.size = element.min_size + added_space;
  }
}

}  // namespace

void Compute(std::vector<Element>* elements, int target_size) {
  int size = 0;
  int flex_grow_sum = 0;
  int flex_shrink_sum = 0;
  int flex_shrink_size = 0;

  for (auto& element : *elements) {
    flex_grow_sum += element.flex_grow;
    flex_shrink_sum += element.min_size * element.flex_shrink;
    if (element.flex_shrink != 0) {
      flex_shrink_size += element.min_size;
    }
    size += element.min_size;
  }

  const int extra_space = target_size - size;
  if (extra_space >= 0) {
    ComputeGrow(elements, extra_space, flex_grow_sum);
  } else if (flex_shrink_size + extra_space >= 0) {
    ComputeShrinkEasy(elements, extra_space, flex_shrink_sum);

  } else {
    ComputeShrinkHard(elements, extra_space + flex_shrink_size,
                      size - flex_shrink_size);
  }
}

}  // namespace ftxui::box_helper

// ---- dom\canvas.cpp ----------------------------------------------

// On Windows, DrawText is a macro defined in windows.h. This conflicts with our
// Canvas::DrawText method when building as a single translation unit.
#ifdef DrawText
#undef DrawText
#endif



namespace ftxui {

namespace {

// Base UTF8 pattern:
// 11100010 10100000 10000000 // empty

// Pattern for the individual dots:
// ┌──────┬───────┐
// │dot1  │ dot4  │
// ├──────┼───────┤
// │dot2  │ dot5  │
// ├──────┼───────┤
// │dot3  │ dot6  │
// ├──────┼───────┤
// │dot0-1│ dot0-2│
// └──────┴───────┘
// 11100010 10100000 10000001 // dot1
// 11100010 10100000 10000010 // dot2
// 11100010 10100000 10000100 // dot3
// 11100010 10100001 10000000 // dot0-1
// 11100010 10100000 10001000 // dot4
// 11100010 10100000 10010000 // dot5
// 11100010 10100000 10100000 // dot6
// 11100010 10100010 10000000 // dot0-2

// NOLINTNEXTLINE
uint8_t g_map_braille[2][4][2] = {
    {
        {0b00000000, 0b00000001},  // NOLINT | dot1
        {0b00000000, 0b00000010},  // NOLINT | dot2
        {0b00000000, 0b00000100},  // NOLINT | dot3
        {0b00000001, 0b00000000},  // NOLINT | dot0-1
    },
    {
        {0b00000000, 0b00001000},  // NOLINT | dot4
        {0b00000000, 0b00010000},  // NOLINT | dot5
        {0b00000000, 0b00100000},  // NOLINT | dot6
        {0b00000010, 0b00000000},  // NOLINT | dot0-2
    },
};

// NOLINTNEXTLINE
std::vector<std::string> g_map_block = {
    " ", "▘", "▖", "▌", "▝", "▀", "▞", "▛",
    "▗", "▚", "▄", "▙", "▐", "▜", "▟", "█",
};

// NOLINTNEXTLINE
const std::map<std::string, uint8_t> g_map_block_inversed = {
    {" ", 0b0000}, {"▘", 0b0001}, {"▖", 0b0010}, {"▌", 0b0011},
    {"▝", 0b0100}, {"▀", 0b0101}, {"▞", 0b0110}, {"▛", 0b0111},
    {"▗", 0b1000}, {"▚", 0b1001}, {"▄", 0b1010}, {"▙", 0b1011},
    {"▐", 0b1100}, {"▜", 0b1101}, {"▟", 0b1110}, {"█", 0b1111},
};

constexpr auto nostyle = [](Cell& /*pixel*/) {};

}  // namespace

/// @brief Constructor.
/// @param width the width of the canvas. A cell is a 2x4 braille dot.
/// @param height the height of the canvas. A cell is a 2x4 braille dot.
Canvas::Canvas(int width, int height)
    : width_(std::max(0, width)),
      height_(std::max(0, height)),
      storage_(static_cast<size_t>(width_) * static_cast<size_t>(height_) /
               8 /* NOLINT */) {}

/// @brief Get the content of a cell.
/// @param x the x coordinate of the cell.
/// @param y the y coordinate of the cell.
Cell Canvas::GetCell(int x, int y) const {
  auto it = storage_.find(XY{x, y});
  return (it == storage_.end()) ? Cell() : it->second.content;
}

/// @brief Draw a braille dot.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
/// @param value whether the dot is filled or not.
void Canvas::DrawPoint(int x, int y, bool value) {
  DrawPoint(x, y, value, [](Cell& /*pixel*/) {});
}

/// @brief Draw a braille dot.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
/// @param value whether the dot is filled or not.
/// @param color the color of the dot.
void Canvas::DrawPoint(int x, int y, bool value, const Color& color) {
  DrawPoint(x, y, value, [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a braille dot.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
/// @param value whether the dot is filled or not.
/// @param style the style of the cell.
void Canvas::DrawPoint(int x, int y, bool value, const Stylizer& style) {
  Style(x, y, style);
  if (value) {
    DrawPointOn(x, y);
  } else {
    DrawPointOff(x, y);
  }
}

/// @brief Draw a braille dot.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
void Canvas::DrawPointOn(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  if (cell.type != CellType::kBraille) {
    cell.content.character = "⠀";  // 3 bytes.
    cell.type = CellType::kBraille;
  }

  cell.content.character[1] |= g_map_braille[x % 2][y % 4][0];  // NOLINT
  cell.content.character[2] |= g_map_braille[x % 2][y % 4][1];  // NOLINT
}

/// @brief Erase a braille dot.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
void Canvas::DrawPointOff(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  if (cell.type != CellType::kBraille) {
    cell.content.character = "⠀";  // 3 byt
    cell.type = CellType::kBraille;
  }

  cell.content.character[1] &= ~(g_map_braille[x % 2][y % 4][0]);  // NOLINT
  cell.content.character[2] &= ~(g_map_braille[x % 2][y % 4][1]);  // NOLINT
}

/// @brief Toggle a braille dot. A filled one will be erased, and the other will
/// be drawn.
/// @param x the x coordinate of the dot.
/// @param y the y coordinate of the dot.
void Canvas::DrawPointToggle(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  if (cell.type != CellType::kBraille) {
    cell.content.character = "⠀";  // 3 byt
    cell.type = CellType::kBraille;
  }

  cell.content.character[1] ^= g_map_braille[x % 2][y % 4][0];  // NOLINT
  cell.content.character[2] ^= g_map_braille[x % 2][y % 4][1];  // NOLINT
}

/// @brief Draw a line made of braille dots.
/// @param x1 the x coordinate of the first dot.
/// @param y1 the y coordinate of the first dot.
/// @param x2 the x coordinate of the second dot.
/// @param y2 the y coordinate of the second dot.
void Canvas::DrawPointLine(int x1, int y1, int x2, int y2) {
  DrawPointLine(x1, y1, x2, y2, [](Cell& /*pixel*/) {});
}

/// @brief Draw a line made of braille dots.
/// @param x1 the x coordinate of the first dot.
/// @param y1 the y coordinate of the first dot.
/// @param x2 the x coordinate of the second dot.
/// @param y2 the y coordinate of the second dot.
/// @param color the color of the line.
void Canvas::DrawPointLine(int x1, int y1, int x2, int y2, const Color& color) {
  DrawPointLine(x1, y1, x2, y2,
                [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a line made of braille dots.
/// @param x1 the x coordinate of the first dot.
/// @param y1 the y coordinate of the first dot.o
/// @param x2 the x coordinate of the second dot.
/// @param y2 the y coordinate of the second dot.
/// @param style the style of the line.
void Canvas::DrawPointLine(int x1,
                           int y1,
                           int x2,
                           int y2,
                           const Stylizer& style) {
  const int dx = std::abs(x2 - x1);
  const int dy = std::abs(y2 - y1);
  const int sx = x1 < x2 ? 1 : -1;
  const int sy = y1 < y2 ? 1 : -1;
  const int length = std::max(dx, dy);

  if (!IsIn(x1, y1) && !IsIn(x2, y2)) {
    return;
  }
  if (dx + dx > width_ * height_) {
    return;
  }

  int error = dx - dy;
  for (int i = 0; i < length; ++i) {
    DrawPoint(x1, y1, true, style);
    if (2 * error >= -dy) {
      error -= dy;
      x1 += sx;
    }
    if (2 * error <= dx) {
      error += dx;
      y1 += sy;
    }
  }
  DrawPoint(x2, y2, true, style);
}

/// @brief Draw a circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
void Canvas::DrawPointCircle(int x, int y, int radius) {
  DrawPointCircle(x, y, radius, [](Cell& /*pixel*/) {});
}

/// @brief Draw a circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param color the color of the circle.
void Canvas::DrawPointCircle(int x, int y, int radius, const Color& color) {
  DrawPointCircle(x, y, radius,
                  [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param style the style of the circle.
void Canvas::DrawPointCircle(int x, int y, int radius, const Stylizer& style) {
  DrawPointEllipse(x, y, radius, radius, style);
}

/// @brief Draw a filled circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
void Canvas::DrawPointCircleFilled(int x, int y, int radius) {
  DrawPointCircleFilled(x, y, radius, [](Cell& /*pixel*/) {});
}

/// @brief Draw a filled circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param color the color of the circle.
void Canvas::DrawPointCircleFilled(int x,
                                   int y,
                                   int radius,
                                   const Color& color) {
  DrawPointCircleFilled(x, y, radius,
                        [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a filled circle made of braille dots.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param style the style of the circle.
void Canvas::DrawPointCircleFilled(int x,
                                   int y,
                                   int radius,
                                   const Stylizer& style) {
  DrawPointEllipseFilled(x, y, radius, radius, style);
}

/// @brief Draw an ellipse made of braille dots.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
void Canvas::DrawPointEllipse(int x, int y, int r1, int r2) {
  DrawPointEllipse(x, y, r1, r2, [](Cell& /*pixel*/) {});
}

/// @brief Draw an ellipse made of braille dots.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param color the color of the ellipse.
void Canvas::DrawPointEllipse(int x,
                              int y,
                              int r1,
                              int r2,
                              const Color& color) {
  DrawPointEllipse(x, y, r1, r2,
                   [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw an ellipse made of braille dots.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param s the style of the ellipse.
void Canvas::DrawPointEllipse(int x1,
                              int y1,
                              int r1,
                              int r2,
                              const Stylizer& s) {
  int x = -r1;
  int y = 0;
  int e2 = r2;
  int dx = (1 + 2 * x) * e2 * e2;
  int dy = x * x;
  int err = dx + dy;

  do {  // NOLINT
    DrawPoint(x1 - x, y1 + y, true, s);
    DrawPoint(x1 + x, y1 + y, true, s);
    DrawPoint(x1 + x, y1 - y, true, s);
    DrawPoint(x1 - x, y1 - y, true, s);
    e2 = 2 * err;
    if (e2 >= dx) {
      x++;
      err += dx += 2 * r2 * r2;
    }
    if (e2 <= dy) {
      y++;
      err += dy += 2 * r1 * r1;
    }
  } while (x <= 0);

  while (y++ < r2) {
    DrawPoint(x1, y1 + y, true, s);
    DrawPoint(x1, y1 - y, true, s);
  }
}

/// @brief Draw a filled ellipse made of braille dots.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
void Canvas::DrawPointEllipseFilled(int x1, int y1, int r1, int r2) {
  DrawPointEllipseFilled(x1, y1, r1, r2, [](Cell& /*pixel*/) {});
}

/// @brief Draw a filled ellipse made of braille dots.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param color the color of the ellipse.
void Canvas::DrawPointEllipseFilled(int x1,
                                    int y1,
                                    int r1,
                                    int r2,
                                    const Color& color) {
  DrawPointEllipseFilled(x1, y1, r1, r2,
                         [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a filled ellipse made of braille dots.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param s the style of the ellipse.
void Canvas::DrawPointEllipseFilled(int x1,
                                    int y1,
                                    int r1,
                                    int r2,
                                    const Stylizer& s) {
  int x = -r1;
  int y = 0;
  int e2 = r2;
  int dx = (1 + 2 * x) * e2 * e2;
  int dy = x * x;
  int err = dx + dy;

  do {  // NOLINT
    for (int xx = x1 + x; xx <= x1 - x; ++xx) {
      DrawPoint(xx, y1 + y, true, s);
      DrawPoint(xx, y1 - y, true, s);
    }
    e2 = 2 * err;
    if (e2 >= dx) {
      x++;
      err += dx += 2 * r2 * r2;
    }
    if (e2 <= dy) {
      y++;
      err += dy += 2 * r1 * r1;
    }
  } while (x <= 0);

  while (y++ < r2) {
    for (int yy = y1 - y; yy <= y1 + y; ++yy) {
      DrawPoint(x1, yy, true, s);
    }
  }
}

/// @brief Draw a block.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
/// @param value whether the block is filled or not.
void Canvas::DrawBlock(int x, int y, bool value) {
  DrawBlock(x, y, value, [](Cell& /*pixel*/) {});
}

/// @brief Draw a block.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
/// @param value whether the block is filled or not.
/// @param color the color of the block.
void Canvas::DrawBlock(int x, int y, bool value, const Color& color) {
  DrawBlock(x, y, value, [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a block.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
/// @param value whether the block is filled or not.
/// @param style the style of the block.
void Canvas::DrawBlock(int x, int y, bool value, const Stylizer& style) {
  Style(x, y, style);
  if (value) {
    DrawBlockOn(x, y);
  } else {
    DrawBlockOff(x, y);
  }
}

/// @brief Draw a block.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
void Canvas::DrawBlockOn(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  y /= 2;
  CanvasCell& cell = storage_[XY{x / 2, y / 2}];
  if (cell.type != CellType::kBlock) {
    cell.content.character = " ";
    cell.type = CellType::kBlock;
  }

  const uint8_t bit = (x % 2) * 2 + y % 2;
  uint8_t value = g_map_block_inversed.at(cell.content.character);
  value |= 1U << bit;
  cell.content.character = g_map_block[value];
}

/// @brief Erase a block.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
void Canvas::DrawBlockOff(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  if (cell.type != CellType::kBlock) {
    cell.content.character = " ";
    cell.type = CellType::kBlock;
  }
  y /= 2;

  const uint8_t bit = (y % 2) * 2 + x % 2;
  uint8_t value = g_map_block_inversed.at(cell.content.character);
  value &= ~(1U << bit);
  cell.content.character = g_map_block[value];
}

/// @brief Toggle a block. If it is filled, it will be erased. If it is empty,
/// it will be filled.
/// @param x the x coordinate of the block.
/// @param y the y coordinate of the block.
void Canvas::DrawBlockToggle(int x, int y) {
  if (!IsIn(x, y)) {
    return;
  }
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  if (cell.type != CellType::kBlock) {
    cell.content.character = " ";
    cell.type = CellType::kBlock;
  }
  y /= 2;

  const uint8_t bit = (y % 2) * 2 + x % 2;
  uint8_t value = g_map_block_inversed.at(cell.content.character);
  value ^= 1U << bit;
  cell.content.character = g_map_block[value];
}

/// @brief Draw a line made of block characters.
/// @param x1 the x coordinate of the first point of the line.
/// @param y1 the y coordinate of the first point of the line.
/// @param x2 the x coordinate of the second point of the line.
/// @param y2 the y coordinate of the second point of the line.
void Canvas::DrawBlockLine(int x1, int y1, int x2, int y2) {
  DrawBlockLine(x1, y1, x2, y2, [](Cell& /*pixel*/) {});
}

/// @brief Draw a line made of block characters.
/// @param x1 the x coordinate of the first point of the line.
/// @param y1 the y coordinate of the first point of the line.
/// @param x2 the x coordinate of the second point of the line.
/// @param y2 the y coordinate of the second point of the line.
/// @param color the color of the line.
void Canvas::DrawBlockLine(int x1, int y1, int x2, int y2, const Color& color) {
  DrawBlockLine(x1, y1, x2, y2,
                [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a line made of block characters.
/// @param x1 the x coordinate of the first point of the line.
/// @param y1 the y coordinate of the first point of the line.
/// @param x2 the x coordinate of the second point of the line.
/// @param y2 the y coordinate of the second point of the line.
/// @param style the style of the line.
void Canvas::DrawBlockLine(int x1,
                           int y1,
                           int x2,
                           int y2,
                           const Stylizer& style) {
  y1 /= 2;
  y2 /= 2;

  const int dx = std::abs(x2 - x1);
  const int dy = std::abs(y2 - y1);
  const int sx = x1 < x2 ? 1 : -1;
  const int sy = y1 < y2 ? 1 : -1;
  const int length = std::max(dx, dy);

  if (!IsIn(x1, y1) && !IsIn(x2, y2)) {
    return;
  }
  if (dx + dx > width_ * height_) {
    return;
  }

  int error = dx - dy;
  for (int i = 0; i < length; ++i) {
    DrawBlock(x1, y1 * 2, true, style);
    if (2 * error >= -dy) {
      error -= dy;
      x1 += sx;
    }
    if (2 * error <= dx) {
      error += dx;
      y1 += sy;
    }
  }
  DrawBlock(x2, y2 * 2, true, style);
}

/// @brief Draw a circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
void Canvas::DrawBlockCircle(int x, int y, int radius) {
  DrawBlockCircle(x, y, radius, nostyle);
}

/// @brief Draw a circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param color the color of the circle.
void Canvas::DrawBlockCircle(int x, int y, int radius, const Color& color) {
  DrawBlockCircle(x, y, radius,
                  [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param style the style of the circle.
void Canvas::DrawBlockCircle(int x, int y, int radius, const Stylizer& style) {
  DrawBlockEllipse(x, y, radius, radius, style);
}

/// @brief Draw a filled circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
void Canvas::DrawBlockCircleFilled(int x, int y, int radius) {
  DrawBlockCircleFilled(x, y, radius, nostyle);
}

/// @brief Draw a filled circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param color the color of the circle.
void Canvas::DrawBlockCircleFilled(int x,
                                   int y,
                                   int radius,
                                   const Color& color) {
  DrawBlockCircleFilled(x, y, radius,
                        [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a filled circle made of block characters.
/// @param x the x coordinate of the center of the circle.
/// @param y the y coordinate of the center of the circle.
/// @param radius the radius of the circle.
/// @param s the style of the circle.
void Canvas::DrawBlockCircleFilled(int x,
                                   int y,
                                   int radius,
                                   const Stylizer& s) {
  DrawBlockEllipseFilled(x, y, radius, radius, s);
}

/// @brief Draw an ellipse made of block characters.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
void Canvas::DrawBlockEllipse(int x, int y, int r1, int r2) {
  DrawBlockEllipse(x, y, r1, r2, nostyle);
}

/// @brief Draw an ellipse made of block characters.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param color the color of the ellipse.
void Canvas::DrawBlockEllipse(int x,
                              int y,
                              int r1,
                              int r2,
                              const Color& color) {
  DrawBlockEllipse(x, y, r1, r2,
                   [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw an ellipse made of block characters.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param s the style of the ellipse.
void Canvas::DrawBlockEllipse(int x1,
                              int y1,
                              int r1,
                              int r2,
                              const Stylizer& s) {
  y1 /= 2;
  r2 /= 2;
  int x = -r1;
  int y = 0;
  int e2 = r2;
  int dx = (1 + 2 * x) * e2 * e2;
  int dy = x * x;
  int err = dx + dy;

  do {  // NOLINT
    DrawBlock(x1 - x, 2 * (y1 + y), true, s);
    DrawBlock(x1 + x, 2 * (y1 + y), true, s);
    DrawBlock(x1 + x, 2 * (y1 - y), true, s);
    DrawBlock(x1 - x, 2 * (y1 - y), true, s);
    e2 = 2 * err;
    if (e2 >= dx) {
      x++;
      err += dx += 2 * r2 * r2;
    }
    if (e2 <= dy) {
      y++;
      err += dy += 2 * r1 * r1;
    }
  } while (x <= 0);

  while (y++ < r2) {
    DrawBlock(x1, 2 * (y1 + y), true, s);
    DrawBlock(x1, 2 * (y1 - y), true, s);
  }
}

/// @brief Draw a filled ellipse made of block characters.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
void Canvas::DrawBlockEllipseFilled(int x, int y, int r1, int r2) {
  DrawBlockEllipseFilled(x, y, r1, r2, nostyle);
}

/// @brief Draw a filled ellipse made of block characters.
/// @param x the x coordinate of the center of the ellipse.
/// @param y the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param color the color of the ellipse.
void Canvas::DrawBlockEllipseFilled(int x,
                                    int y,
                                    int r1,
                                    int r2,
                                    const Color& color) {
  DrawBlockEllipseFilled(x, y, r1, r2,
                         [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a filled ellipse made of block characters.
/// @param x1 the x coordinate of the center of the ellipse.
/// @param y1 the y coordinate of the center of the ellipse.
/// @param r1 the radius of the ellipse along the x axis.
/// @param r2 the radius of the ellipse along the y axis.
/// @param s the style of the ellipse.
void Canvas::DrawBlockEllipseFilled(int x1,
                                    int y1,
                                    int r1,
                                    int r2,
                                    const Stylizer& s) {
  y1 /= 2;
  r2 /= 2;
  int x = -r1;
  int y = 0;
  int e2 = r2;
  int dx = (1 + 2 * x) * e2 * e2;
  int dy = x * x;
  int err = dx + dy;

  do {  // NOLINT
    for (int xx = x1 + x; xx <= x1 - x; ++xx) {
      DrawBlock(xx, 2 * (y1 + y), true, s);
      DrawBlock(xx, 2 * (y1 - y), true, s);
    }
    e2 = 2 * err;
    if (e2 >= dx) {
      x++;
      err += dx += 2 * r2 * r2;
    }
    if (e2 <= dy) {
      y++;
      err += dy += 2 * r1 * r1;
    }
  } while (x <= 0);

  while (y++ < r2) {
    for (int yy = y1 + y; yy <= y1 - y; ++yy) {
      DrawBlock(x1, 2 * yy, true, s);
    }
  }
}

/// @brief Draw a piece of text.
/// @param x the x coordinate of the text.
/// @param y the y coordinate of the text.
/// @param value the text to draw.
void Canvas::DrawText(int x, int y, std::string_view value) {
  DrawText(x, y, value, nostyle);
}

/// @brief Draw a piece of text.
/// @param x the x coordinate of the text.
/// @param y the y coordinate of the text.
/// @param value the text to draw.
/// @param color the color of the text.
void Canvas::DrawText(int x,
                      int y,
                      std::string_view value,
                      const Color& color) {
  DrawText(x, y, value, [color](Cell& p) { p.foreground_color = color; });
}

/// @brief Draw a piece of text.
/// @param x the x coordinate of the text.
/// @param y the y coordinate of the text.
/// @param value the text to draw.
/// @param style the style of the text.
void Canvas::DrawText(int x,
                      int y,
                      std::string_view value,
                      const Stylizer& style) {
  for (const auto& it : Utf8ToGlyphs(value)) {
    if (!IsIn(x, y)) {
      x += 2;
      continue;
    }
    CanvasCell& cell = storage_[XY{x / 2, y / 4}];
    cell.type = CellType::kCell;
    cell.content.character = it;
    style(cell.content);
    x += 2;
  }
}

/// @brief Directly draw a predefined pixel at the given coordinate
/// @param x the x coordinate of the pixel.
/// @param y the y coordinate of the pixel.
/// @param p the pixel to draw.
void Canvas::DrawCell(int x, int y, const Cell& p) {
  CanvasCell& cell = storage_[XY{x / 2, y / 4}];
  cell.type = CellType::kCell;
  cell.content = p;
}

/// @brief Draw a predefined image, with top-left corner at the given coordinate
///   You can supply negative coordinates to align the image however you like -
///   only the 'visible' portion will be drawn
/// @param x the x coordinate corresponding to the top-left corner of the image.
/// @param y the y coordinate corresponding to the top-left corner of the image.
/// @param image the image to draw.
void Canvas::DrawSurface(int x, int y, const Surface& image) {
  x /= 2;
  y /= 4;
  const int dx_begin = std::max(0, -x);
  const int dy_begin = std::max(0, -y);
  const int dx_end = std::min(image.dimx(), width_ - x);
  const int dy_end = std::min(image.dimy(), height_ - y);

  for (int dy = dy_begin; dy < dy_end; ++dy) {
    for (int dx = dx_begin; dx < dx_end; ++dx) {
      CanvasCell& cell = storage_[XY{
          x + dx,
          y + dy,
      }];
      cell.type = CellType::kCell;
      cell.content = image.CellAt(dx, dy);
    }
  }
}

/// @brief Modify a pixel at a given location.
/// @param x The x-coordinate of the pixel.
/// @param y The y-coordinate of the pixel.
/// @param style a function that modifies the pixel.
void Canvas::Style(int x, int y, const Stylizer& style) {
  if (IsIn(x, y)) {
    style(storage_[XY{x / 2, y / 4}].content);
  }
}

namespace {

class CanvasNodeBase : public Node {
 public:
  CanvasNodeBase() = default;

  void Render(Screen& screen) override {
    const Canvas& c = canvas();
    const int y_max = std::min(c.height() / 4, box_.y_max - box_.y_min + 1);
    const int x_max = std::min(c.width() / 2, box_.x_max - box_.x_min + 1);
    for (int y = 0; y < y_max; ++y) {
      for (int x = 0; x < x_max; ++x) {
        screen.CellAt(box_.x_min + x, box_.y_min + y) = c.GetCell(x, y);
      }
    }
  }

  virtual const Canvas& canvas() = 0;
};

}  // namespace

/// @brief Produce an element from a Canvas, or a reference to a Canvas.
// NOLINTNEXTLINE
Element canvas(ConstRef<Canvas> canvas) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
  class Impl : public CanvasNodeBase {
   public:
    explicit Impl(ConstRef<Canvas> canvas) : canvas_(std::move(canvas)) {
      requirement_.min_x = (canvas_->width() + 1) / 2;
      requirement_.min_y = (canvas_->height() + 3) / 4;
    }
    const Canvas& canvas() final { return *canvas_; }
    ConstRef<Canvas> canvas_;
  };
  return std::make_shared<Impl>(canvas);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

/// @brief Produce an element drawing a canvas of requested size.
/// @param width the width of the canvas.
/// @param height the height of the canvas.
/// @param fn a function drawing the canvas.
Element canvas(int width, int height, std::function<void(Canvas&)> fn) {
  class Impl : public CanvasNodeBase {
   public:
    Impl(int width, int height, std::function<void(Canvas&)> fn)
        : width_(width), height_(height), fn_(std::move(fn)) {}

    void ComputeRequirement() final {
      requirement_.min_x = (width_ + 1) / 2;
      requirement_.min_y = (height_ + 3) / 4;
    }

    void Render(Screen& screen) final {
      const int width = (box_.x_max - box_.x_min + 1) * 2;
      const int height = (box_.y_max - box_.y_min + 1) * 4;
      canvas_ = Canvas(width, height);
      fn_(canvas_);
      CanvasNodeBase::Render(screen);
    }

    const Canvas& canvas() final { return canvas_; }
    Canvas canvas_;
    int width_;
    int height_;
    std::function<void(Canvas&)> fn_;
  };
  return std::make_shared<Impl>(width, height, std::move(fn));
}

/// @brief Produce an element drawing a canvas.
/// @param fn a function drawing the canvas.
Element canvas(std::function<void(Canvas&)> fn) {
  const int default_dim = 12;
  return canvas(default_dim, default_dim, std::move(fn));
}

}  // namespace ftxui

// ---- dom\clear_under.cpp -----------------------------------------


namespace ftxui {

namespace {
using ftxui::Screen;

class ClearUnder : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y) = Cell();
        screen.CellAt(x, y).character = " ";  // Consider the Cell written.
      }
    }
    Node::Render(screen);
  }
};
}  // namespace

/// @brief Before drawing |child|, clear the cells below. This is useful in
///        combination with dbox.
/// @see ftxui::dbox
/// @ingroup dom
Element clear_under(Element element) {
  return std::make_shared<ClearUnder>(std::move(element));
}

}  // namespace ftxui

// ---- dom\color_decorator.cpp -------------------------------------


namespace ftxui {

namespace {
class BgColor : public NodeDecorator {
 public:
  BgColor(Element child, Color color)
      : NodeDecorator(std::move(child)), color_(color) {}

  void Render(Screen& screen) override {
    if (color_.IsOpaque()) {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).background_color = color_;
        }
      }
    } else {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          Color& color = screen.CellAt(x, y).background_color;
          color = Color::Blend(color, color_);
        }
      }
    }
    NodeDecorator::Render(screen);
  }

  Color color_;
};

class FgColor : public NodeDecorator {
 public:
  FgColor(Element child, Color color)
      : NodeDecorator(std::move(child)), color_(color) {}

  void Render(Screen& screen) override {
    if (color_.IsOpaque()) {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).foreground_color = color_;
        }
      }
    } else {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          Color& color = screen.CellAt(x, y).foreground_color;
          color = Color::Blend(color, color_);
        }
      }
    }
    NodeDecorator::Render(screen);
  }

  Color color_;
};

}  // namespace

/// @brief Set the foreground color of an element.
/// @param color The color of the output element.
/// @param child The input element.
/// @return The output element colored.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = color(Color::Green, text("Success")),
/// ```
Element color(Color color, Element child) {
  return std::make_shared<FgColor>(std::move(child), color);
}

/// @brief Set the background color of an element.
/// @param color The color of the output element.
/// @param child The input element.
/// @return The output element colored.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = bgcolor(Color::Green, text("Success")),
/// ```
Element bgcolor(Color color, Element child) {
  return std::make_shared<BgColor>(std::move(child), color);
}

/// @brief Decorate using a foreground color.
/// @param c The foreground color to be applied.
/// @return The Decorator applying the color.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = text("red") | color(Color::Red);
/// ```
Decorator color(Color c) {
  return [c](Element child) { return color(c, std::move(child)); };
}

/// @brief Decorate using a background color.
/// @param color The background color to be applied.
/// @return The Decorator applying the color.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = text("red") | bgcolor(Color::Red);
/// ```
Decorator bgcolor(Color color) {
  return [color](Element child) { return bgcolor(color, std::move(child)); };
}

}  // namespace ftxui

// ---- dom\composite_decorator.cpp ---------------------------------



namespace ftxui {

/// @brief Center an element horizontally.
/// @param child The decorated element.
/// @return The centered element.
/// @ingroup dom
Element hcenter(Element child) {
  return hbox(filler(), std::move(child), filler());
}

/// @brief Center an element vertically.
/// @param child The decorated element.
/// @return The centered element.
/// @ingroup dom
Element vcenter(Element child) {
  return vbox(filler(), std::move(child), filler());
}

/// @brief Center an element horizontally and vertically.
/// @param child The decorated element.
/// @return The centered element.
/// @ingroup dom
Element center(Element child) {
  return hcenter(vcenter(std::move(child)));
}

/// @brief Align an element on the right side.
/// @param child The decorated element.
/// @return The right aligned element.
/// @ingroup dom
Element align_right(Element child) {
  return hbox(filler(), std::move(child));
}

}  // namespace ftxui

// ---- dom\dbox.cpp ------------------------------------------------



namespace ftxui {

namespace {
class DBox : public Node {
 public:
  explicit DBox(Elements children) : Node(std::move(children)) {}

  void ComputeRequirement() override {
    requirement_ = Requirement{};
    for (auto& child : children_) {
      child->ComputeRequirement();

      // Extend the min_x and min_y to contain all the children
      requirement_.min_x =
          std::max(requirement_.min_x, child->requirement().min_x);
      requirement_.min_y =
          std::max(requirement_.min_y, child->requirement().min_y);
    }

    // Propagate the focused requirement.
    // We iterate in reverse order because children are rendered from first to
    // last, meaning the last child is on top of the others. We want the
    // top-most child to be prioritized for focus.
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
      auto& child = *it;
      if (requirement_.focused.Prefer(child->requirement().focused)) {
        requirement_.focused = child->requirement().focused;
      }
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    for (auto& child : children_) {
      child->SetBox(box);
    }
  }
};
}  // namespace

/// @brief Stack several element on top of each other.
/// @param children_ The input elements.
/// @return The right aligned element.
/// @ingroup dom
Element dbox(Elements children_) {
  return std::make_shared<DBox>(std::move(children_));
}

}  // namespace ftxui

// ---- dom\dim.cpp -------------------------------------------------


namespace ftxui {

namespace {
class Dim : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    Node::Render(screen);
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).dim = true;
      }
    }
  }
};
}  // namespace

/// @brief Use a light font, for elements with less emphasis.
/// @ingroup dom
Element dim(Element child) {
  return std::make_shared<Dim>(std::move(child));
}

}  // namespace ftxui

// ---- dom\flex.cpp ------------------------------------------------


namespace ftxui {

namespace {

class Flex : public Node {
 public:
  Flex(int grow_x, int grow_y, int shrink_x, int shrink_y)
      : grow_x_(grow_x),
        grow_y_(grow_y),
        shrink_x_(shrink_x),
        shrink_y_(shrink_y) {}
  Flex(Element child, int grow_x, int grow_y, int shrink_x, int shrink_y)
      : Node(unpack(std::move(child))),
        grow_x_(grow_x),
        grow_y_(grow_y),
        shrink_x_(shrink_x),
        shrink_y_(shrink_y) {}

  void ComputeRequirement() override {
    requirement_.min_x = 0;
    requirement_.min_y = 0;
    if (!children_.empty()) {
      children_[0]->ComputeRequirement();
      requirement_ = children_[0]->requirement();
    }
    if (grow_x_ != -1) {
      requirement_.flex_grow_x = grow_x_;
    }
    if (grow_y_ != -1) {
      requirement_.flex_grow_y = grow_y_;
    }
    if (shrink_x_ != -1) {
      requirement_.flex_shrink_x = shrink_x_;
    }
    if (shrink_y_ != -1) {
      requirement_.flex_shrink_y = shrink_y_;
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);
    if (children_.empty()) {
      return;
    }
    children_[0]->SetBox(box);
  }

  int grow_x_;
  int grow_y_;
  int shrink_x_;
  int shrink_y_;
};

}  // namespace

/// @brief An element that will expand proportionally to the space left in
/// a container.
/// @ingroup dom
Element filler() {
  return std::make_shared<Flex>(1, 1, 1, 1);
}

/// @brief Make a child element to expand proportionally to the space left in a
/// container.
/// @ingroup dom
///
/// #### Examples:
///
/// ~~~cpp
///   hbox({
///     text("left") | border ,
///     text("middle") | border | flex,
///     text("right") | border,
///   });
/// ~~~
///
/// #### Output:
///
/// ~~~bash
/// ┌────┐┌─────────────────────────────────────────────────────────┐┌─────┐
/// │left││middle                                                   ││right│
/// └────┘└─────────────────────────────────────────────────────────┘└─────┘
/// ~~~
Element flex(Element child) {
  return std::make_shared<Flex>(std::move(child), 1, 1, 1, 1);
}

/// @brief Expand/Minimize if possible/needed on the X axis.
/// @ingroup dom
Element xflex(Element child) {
  return std::make_shared<Flex>(std::move(child), 1, -1, 1, -1);
}

/// @brief Expand/Minimize if possible/needed on the Y axis.
/// @ingroup dom
Element yflex(Element child) {
  return std::make_shared<Flex>(std::move(child), -1, 1, -1, 1);
}

/// @brief Expand if possible.
/// @ingroup dom
Element flex_grow(Element child) {
  return std::make_shared<Flex>(std::move(child), 1, 1, -1, -1);
}

/// @brief Expand if possible on the X axis.
/// @ingroup dom
Element xflex_grow(Element child) {
  return std::make_shared<Flex>(std::move(child), 1, -1, -1, -1);
}

/// @brief Expand if possible on the Y axis.
/// @ingroup dom
Element yflex_grow(Element child) {
  return std::make_shared<Flex>(std::move(child), -1, 1, -1, -1);
}

/// @brief Minimize if needed.
/// @ingroup dom
Element flex_shrink(Element child) {
  return std::make_shared<Flex>(std::move(child), -1, -1, 1, 1);
}

/// @brief Minimize if needed on the X axis.
/// @ingroup dom
Element xflex_shrink(Element child) {
  return std::make_shared<Flex>(std::move(child), -1, -1, 1, -1);
}

/// @brief Minimize if needed on the Y axis.
/// @ingroup dom
Element yflex_shrink(Element child) {
  return std::make_shared<Flex>(std::move(child), -1, -1, -1, 1);
}

/// @brief Make the element not flexible.
/// @ingroup dom
Element notflex(Element child) {
  return std::make_shared<Flex>(std::move(child), 0, 0, 0, 0);
}

Decorator flex_factor(int grow, int shrink) {
  return [grow, shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), grow, grow, shrink, shrink);
  };
}

Decorator flex_grow_factor(int grow) {
  return [grow](Element child) {
    return std::make_shared<Flex>(std::move(child), grow, grow, -1, -1);
  };
}

Decorator flex_shrink_factor(int shrink) {
  return [shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), -1, -1, shrink, shrink);
  };
}

Decorator xflex_factor(int grow, int shrink) {
  return [grow, shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), grow, -1, shrink, -1);
  };
}

Decorator xflex_grow_factor(int grow) {
  return [grow](Element child) {
    return std::make_shared<Flex>(std::move(child), grow, -1, -1, -1);
  };
}

Decorator xflex_shrink_factor(int shrink) {
  return [shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), -1, -1, shrink, -1);
  };
}

Decorator yflex_factor(int grow, int shrink) {
  return [grow, shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), -1, grow, -1, shrink);
  };
}

Decorator yflex_grow_factor(int grow) {
  return [grow](Element child) {
    return std::make_shared<Flex>(std::move(child), -1, grow, -1, -1);
  };
}

Decorator yflex_shrink_factor(int shrink) {
  return [shrink](Element child) {
    return std::make_shared<Flex>(std::move(child), -1, -1, -1, shrink);
  };
}

}  // namespace ftxui

// ---- dom\flexbox.cpp ---------------------------------------------



namespace ftxui::flexbox_helper {

// A block is a rectangle in the flexbox.
struct Block {
  // Input:
  int min_size_x = 0;
  int min_size_y = 0;
  int flex_grow_x = 0;
  int flex_grow_y = 0;
  int flex_shrink_x = 0;
  int flex_shrink_y = 0;

  // Output:
  int line{};
  int line_position{};
  int x = 0;
  int y = 0;
  int dim_x = 0;
  int dim_y = 0;
  bool overflow = false;
};

// A line is a row of blocks.
struct Line {
  std::vector<Block*> blocks;
  int x = 0;
  int y = 0;
  int dim_x = 0;
  int dim_y = 0;
};

struct Global {
  std::vector<Block> blocks;
  std::vector<Line> lines;
  FlexboxConfig config;
  int size_x;
  int size_y;
};

void Compute(Global& global);

}  // namespace ftxui::flexbox_helper


namespace ftxui {

namespace {
void Normalize(FlexboxConfig::Direction& direction) {
  switch (direction) {
    case FlexboxConfig::Direction::Row:
    case FlexboxConfig::Direction::RowInversed: {
      direction = FlexboxConfig::Direction::Row;
    } break;
    case FlexboxConfig::Direction::Column:
    case FlexboxConfig::Direction::ColumnInversed: {
      direction = FlexboxConfig::Direction::Column;
    } break;
  }
}

void Normalize(FlexboxConfig::AlignContent& align_content) {
  align_content = FlexboxConfig::AlignContent::FlexStart;
}

void Normalize(FlexboxConfig::JustifyContent& justify_content) {
  justify_content = FlexboxConfig::JustifyContent::FlexStart;
}

void Normalize(FlexboxConfig::Wrap& wrap) {
  wrap = FlexboxConfig::Wrap::Wrap;
}

FlexboxConfig Normalize(FlexboxConfig config) {
  Normalize(config.direction);
  Normalize(config.wrap);
  Normalize(config.justify_content);
  Normalize(config.align_content);
  return config;
}

class Flexbox : public Node {
 public:
  Flexbox(Elements children, FlexboxConfig config)
      : Node(std::move(children)),
        config_(config),
        config_normalized_(Normalize(config)) {
    requirement_.flex_grow_x = 1;
    requirement_.flex_grow_y = 0;

    if (IsColumnOriented()) {
      std::swap(requirement_.flex_grow_x, requirement_.flex_grow_y);
    }
  }

  bool IsColumnOriented() const {
    return config_.direction == FlexboxConfig::Direction::Column ||
           config_.direction == FlexboxConfig::Direction::ColumnInversed;
  }

  void Layout(flexbox_helper::Global& global,
              bool compute_requirement = false) {
    global.blocks.reserve(children_.size());
    for (auto& child : children_) {
      flexbox_helper::Block block;
      block.min_size_x = child->requirement().min_x;
      block.min_size_y = child->requirement().min_y;
      if (!compute_requirement) {
        block.flex_grow_x = child->requirement().flex_grow_x;
        block.flex_grow_y = child->requirement().flex_grow_y;
        block.flex_shrink_x = child->requirement().flex_shrink_x;
        block.flex_shrink_y = child->requirement().flex_shrink_y;
      }
      global.blocks.push_back(block);
    }

    flexbox_helper::Compute(global);
  }

  void ComputeRequirement() override {
    requirement_ = Requirement{};
    for (auto& child : children_) {
      child->ComputeRequirement();
    }
    global_ = flexbox_helper::Global();
    global_.config = config_normalized_;
    if (IsColumnOriented()) {
      global_.size_x = 100000;  // NOLINT
      global_.size_y = asked_;
    } else {
      global_.size_x = asked_;
      global_.size_y = 100000;  // NOLINT
    }
    Layout(global_, true);

    if (global_.blocks.empty()) {
      return;
    }

    // Compute the union of all the blocks:
    Box box;
    box.x_min = global_.blocks[0].x;
    box.y_min = global_.blocks[0].y;
    box.x_max = global_.blocks[0].x + global_.blocks[0].dim_x;
    box.y_max = global_.blocks[0].y + global_.blocks[0].dim_y;
    for (auto& b : global_.blocks) {
      box.x_min = std::min(box.x_min, b.x);
      box.y_min = std::min(box.y_min, b.y);
      box.x_max = std::max(box.x_max, b.x + b.dim_x);
      box.y_max = std::max(box.y_max, b.y + b.dim_y);
    }
    requirement_.min_x = box.x_max - box.x_min;
    requirement_.min_y = box.y_max - box.y_min;

    // Find the selection:
    for (size_t i = 0; i < children_.size(); ++i) {
      if (requirement_.focused.Prefer(children_[i]->requirement().focused)) {
        requirement_.focused = children_[i]->requirement().focused;
        // Shift |focused.box| according to its position inside this component:
        auto& b = global_.blocks[i];
        requirement_.focused.box.Shift(b.x, b.y);
        requirement_.focused.box =
            Box::Intersection(requirement_.focused.box, box);
      }
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    const int asked_previous = asked_;
    asked_ = std::min(asked_, IsColumnOriented() ? box.y_max - box.y_min + 1
                                                 : box.x_max - box.x_min + 1);
    need_iteration_ = (asked_ != asked_previous);

    flexbox_helper::Global global;
    global.config = config_;
    global.size_x = box.x_max - box.x_min + 1;
    global.size_y = box.y_max - box.y_min + 1;
    Layout(global);

    for (size_t i = 0; i < children_.size(); ++i) {
      auto& child = children_[i];
      auto& b = global.blocks[i];

      Box children_box;
      children_box.x_min = box.x_min + b.x;
      children_box.y_min = box.y_min + b.y;
      children_box.x_max = box.x_min + b.x + b.dim_x - 1;
      children_box.y_max = box.y_min + b.y + b.dim_y - 1;

      const Box intersection = Box::Intersection(children_box, box);
      child->SetBox(intersection);

      need_iteration_ |= (intersection != children_box);
    }
  }

  void Select(Selection& selection) override {
    // If this Node box_ doesn't intersect with the selection, then no
    // selection.
    if (Box::Intersection(selection.GetBox(), box_).IsEmpty()) {
      return;
    }

    Selection selection_lines = IsColumnOriented()
                                    ? selection.SaturateVertical(box_)
                                    : selection.SaturateHorizontal(box_);

    size_t i = 0;
    for (auto& line : global_.lines) {
      Box box;
      box.x_min = box_.x_min + line.x;
      box.x_max = box_.x_min + line.x + line.dim_x - 1;
      box.y_min = box_.y_min + line.y;
      box.y_max = box_.y_min + line.y + line.dim_y - 1;

      // If the line box doesn't intersect with the selection, then no
      // selection.
      if (Box::Intersection(selection.GetBox(), box).IsEmpty()) {
        i += line.blocks.size();
        continue;
      }

      Selection selection_line = IsColumnOriented()
                                     ? selection_lines.SaturateHorizontal(box)
                                     : selection_lines.SaturateVertical(box);

      for (auto& block : line.blocks) {
        std::ignore = block;
        children_[i]->Select(selection_line);
        i++;
      }
    }
  }

  void Check(Status* status) override {
    for (auto& child : children_) {
      child->Check(status);
    }

    if (status->iteration == 0) {
      asked_ = 6000;  // NOLINT
      need_iteration_ = true;
    }

    status->need_iteration |= need_iteration_;
  }

  int asked_ = 6000;  // NOLINT
  bool need_iteration_ = true;
  const FlexboxConfig config_;
  const FlexboxConfig config_normalized_;
  flexbox_helper::Global global_;
};

}  // namespace

/// @brief A container displaying elements on row/columns and capable of
/// wrapping on the next column/row when full.
/// @param children The elements in the container
/// @param config The option
/// @return The container.
///
/// #### Example
///
/// ```cpp
/// flexbox({
///   text("element 1"),
///   text("element 2"),
///   text("element 3"),
/// }, FlexboxConfig()
///      .Set(FlexboxConfig::Direction::Column)
///      .Set(FlexboxConfig::Wrap::WrapInversed)
///      .SetGapMainAxis(1)
///      .SetGapCrossAxis(1)
/// )
/// ```
Element flexbox(Elements children, FlexboxConfig config) {
  return std::make_shared<Flexbox>(std::move(children), config);
}

/// @brief A container displaying elements in rows from left to right. When
/// filled, it starts on a new row below.
/// @param children The elements in the container
/// @return The container.
///
/// #### Example
///
/// ```cpp
/// hflow({
///   text("element 1"),
///   text("element 2"),
///   text("element 3"),
/// });
/// ```
Element hflow(Elements children) {
  return flexbox(std::move(children), FlexboxConfig());
}

/// @brief A container displaying elements in rows from top to bottom. When
/// filled, it starts on a new columns on the right.
/// filled, it starts on a new row.
/// is full, it starts a new row.
/// @param children The elements in the container
/// @return The container.
///
/// #### Example
///
/// ```cpp
/// vflow({
///   text("element 1"),
///   text("element 2"),
///   text("element 3"),
/// });
/// ```
Element vflow(Elements children) {
  return flexbox(std::move(children),
                 FlexboxConfig().Set(FlexboxConfig::Direction::Column));
}

}  // namespace ftxui

// ---- dom\flexbox_config.cpp --------------------------------------

namespace ftxui {

/// @brief Set the flexbox direction.
FlexboxConfig& FlexboxConfig::Set(FlexboxConfig::Direction d) {
  this->direction = d;
  return *this;
}

/// @brief Set the flexbox wrap.
FlexboxConfig& FlexboxConfig::Set(FlexboxConfig::Wrap w) {
  this->wrap = w;
  return *this;
}

/// @brief Set the flexbox justify content.
FlexboxConfig& FlexboxConfig::Set(FlexboxConfig::JustifyContent j) {
  this->justify_content = j;
  return *this;
}

/// @brief Set the flexbox align items.
FlexboxConfig& FlexboxConfig::Set(FlexboxConfig::AlignItems a) {
  this->align_items = a;
  return *this;
}

/// @brief Set the flexbox align content.
FlexboxConfig& FlexboxConfig::Set(FlexboxConfig::AlignContent a) {
  this->align_content = a;
  return *this;
}

/// @brief Set the flexbox flex direction.
FlexboxConfig& FlexboxConfig::SetGap(int x, int y) {
  this->gap_x = x;
  this->gap_y = y;
  return *this;
}

}  // namespace ftxui

// ---- dom\flexbox_helper.cpp --------------------------------------



namespace ftxui::flexbox_helper {

namespace {
void SymmetryXY(FlexboxConfig& c) {
  std::swap(c.gap_x, c.gap_y);
  switch (c.direction) {
    case FlexboxConfig::Direction::Row:
      c.direction = FlexboxConfig::Direction::Column;
      break;
    case FlexboxConfig::Direction::RowInversed:
      c.direction = FlexboxConfig::Direction::ColumnInversed;
      break;
    case FlexboxConfig::Direction::Column:
      c.direction = FlexboxConfig::Direction::Row;
      break;
    case FlexboxConfig::Direction::ColumnInversed:
      c.direction = FlexboxConfig::Direction::RowInversed;
      break;
  }
}

void SymmetryX(FlexboxConfig& c) {
  switch (c.direction) {
    case FlexboxConfig::Direction::Row:
      c.direction = FlexboxConfig::Direction::RowInversed;
      break;
    case FlexboxConfig::Direction::RowInversed:
      c.direction = FlexboxConfig::Direction::Row;
      break;
    default:
      break;
  }
}

void SymmetryY(FlexboxConfig& c) {
  switch (c.wrap) {
    case FlexboxConfig::Wrap::NoWrap:
      break;
    case FlexboxConfig::Wrap::Wrap:
      c.wrap = FlexboxConfig::Wrap::WrapInversed;
      break;
    case FlexboxConfig::Wrap::WrapInversed:
      c.wrap = FlexboxConfig::Wrap::Wrap;
      break;
  }
}

void SymmetryXY(Global& g) {
  SymmetryXY(g.config);
  std::swap(g.size_x, g.size_y);
  for (auto& b : g.blocks) {
    std::swap(b.min_size_x, b.min_size_y);
    std::swap(b.flex_grow_x, b.flex_grow_y);
    std::swap(b.flex_shrink_x, b.flex_shrink_y);
    std::swap(b.x, b.y);
    std::swap(b.dim_x, b.dim_y);
  }
  for (auto& l : g.lines) {
    std::swap(l.x, l.y);
    std::swap(l.dim_x, l.dim_y);
  }
}

void SymmetryX(Global& g) {
  SymmetryX(g.config);
  for (auto& b : g.blocks) {
    b.x = g.size_x - b.x - b.dim_x;
  }
  for (auto& l : g.lines) {
    l.x = g.size_x - l.x - l.dim_x;
  }
}

void SymmetryY(Global& g) {
  SymmetryY(g.config);
  for (auto& b : g.blocks) {
    b.y = g.size_y - b.y - b.dim_y;
  }
  for (auto& l : g.lines) {
    l.y = g.size_y - l.y - l.dim_y;
  }
}

void SetX(Global& global) {
  for (auto& line : global.lines) {
    std::vector<box_helper::Element> elements;
    elements.reserve(line.blocks.size());
    for (auto* block : line.blocks) {
      box_helper::Element element;
      element.min_size = block->min_size_x;
      element.flex_grow =
          block->flex_grow_x != 0 || global.config.justify_content ==
                                         FlexboxConfig::JustifyContent::Stretch
              ? 1
              : 0;
      element.flex_shrink = block->flex_shrink_x;
      elements.push_back(element);
    }

    box_helper::Compute(
        &elements,
        global.size_x - global.config.gap_x * (int(line.blocks.size()) - 1));

    int x = 0;
    for (size_t i = 0; i < line.blocks.size(); ++i) {
      line.blocks[i]->x = x;
      line.blocks[i]->dim_x = elements[i].size;
      x += elements[i].size;
      x += global.config.gap_x;
    }
  }

  for (auto& line : global.lines) {
    line.x = 0;
    line.dim_x = global.size_x;
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void SetY(Global& g) {
  std::vector<box_helper::Element> elements;
  elements.reserve(g.lines.size());
  for (auto& line : g.lines) {
    box_helper::Element element;
    element.flex_shrink = line.blocks.front()->flex_shrink_y;
    element.flex_grow = line.blocks.front()->flex_grow_y;
    for (auto* block : line.blocks) {
      element.min_size = std::max(element.min_size, block->min_size_y);
      element.flex_shrink = std::min(element.flex_shrink, block->flex_shrink_y);
      element.flex_grow = std::min(element.flex_grow, block->flex_grow_y);
    }
    elements.push_back(element);
  }

  // box_helper::Compute(&elements, g.size_y);
  box_helper::Compute(&elements, 10000);  // NOLINT

  // [Align-content]
  std::vector<int> ys(elements.size());
  int y = 0;
  for (size_t i = 0; i < elements.size(); ++i) {
    ys[i] = y;
    y += elements[i].size;
    y += g.config.gap_y;
  }
  int remaining_space = std::max(0, g.size_y - y);
  switch (g.config.align_content) {
    case FlexboxConfig::AlignContent::FlexStart: {
      break;
    }

    case FlexboxConfig::AlignContent::FlexEnd: {
      for (size_t i = 0; i < ys.size(); ++i) {  // NOLINT
        ys[i] += remaining_space;
      }
      break;
    }

    case FlexboxConfig::AlignContent::Center: {
      for (size_t i = 0; i < ys.size(); ++i) {  // NOLINT
        ys[i] += remaining_space / 2;
      }
      break;
    }

    case FlexboxConfig::AlignContent::Stretch: {
      for (int i = static_cast<int>(ys.size()) - 1; i >= 0; --i) {  // NOLINT
        const int shifted = remaining_space * (i + 0) / (i + 1);
        ys[i] += shifted;
        const int consumed = remaining_space - shifted;
        elements[i].size += consumed;
        remaining_space -= consumed;
      }
      break;
    }

    case FlexboxConfig::AlignContent::SpaceBetween: {
      for (int i = static_cast<int>(ys.size()) - 1; i >= 1; --i) {  // NOLINT
        ys[i] += remaining_space;
        remaining_space = remaining_space * (i - 1) / i;
      }
      break;
    }

    case FlexboxConfig::AlignContent::SpaceAround: {
      for (int i = static_cast<int>(ys.size()) - 1; i >= 0; --i) {  // NOLINT
        ys[i] += remaining_space * (2 * i + 1) / (2 * i + 2);
        remaining_space = remaining_space * (2 * i) / (2 * i + 2);
      }
      break;
    }

    case FlexboxConfig::AlignContent::SpaceEvenly: {
      for (int i = static_cast<int>(ys.size()) - 1; i >= 0; --i) {  // NOLINT
        ys[i] += remaining_space * (i + 1) / (i + 2);
        remaining_space = remaining_space * (i + 1) / (i + 2);
      }
      break;
    }
  }

  // [Align items]
  for (size_t i = 0; i < g.lines.size(); ++i) {
    auto& element = elements[i];
    for (auto* block : g.lines[i].blocks) {
      const bool stretch =
          block->flex_grow_y != 0 ||
          g.config.align_content == FlexboxConfig::AlignContent::Stretch;
      const int size =
          stretch ? element.size : std::min(element.size, block->min_size_y);
      switch (g.config.align_items) {
        case FlexboxConfig::AlignItems::FlexStart: {
          block->y = ys[i];
          block->dim_y = size;
          break;
        }

        case FlexboxConfig::AlignItems::Center: {
          block->y = ys[i] + (element.size - size) / 2;
          block->dim_y = size;
          break;
        }

        case FlexboxConfig::AlignItems::FlexEnd: {
          block->y = ys[i] + element.size - size;
          block->dim_y = size;
          break;
        }

        case FlexboxConfig::AlignItems::Stretch: {
          block->y = ys[i];
          block->dim_y = element.size;
          break;
        }
      }
    }
  }

  ys.push_back(g.size_y);
  for (size_t i = 0; i < g.lines.size(); ++i) {
    g.lines[i].y = ys[i];
    g.lines[i].dim_y = ys[i + 1] - ys[i];
  }
}

void JustifyContent(Global& g) {
  for (auto& line : g.lines) {
    Block* last = line.blocks.back();
    int remaining_space = g.size_x - last->x - last->dim_x;
    switch (g.config.justify_content) {
      case FlexboxConfig::JustifyContent::FlexStart:
      case FlexboxConfig::JustifyContent::Stretch:
        break;

      case FlexboxConfig::JustifyContent::FlexEnd: {
        for (auto* block : line.blocks) {
          block->x += remaining_space;
        }
        break;
      }

      case FlexboxConfig::JustifyContent::Center: {
        for (auto* block : line.blocks) {
          block->x += remaining_space / 2;
        }
        break;
      }

      case FlexboxConfig::JustifyContent::SpaceBetween: {
        for (int i = (int)line.blocks.size() - 1; i >= 1; --i) {
          line.blocks[i]->x += remaining_space;
          remaining_space = remaining_space * (i - 1) / i;
        }
        break;
      }

      case FlexboxConfig::JustifyContent::SpaceAround: {
        for (int i = (int)line.blocks.size() - 1; i >= 0; --i) {
          line.blocks[i]->x += remaining_space * (2 * i + 1) / (2 * i + 2);
          remaining_space = remaining_space * (2 * i) / (2 * i + 2);
        }
        break;
      }

      case FlexboxConfig::JustifyContent::SpaceEvenly: {
        for (int i = (int)line.blocks.size() - 1; i >= 0; --i) {
          line.blocks[i]->x += remaining_space * (i + 1) / (i + 2);
          remaining_space = remaining_space * (i + 1) / (i + 2);
        }
        break;
      }
    }
  }
}

void Compute1(Global& global);
void Compute2(Global& global);
void Compute3(Global& global);

void Compute1(Global& global) {
  if (global.config.direction == FlexboxConfig::Direction::RowInversed) {
    SymmetryX(global);
    Compute2(global);
    SymmetryX(global);
    return;
  }
  Compute2(global);
}

void Compute2(Global& global) {
  if (global.config.wrap == FlexboxConfig::Wrap::WrapInversed) {
    SymmetryY(global);
    Compute3(global);
    SymmetryY(global);
    return;
  }
  Compute3(global);
}

void Compute3(Global& global) {
  // Step 1: Lay out every elements into rows:
  {
    Line line;
    int x = 0;
    for (auto& block : global.blocks) {
      // Does it fit the end of the row?
      // No? Then we need to start a new one:
      if (x + block.min_size_x > global.size_x) {
        x = 0;
        if (!line.blocks.empty()) {
          global.lines.push_back(std::move(line));
        }
        line = Line();
      }

      block.line = static_cast<int>(global.lines.size());
      block.line_position = static_cast<int>(line.blocks.size());
      line.blocks.push_back(&block);
      x += block.min_size_x + global.config.gap_x;
    }
    if (!line.blocks.empty()) {
      global.lines.push_back(std::move(line));
    }
  }

  // Step 2: Set positions on the X axis.
  SetX(global);
  JustifyContent(global);  // Distribute remaining space.

  // Step 3: Set positions on the Y axis.
  SetY(global);
}

}  // namespace

void Compute(Global& global) {
  if (global.config.direction == FlexboxConfig::Direction::Column ||
      global.config.direction == FlexboxConfig::Direction::ColumnInversed) {
    SymmetryXY(global);
    Compute1(global);
    SymmetryXY(global);
    return;
  }
  Compute1(global);
}

}  // namespace ftxui::flexbox_helper

// ---- dom\focus.cpp -----------------------------------------------


namespace ftxui {

/// @brief Used inside a `frame`, this force the view to be scrolled toward a
/// a given position. The position is expressed in proportion of the requested
/// size.
///
/// For instance:
/// - (0, 0) means that the view is scrolled toward the upper left.
/// - (1, 0) means that the view is scrolled toward the upper right.
/// - (0, 1) means that the view is scrolled toward the bottom left.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = huge_document()
///   | focusPositionRelative(0.f, 1.f)
///   | frame;
/// ```
Decorator focusPositionRelative(float x, float y) {
  class Impl : public NodeDecorator {
   public:
    Impl(Element child, float x, float y)
        : NodeDecorator(std::move(child)), x_(x), y_(y) {}

    void ComputeRequirement() override {
      NodeDecorator::ComputeRequirement();
      requirement_.focused.enabled = true;
      requirement_.focused.node = this;
      requirement_.focused.box.x_min = int(float(requirement_.min_x) * x_);
      requirement_.focused.box.y_min = int(float(requirement_.min_y) * y_);
      requirement_.focused.box.x_max = int(float(requirement_.min_x) * x_);
      requirement_.focused.box.y_max = int(float(requirement_.min_y) * y_);
    }

   private:
    const float x_;
    const float y_;
  };

  return [x, y](Element child) {
    return std::make_shared<Impl>(std::move(child), x, y);
  };
}

/// @brief Used inside a `frame`, this force the view to be scrolled toward a
/// a given position. The position is expressed in the numbers of cells.
///
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document = huge_document()
///   | focusPosition(10, 10)
///   | frame;
/// ```
Decorator focusPosition(int x, int y) {
  class Impl : public NodeDecorator {
   public:
    Impl(Element child, int x, int y)
        : NodeDecorator(std::move(child)), x_(x), y_(y) {}

    void ComputeRequirement() override {
      NodeDecorator::ComputeRequirement();
      requirement_.focused.enabled = false;

      Box& box = requirement_.focused.box;
      box.x_min = x_;
      box.y_min = y_;
      box.x_max = x_;
      box.y_max = y_;
    }

   private:
    const int x_;
    const int y_;
  };

  return [x, y](Element child) {
    return std::make_shared<Impl>(std::move(child), x, y);
  };
}

}  // namespace ftxui

// ---- dom\frame.cpp -----------------------------------------------


namespace ftxui {

namespace {
class FocusNode : public Node {
 public:
  explicit FocusNode(Elements children) : Node(std::move(children)) {}

  void ComputeRequirement() override {
    Node::ComputeRequirement();
    requirement_ = children_[0]->requirement();
    requirement_.focused.enabled = true;
    requirement_.focused.node = this;
    requirement_.focused.box.x_min = 0;
    requirement_.focused.box.y_min = 0;
    requirement_.focused.box.x_max = requirement_.min_x - 1;
    requirement_.focused.box.y_max = requirement_.min_y - 1;
  }

  void SetBox(Box box) override {
    Node::SetBox(box);
    children_[0]->SetBox(box);
  }
};

class Frame : public Node {
 public:
  Frame(Elements children, bool x_frame, bool y_frame)
      : Node(std::move(children)), x_frame_(x_frame), y_frame_(y_frame) {}

  void SetBox(Box box) override {
    Node::SetBox(box);
    auto& focused_box = requirement_.focused.box;
    Box children_box = box;

    if (x_frame_) {
      const int external_dimx = box.x_max - box.x_min;
      const int internal_dimx = std::max(requirement_.min_x, external_dimx);
      const int focused_dimx = focused_box.x_max - focused_box.x_min;
      int dx = focused_box.x_min - external_dimx / 2 + focused_dimx / 2;
      dx = std::max(0, std::min(internal_dimx - external_dimx - 1, dx));
      children_box.x_min = box.x_min - dx;
      children_box.x_max = box.x_min + internal_dimx - dx;
    }

    if (y_frame_) {
      const int external_dimy = box.y_max - box.y_min;
      const int internal_dimy = std::max(requirement_.min_y, external_dimy);
      const int focused_dimy = focused_box.y_max - focused_box.y_min;
      int dy = focused_box.y_min - external_dimy / 2 + focused_dimy / 2;
      dy = std::max(0, std::min(internal_dimy - external_dimy - 1, dy));
      children_box.y_min = box.y_min - dy;
      children_box.y_max = box.y_min + internal_dimy - dy;
    }

    children_[0]->SetBox(children_box);
  }

  void Render(Screen& screen) override {
    const AutoReset<Box> stencil(&screen.stencil,
                                 Box::Intersection(box_, screen.stencil));
    children_[0]->Render(screen);
  }

 private:
  bool x_frame_;
  bool y_frame_;
};

class FocusCursorNode : public FocusNode {
 public:
  FocusCursorNode(Elements children, Screen::Cursor::Shape shape)
      : FocusNode(std::move(children)), shape_(shape) {}

 private:
  void ComputeRequirement() override {
    FocusNode::ComputeRequirement();  // NOLINT
    requirement_.focused.cursor_shape = shape_;
  }
  Screen::Cursor::Shape shape_;
};

}  // namespace

/// @brief Set the `child` to be the one focused among its siblings.
/// @param child The element to be focused.
/// @ingroup dom
Element focus(Element child) {
  return std::make_shared<FocusNode>(unpack(std::move(child)));
}

/// This is deprecated. Use `focus` instead.
/// @brief Set the `child` to be the one focused among its siblings.
/// @param e The element to be focused.
Element select(Element e) {
  return focus(std::move(e));
}

/// @brief Allow an element to be displayed inside a 'virtual' area. It size can
/// be larger than its container. In this case only a smaller portion is
/// displayed. The view is scrollable to make the focused element visible.
/// @see frame
/// @see xframe
/// @see yframe
Element frame(Element child) {
  return std::make_shared<Frame>(unpack(std::move(child)), true, true);
}

/// @brief Same as `frame`, but only on the x-axis.
/// @see frame
/// @see xframe
/// @see yframe
Element xframe(Element child) {
  return std::make_shared<Frame>(unpack(std::move(child)), true, false);
}

/// @brief Same as `frame`, but only on the y-axis.
/// @see frame
/// @see xframe
/// @see yframe
Element yframe(Element child) {
  return std::make_shared<Frame>(unpack(std::move(child)), false, true);
}

/// @brief Same as `focus`, but set the cursor shape to be a still block.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorBlock(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::Block);
}

/// @brief Same as `focus`, but set the cursor shape to be a blinking block.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorBlockBlinking(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::BlockBlinking);
}

/// @brief Same as `focus`, but set the cursor shape to be a still block.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorBar(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::Bar);
}

/// @brief Same as `focus`, but set the cursor shape to be a blinking bar.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorBarBlinking(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::BarBlinking);
}

/// @brief Same as `focus`, but set the cursor shape to be a still underline.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorUnderline(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::Underline);
}

/// @brief Same as `focus`, but set the cursor shape to be a blinking underline.
/// @see focus
/// @see focusCursorBlock
/// @see focusCursorBlockBlinking
/// @see focusCursorBar
/// @see focusCursorBarBlinking
/// @see focusCursorUnderline
/// @see focusCursorUnderlineBlinking
/// @ingroup dom
Element focusCursorUnderlineBlinking(Element child) {
  return std::make_shared<FocusCursorNode>(unpack(std::move(child)),
                                           Screen::Cursor::UnderlineBlinking);
}

}  // namespace ftxui

// ---- dom\gauge.cpp -----------------------------------------------



namespace ftxui {

namespace {
// NOLINTNEXTLINE
static const std::string charset_horizontal[11] = {
    " ", " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█",
    // An extra character in case when the fuzzer manage to have:
    // int(9 * (limit - limit_int) = 9
    "█"};

// NOLINTNEXTLINE
static const std::string charset_horizontal_microsoft[11] = {
    " ", " ", " ", " ", "▌", "▌", "▌", "█", "█", "█",
    // An extra character in case when the fuzzer manage to have:
    // int(9 * (limit - limit_int) = 9
    "█"};

// NOLINTNEXTLINE
static const std::string charset_vertical[10] = {
    "█",
    "▇",
    "▆",
    "▅",
    "▄",
    "▃",
    "▂",
    "▁",
    " ",
    // An extra character in case when the fuzzer manage to have:
    // int(8 * (limit - limit_int) = 8
    " ",
};

class Gauge : public Node {
 public:
  Gauge(float progress, Direction direction, std::vector<std::string> charset = {})
      : progress_(progress),
        direction_(direction),
        charset_(std::move(charset)) {
    // This handle NAN correctly:
    if (!(progress_ > 0.F)) {
      progress_ = 0.F;
    }
    if (!(progress_ < 1.F)) {
      progress_ = 1.F;
    }
  }

  void ComputeRequirement() override {
    switch (direction_) {
      case Direction::Right:
      case Direction::Left:
        requirement_.flex_grow_x = 1;
        requirement_.flex_grow_y = 0;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_shrink_y = 0;
        break;
      case Direction::Up:
      case Direction::Down:
        requirement_.flex_grow_x = 0;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_x = 0;
        requirement_.flex_shrink_y = 1;
        break;
    }
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(Screen& screen) override {
    switch (direction_) {
      case Direction::Right:
        RenderHorizontal(screen, /*invert=*/false);
        break;
      case Direction::Up:
        RenderVertical(screen, /*invert=*/false);
        break;
      case Direction::Left:
        RenderHorizontal(screen, /*invert=*/true);
        break;
      case Direction::Down:
        RenderVertical(screen, /*invert=*/true);
        break;
    }
  }

  void RenderHorizontal(Screen& screen, bool invert) {
    if (box_.y_min > box_.y_max) {
      return;
    }

    // `full` is the index of the "full" glyph in `charset`; the boundary
    // cell picks glyph index int(full * fractional_fill).
    const std::string* charset;
    int full;
    if (charset_.empty()) {
      charset = Terminal::GetQuirks().BlockCharacters()  // NOLINT
                    ? charset_horizontal
                    : charset_horizontal_microsoft;
      full = 9;
    } else {
      charset = charset_.data();
      full = static_cast<int>(charset_.size()) - 1;
    }

    // Draw the progress bar horizontally across the full allocated height:
    const float progress = invert ? 1.F - progress_ : progress_;
    const auto limit =
        float(box_.x_min) + progress * float(box_.x_max - box_.x_min + 1);
    const int limit_int = static_cast<int>(limit);

    for (int y = box_.y_min; y <= box_.y_max; y++) {
      int x = box_.x_min;
      while (x < limit_int) {
        screen.at(x++, y) = charset[full];
      }
      if (x <= box_.x_max) {
        screen.at(x++, y) = charset[int(full * (limit - limit_int))];
      }
      while (x <= box_.x_max) {
        screen.at(x++, y) = charset[0];
      }
    }

    if (invert) {
      Invert(screen);
    }
  }

  void RenderVertical(Screen& screen, bool invert) {
    if (box_.x_min > box_.x_max) {
      return;
    }

    const std::string* charset;
    int full;
    if (charset_.empty()) {
      charset = charset_vertical;
      full = 8;
    } else {
      charset = charset_.data();
      full = static_cast<int>(charset_.size()) - 1;
    }

    // Draw the progress bar vertically across the full allocated width:
    const float progress = invert ? progress_ : 1.F - progress_;
    const float limit =
        float(box_.y_min) + progress * float(box_.y_max - box_.y_min + 1);
    const int limit_int = static_cast<int>(limit);

    for (int x = box_.x_min; x <= box_.x_max; x++) {
      int y = box_.y_min;
      while (y < limit_int) {
        screen.at(x, y++) = charset[full];
      }
      if (y <= box_.y_max) {
        screen.at(x, y++) = charset[int(full * (limit - limit_int))];
      }
      while (y <= box_.y_max) {
        screen.at(x, y++) = charset[0];
      }
    }
    if (invert) {
      Invert(screen);
    }
  }

  void Invert(Screen& screen) {
    for (int y = box_.y_min; y <= box_.y_max; y++) {
      for (int x = box_.x_min; x <= box_.x_max; x++) {
        screen.CellAt(x, y).inverted ^= true;
      }
    }
  }

 private:
  float progress_;
  Direction direction_;
  std::vector<std::string> charset_;
};

}  // namespace

/// @brief Draw a high definition progress bar progressing in specified
/// direction.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @param direction Direction of progress bars progression.
/// @ingroup dom
Element gaugeDirection(float progress, Direction direction) {
  return std::make_shared<Gauge>(progress, direction);
}

/// @brief Draw a high definition progress bar progressing from left to right.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @ingroup dom
///
/// ### Example
///
/// A gauge. It can be used to represent a progress bar.
/// ~~~cpp
/// border(gaugeRight(0.5))
/// ~~~
///
/// #### Output
///
/// ~~~bash
/// ┌──────────────────────────────────────────────────────────────────────────┐
/// │█████████████████████████████████████                                     │
/// └──────────────────────────────────────────────────────────────────────────┘
/// ~~~
Element gaugeRight(float progress) {
  return gaugeDirection(progress, Direction::Right);
}

/// @brief Draw a high definition progress bar progressing from right to left.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @ingroup dom
///
/// ### Example
///
/// A gauge. It can be used to represent a progress bar.
/// ~~~cpp
/// border(gaugeLeft(0.5))
/// ~~~
///
/// #### Output
///
/// ~~~bash
/// ┌──────────────────────────────────────────────────────────────────────────┐
/// │                                     █████████████████████████████████████│
/// └──────────────────────────────────────────────────────────────────────────┘
/// ~~~
Element gaugeLeft(float progress) {
  return gaugeDirection(progress, Direction::Left);
}

/// @brief Draw a high definition progress bar progressing from bottom to top.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @ingroup dom
///
/// ### Example
///
/// A gauge. It can be used to represent a progress bar.
/// ~~~cpp
/// border(gaugeUp(0.5))
/// ~~~
///
/// #### Output
///
/// ~~~bash
///  ┌─┐
///  │ │
///  │ │
///  │ │
///  │ │
///  │█│
///  │█│
///  │█│
///  │█│
///  └─┘
/// ~~~
Element gaugeUp(float progress) {
  return gaugeDirection(progress, Direction::Up);
}

/// @brief Draw a high definition progress bar progressing from top to bottom.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @ingroup dom
///
/// ### Example
///
/// A gauge. It can be used to represent a progress bar.
/// ~~~cpp
/// border(gaugeDown(0.5))
/// ~~~
///
/// #### Output
///
/// ~~~bash
///  ┌─┐
///  │█│
///  │█│
///  │█│
///  │█│
///  │ │
///  │ │
///  │ │
///  │ │
///  └─┘
/// ~~~
Element gaugeDown(float progress) {
  return gaugeDirection(progress, Direction::Down);
}

/// @brief Draw a high definition progress bar.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @ingroup dom
///
/// ### Example
///
/// A gauge. It can be used to represent a progress bar.
/// ~~~cpp
/// border(gauge(0.5))
/// ~~~
///
/// #### Output
///
/// ~~~bash
/// ┌──────────────────────────────────────────────────────────────────────────┐
/// │█████████████████████████████████████                                     │
/// └──────────────────────────────────────────────────────────────────────────┘
/// ~~~
Element gauge(float progress) {
  return gaugeRight(progress);
}

/// @brief Draw a high definition progress bar using a custom charset.
/// @param progress The proportion of the area to be filled. Belong to [0,1].
/// @param charset Glyphs from "empty" (index 0) to "full" (last index); a
/// 2-entry charset gives a plain unshaded bar.
/// @param direction Direction of progress bars progression. Defaults to
/// Right.
/// @ingroup dom
///
/// ### Example
///
/// A gauge rendered with a custom charset instead of the default block
/// characters.
/// ~~~cpp
/// border(gaugeCharset(0.5, {".", "#"}))
/// ~~~
///
/// #### Output
///
/// ~~~bash
/// ┌──────────────────────────────────────────────────────────────────────────┐
/// │#####################################.....................................│
/// └──────────────────────────────────────────────────────────────────────────┘
/// ~~~
Element gaugeCharset(float progress,
                      std::vector<std::string> charset,
                      Direction direction) {
  return std::make_shared<Gauge>(progress, direction, std::move(charset));
}

}  // namespace ftxui

// ---- dom\graph.cpp -----------------------------------------------



namespace ftxui {

namespace {
// NOLINTNEXTLINE
static const std::array<std::string, 9> graph_charset = {
    " ", "▗", "▐", "▖", "▄", "▟", "▌", "▙", "█"};

// NOLINTNEXTLINE
static const std::array<std::string, 9> graph_charset_microsoft = {
    " ", " ", "█", " ", "█", "█", "█", "█", "█"};

class Graph : public Node {
 public:
  explicit Graph(GraphFunction graph_function)
      : graph_function_(std::move(graph_function)) {}

  void ComputeRequirement() override {
    requirement_.flex_grow_x = 1;
    requirement_.flex_grow_y = 1;
    requirement_.flex_shrink_x = 1;
    requirement_.flex_shrink_y = 1;
    requirement_.min_x = 3;
    requirement_.min_y = 3;
  }

  void Render(Screen& screen) override {
    const int width = (box_.x_max - box_.x_min + 1) * 2;
    const int height = (box_.y_max - box_.y_min + 1) * 2;
    if (width <= 0 || height <= 0) {
      return;
    }

    const auto& current_graph_charset = Terminal::GetQuirks().BlockCharacters()
                                            ? graph_charset
                                            : graph_charset_microsoft;

    auto data = graph_function_(width, height);
    int i = 0;
    for (int x = box_.x_min; x <= box_.x_max; ++x) {
      const int height_1 = 2 * box_.y_max - data.at(static_cast<size_t>(i++));
      const int height_2 = 2 * box_.y_max - data.at(static_cast<size_t>(i++));
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        const int yy = 2 * y;
        const int i_1 = yy < height_1 ? 0 : yy == height_1 ? 3 : 6;  // NOLINT
        const int i_2 = yy < height_2 ? 0 : yy == height_2 ? 1 : 2;  // NOLINT
        screen.at(x, y) =
            current_graph_charset.at(static_cast<size_t>(i_1 + i_2));  // NOLINT
      }
    }
  }

 private:
  GraphFunction graph_function_;
};

}  // namespace

/// @brief Draw a graph using a GraphFunction.
/// @param graph_function the function to be called to get the data.
Element graph(GraphFunction graph_function) {
  return std::make_shared<Graph>(std::move(graph_function));
}

}  // namespace ftxui

// ---- dom\gridbox.cpp ---------------------------------------------


namespace ftxui {
class Screen;

namespace {

// Accumulate the values of a list U[n] into v[n]. So that:
// V[0] = 0;
// V[n+1] = v[n] + U[n]
// return the sum of U[n].
int Integrate(std::vector<int>& elements) {
  int accu = 0;
  for (auto& i : elements) {
    const int old_accu = accu;
    accu += i;
    i = old_accu;
  }
  return accu;
}

class GridBox : public Node {
 public:
  explicit GridBox(std::vector<Elements> lines) : lines_(std::move(lines)) {
    y_size = static_cast<int>(lines_.size());
    for (const auto& line : lines_) {
      x_size = std::max(x_size, int(line.size()));
    }

    // Fill in empty cells, in case the user did not used the API correctly:
    for (auto& line : lines_) {
      while (line.size() < size_t(x_size)) {
        line.push_back(filler());
      }
    }

    // Add children to properly forward non overridden methods from Node.
    for (auto& line : lines_) {
      for (auto& cell : line) {
        children_.push_back(cell);
      }
    }
  }

  void ComputeRequirement() override {
    requirement_ = Requirement{};
    for (auto& line : lines_) {
      for (auto& cell : line) {
        cell->ComputeRequirement();
      }
    }

    // Compute the size of each columns/row.
    std::vector<int> size_x(x_size, 0);
    std::vector<int> size_y(y_size, 0);
    for (int x = 0; x < x_size; ++x) {
      for (int y = 0; y < y_size; ++y) {
        size_x[x] = std::max(size_x[x], lines_[y][x]->requirement().min_x);
        size_y[y] = std::max(size_y[y], lines_[y][x]->requirement().min_y);
      }
    }

    requirement_.min_x = Integrate(size_x);
    requirement_.min_y = Integrate(size_y);

    // Forward the focused/focused child state:
    for (int x = 0; x < x_size; ++x) {
      for (int y = 0; y < y_size; ++y) {
        if (requirement_.focused.Prefer(lines_[y][x]->requirement().focused)) {
          requirement_.focused = lines_[y][x]->requirement().focused;
          requirement_.focused.box.Shift(size_x[x], size_y[y]);
        }
      }
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    box_helper::Element init;
    init.min_size = 0;
    init.flex_grow = 1024;    // NOLINT
    init.flex_shrink = 1024;  // NOLINT
    std::vector<box_helper::Element> elements_x(x_size, init);
    std::vector<box_helper::Element> elements_y(y_size, init);

    for (int y = 0; y < y_size; ++y) {
      for (int x = 0; x < x_size; ++x) {
        const auto& cell = lines_[y][x];
        const auto& requirement = cell->requirement();
        auto& e_x = elements_x[x];
        auto& e_y = elements_y[y];
        e_x.min_size = std::max(e_x.min_size, requirement.min_x);
        e_y.min_size = std::max(e_y.min_size, requirement.min_y);
        e_x.flex_grow = std::min(e_x.flex_grow, requirement.flex_grow_x);
        e_y.flex_grow = std::min(e_y.flex_grow, requirement.flex_grow_y);
        e_x.flex_shrink = std::min(e_x.flex_shrink, requirement.flex_shrink_x);
        e_y.flex_shrink = std::min(e_y.flex_shrink, requirement.flex_shrink_y);
      }
    }

    const int target_size_x = box.x_max - box.x_min + 1;
    const int target_size_y = box.y_max - box.y_min + 1;
    box_helper::Compute(&elements_x, target_size_x);
    box_helper::Compute(&elements_y, target_size_y);

    Box box_y = box;
    int y = box_y.y_min;
    for (int iy = 0; iy < y_size; ++iy) {
      box_y.y_min = y;
      y += elements_y[iy].size;
      box_y.y_max = y - 1;

      Box box_x = box_y;
      int x = box_x.x_min;
      for (int ix = 0; ix < x_size; ++ix) {
        box_x.x_min = x;
        x += elements_x[ix].size;
        box_x.x_max = x - 1;
        lines_[iy][ix]->SetBox(box_x);
      }
    }
  }

  void Render(Screen& screen) override {
    for (auto& line : lines_) {
      for (auto& cell : line) {
        cell->Render(screen);
      }
    }
  }

  int x_size = 0;
  int y_size = 0;
  std::vector<Elements> lines_;
};
}  // namespace
   //
/// @brief A container displaying a grid of elements.
/// @param lines A list of lines, each line being a list of elements.
/// @return The container.
///
/// #### Example
///
/// ```cpp
/// auto cell = [](const char* t) { return text(t) | border; };
/// auto document = gridbox({
///   {cell("north-west") , cell("north")  , cell("north-east")} ,
///   {cell("west")       , cell("center") , cell("east")}       ,
///   {cell("south-west") , cell("south")  , cell("south-east")} ,
/// });
/// ```
/// Output:
/// ```
/// ╭──────────╮╭──────╮╭──────────╮
/// │north-west││north ││north-east│
/// ╰──────────╯╰──────╯╰──────────╯
/// ╭──────────╮╭──────╮╭──────────╮
/// │west      ││center││east      │
/// ╰──────────╯╰──────╯╰──────────╯
/// ╭──────────╮╭──────╮╭──────────╮
/// │south-west││south ││south-east│
/// ╰──────────╯╰──────╯╰──────────╯
/// ```
Element gridbox(std::vector<Elements> lines) {
  return std::make_shared<GridBox>(std::move(lines));
}

}  // namespace ftxui

// ---- dom\hbox.cpp ------------------------------------------------

namespace ftxui {

namespace {
class HBox : public Node {
 public:
  explicit HBox(Elements children) : Node(std::move(children)) {}

 private:
  void ComputeRequirement() override {
    requirement_ = Requirement{};

    for (auto& child : children_) {
      child->ComputeRequirement();

      // Propagate the focused requirement.
      if (requirement_.focused.Prefer(child->requirement().focused)) {
        requirement_.focused = child->requirement().focused;
        requirement_.focused.box.Shift(requirement_.min_x, 0);
      }

      // Extend the min_x and min_y to contain all the children
      requirement_.min_x += child->requirement().min_x;
      requirement_.min_y =
          std::max(requirement_.min_y, child->requirement().min_y);
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    std::vector<box_helper::Element> elements(children_.size());
    for (size_t i = 0; i < children_.size(); ++i) {
      auto& element = elements[i];
      const auto& requirement = children_[i]->requirement();
      element.min_size = requirement.min_x;
      element.flex_grow = requirement.flex_grow_x;
      element.flex_shrink = requirement.flex_shrink_x;
    }
    const int target_size = box.x_max - box.x_min + 1;
    box_helper::Compute(&elements, target_size);

    int x = box.x_min;
    for (size_t i = 0; i < children_.size(); ++i) {
      box.x_min = x;
      box.x_max = x + elements[i].size - 1;
      children_[i]->SetBox(box);
      x = box.x_max + 1;
    }
  }

  void Select(Selection& selection) override {
    // If this Node box_ doesn't intersect with the selection, then no
    // selection.
    if (Box::Intersection(selection.GetBox(), box_).IsEmpty()) {
      return;
    }

    Selection selection_saturated = selection.SaturateHorizontal(box_);
    for (auto& child : children_) {
      child->Select(selection_saturated);
    }
  }
};

}  // namespace

/// @brief A container displaying elements horizontally one by one.
/// @param children The elements in the container
/// @return The container.
///
/// #### Example
///
/// ```cpp
/// hbox({
///   text("Left"),
///   text("Right"),
/// });
/// ```
Element hbox(Elements children) {
  return std::make_shared<HBox>(std::move(children));
}

}  // namespace ftxui

// ---- dom\hyperlink.cpp -------------------------------------------


namespace ftxui {

namespace {
class Hyperlink : public NodeDecorator {
 public:
  Hyperlink(Element child, std::string link)
      : NodeDecorator(std::move(child)), link_(std::move(link)) {}

  void Render(Screen& screen) override {
    const uint8_t hyperlink_id = screen.RegisterHyperlink(link_);
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).hyperlink = hyperlink_id;
      }
    }
    NodeDecorator::Render(screen);
  }

  std::string link_;
};
}  // namespace

/// @brief Make the rendered area clickable using a web browser.
///        The link will be opened when the user clicks on it.
///        This is supported only on a limited set of terminal emulators.
///        List: https://github.com/Alhadis/OSC8-Adoption/
/// @param link The link
/// @param child The input element.
/// @return The output element with the link.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document =
///   hyperlink("https://github.com/ArthurSonzogni/FTXUI", "link");
/// ```
Element hyperlink(std::string_view link, Element child) {
  return std::make_shared<Hyperlink>(std::move(child), std::string(link));
}

/// @brief Decorate using a hyperlink.
///        The link will be opened when the user clicks on it.
///        This is supported only on a limited set of terminal emulators.
///        List: https://github.com/Alhadis/OSC8-Adoption/
/// @param link The link to redirect the users to.
/// @return The Decorator applying the hyperlink.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// Element document =
///   text("red") | hyperlink("https://github.com/Arthursonzogni/FTXUI");
/// ```
// NOLINTNEXTLINE
Decorator hyperlink(std::string_view link) {
  return [link = std::string(link)](Element child) {
    return hyperlink(link, std::move(child));
  };
}

}  // namespace ftxui

// ---- dom\inverted.cpp --------------------------------------------


namespace ftxui {

namespace {
class Inverted : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    Node::Render(screen);
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).inverted ^= true;
      }
    }
  }
};
}  // namespace

/// @brief Add a filter that will invert the foreground and the background
/// colors.
/// @ingroup dom
Element inverted(Element child) {
  return std::make_shared<Inverted>(std::move(child));
}

}  // namespace ftxui

// ---- dom\italic.cpp ----------------------------------------------


namespace ftxui {

/// @brief Apply a underlinedDouble to text.
/// @ingroup dom
Element italic(Element child) {
  class Impl : public NodeDecorator {
   public:
    using NodeDecorator::NodeDecorator;

    void Render(Screen& screen) override {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).italic = true;
        }
      }
      Node::Render(screen);
    }
  };

  return std::make_shared<Impl>(std::move(child));
}

}  // namespace ftxui

// ---- dom\linear_gradient.cpp -------------------------------------


namespace ftxui {
namespace {

struct LinearGradientNormalized {
  float angle = 0.F;
  std::vector<Color> colors;
  std::vector<float> positions;  // Sorted.
};

// Convert a LinearGradient to a normalized version.
LinearGradientNormalized Normalize(LinearGradient gradient) {
  // Handle gradient of size 0.
  if (gradient.stops.empty()) {
    return LinearGradientNormalized{
        0.F,
        {Color::Default, Color::Default},
        {0.F, 1.F},
    };
  }

  // Fill in the two extent, if not provided.
  if (!gradient.stops.front().position) {
    gradient.stops.front().position = 0.F;
  }
  if (!gradient.stops.back().position) {
    gradient.stops.back().position = 1.F;
  }

  // Fill in the blank, by interpolating positions.
  size_t last_checkpoint = 0;
  for (size_t i = 1; i < gradient.stops.size(); ++i) {
    if (!gradient.stops[i].position) {
      continue;
    }

    if (i - last_checkpoint >= 2) {
      const float min =
          gradient.stops[last_checkpoint].position.value();  // NOLINT
      const float max = gradient.stops[i].position.value();  // NOLINT
      for (size_t j = last_checkpoint + 1; j < i; ++j) {
        gradient.stops[j].position = min + (max - min) *
                                               float(j - last_checkpoint) /
                                               float(i - last_checkpoint);
      }
    }

    last_checkpoint = i;
  }

  // Sort the stops by position.
  std::sort(
      gradient.stops.begin(), gradient.stops.end(),
      [](const auto& a, const auto& b) { return a.position < b.position; });

  // If we don't being with zero, add a stop at zero.
  if (gradient.stops.front().position != 0) {
    gradient.stops.insert(gradient.stops.begin(),
                          {gradient.stops.front().color, 0.F});
  }
  // If we don't end with one, add a stop at one.
  if (gradient.stops.back().position != 1) {
    gradient.stops.push_back({gradient.stops.back().color, 1.F});
  }

  // Normalize the angle.
  LinearGradientNormalized normalized;
  const float modulo = 360.F;
  normalized.angle =
      std::fmod(std::fmod(gradient.angle, modulo) + modulo, modulo);
  for (auto& stop : gradient.stops) {
    normalized.colors.push_back(stop.color);
    // NOLINTNEXTLINE
    normalized.positions.push_back(stop.position.value());
  }
  return normalized;
}

Color Interpolate(const LinearGradientNormalized& gradient, float t) {
  // Find the right color in the gradient's stops.
  size_t i = 1;
  while (true) {
    // Note that `t` might be slightly greater than 1.0 due to floating point
    // precision. This is why we need to handle the case where `t` is greater
    // than the last stop's position.
    // See https://github.com/ArthurSonzogni/FTXUI/issues/998
    if (i >= gradient.positions.size()) {
      const float half = 0.5F;
      return Color::Interpolate(half, gradient.colors.back(),
                                gradient.colors.back());
    }
    if (t <= gradient.positions[i]) {
      break;
    }
    ++i;
  }

  const float t0 = gradient.positions[i - 1];
  const float t1 = gradient.positions[i - 0];
  const float tt = (t - t0) / (t1 - t0);

  const Color& c0 = gradient.colors[i - 1];
  const Color& c1 = gradient.colors[i - 0];
  const Color& cc = Color::Interpolate(tt, c0, c1);

  return cc;
}

class LinearGradientColor : public NodeDecorator {
 public:
  explicit LinearGradientColor(Element child,
                               const LinearGradient& gradient,
                               bool background_color)
      : NodeDecorator(std::move(child)),
        gradient_(Normalize(gradient)),
        background_color_{background_color} {}

 private:
  void Render(Screen& screen) override {
    const float degtorad = 0.01745329251F;
    const float dx = std::cos(gradient_.angle * degtorad);
    const float dy = std::sin(gradient_.angle * degtorad);

    // Project every corner to get the extent of the gradient.
    const float p1 = float(box_.x_min) * dx + float(box_.y_min) * dy;
    const float p2 = float(box_.x_min) * dx + float(box_.y_max) * dy;
    const float p3 = float(box_.x_max) * dx + float(box_.y_min) * dy;
    const float p4 = float(box_.x_max) * dx + float(box_.y_max) * dy;
    const float min = std::min({p1, p2, p3, p4});
    const float max = std::max({p1, p2, p3, p4});

    // Renormalize the projection to [0, 1] using the extent and projective
    // geometry.
    const float dX = dx / (max - min);
    const float dY = dy / (max - min);
    const float dZ = -min / (max - min);

    // Project every pixel to get the color.
    if (background_color_) {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          const float t = float(x) * dX + float(y) * dY + dZ;
          screen.CellAt(x, y).background_color = Interpolate(gradient_, t);
        }
      }
    } else {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          const float t = float(x) * dX + float(y) * dY + dZ;
          screen.CellAt(x, y).foreground_color = Interpolate(gradient_, t);
        }
      }
    }

    NodeDecorator::Render(screen);
  }

  LinearGradientNormalized gradient_;
  bool background_color_;
};

}  // namespace

/// @brief Build the "empty" gradient. This is often followed by calls to
/// LinearGradient::Angle() and LinearGradient::Stop().
/// Example:
/// ```cpp
///  auto gradient =
///   LinearGradient()
///    .Angle(45)
///    .Stop(Color::Red, 0.0)
///    .Stop(Color::Green, 0.5)
///    .Stop(Color::Blue, 1.0);;
/// ```
LinearGradient::LinearGradient() = default;

/// @brief Build a gradient with two colors.
/// @param begin The color at the beginning of the gradient.
/// @param end The color at the end of the gradient.
LinearGradient::LinearGradient(Color begin, Color end)
    : LinearGradient(0, begin, end) {}

/// @brief Build a gradient with two colors and an angle.
/// @param a The angle of the gradient.
/// @param begin The color at the beginning of the gradient.
/// @param end The color at the end of the gradient.
LinearGradient::LinearGradient(float a, Color begin, Color end) : angle(a) {
  stops.push_back({begin, {}});
  stops.push_back({end, {}});
}

/// @brief Set the angle of the gradient.
/// @param a The angle of the gradient.
/// @return The gradient.
LinearGradient& LinearGradient::Angle(float a) {
  angle = a;
  return *this;
}

/// @brief Add a color stop to the gradient.
/// @param c The color of the stop.
/// @param p The position of the stop.
LinearGradient& LinearGradient::Stop(Color c, float p) {
  stops.push_back({c, p});
  return *this;
}

/// @brief Add a color stop to the gradient.
/// @param c The color of the stop.
/// @return The gradient.
/// @note The position of the stop is interpolated from nearby stops.
LinearGradient& LinearGradient::Stop(Color c) {
  stops.push_back({c, {}});
  return *this;
}

/// @brief Set the foreground color of an element with linear-gradient effect.
/// @param gradient The gradient effect to be applied on the output element.
/// @param child The input element.
/// @return The output element colored.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// color(LinearGradient{0, {Color::Red, Color::Blue}}, text("Hello"))
/// ```
Element color(const LinearGradient& gradient, Element child) {
  return std::make_shared<LinearGradientColor>(std::move(child), gradient,
                                               /*background_color*/ false);
}

/// @brief Set the background color of an element with linear-gradient effect.
/// @param gradient The gradient effect to be applied on the output element.
/// @param child The input element.
/// @return The output element colored.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// bgcolor(LinearGradient{0, {Color::Red, Color::Blue}}, text("Hello"))
/// ```
Element bgcolor(const LinearGradient& gradient, Element child) {
  return std::make_shared<LinearGradientColor>(std::move(child), gradient,
                                               /*background_color*/ true);
}

/// @brief Decorate using a linear-gradient effect on the foreground color.
/// @param gradient The gradient effect to be applied on the output element.
/// @return The Decorator applying the color.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// text("Hello") | color(LinearGradient{0, {Color::Red, Color::Blue}})
/// ```
Decorator color(const LinearGradient& gradient) {
  return
      [gradient](Element child) { return color(gradient, std::move(child)); };
}

/// @brief Decorate using a linear-gradient effect on the background color.
/// @param gradient The gradient effect to be applied on the output element.
/// @return The Decorator applying the color.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// text("Hello") | color(LinearGradient{0, {Color::Red, Color::Blue}})
/// ```
Decorator bgcolor(const LinearGradient& gradient) {
  return
      [gradient](Element child) { return bgcolor(gradient, std::move(child)); };
}

}  // namespace ftxui

// ---- dom\node.cpp ------------------------------------------------


namespace ftxui {

Node::Node() = default;
Node::Node(Elements children) : children_(std::move(children)) {}
Node::~Node() = default;

/// @brief Compute how much space an element needs.
void Node::ComputeRequirement() {
  if (children_.empty()) {
    return;
  }
  for (auto& child : children_) {
    child->ComputeRequirement();
  }

  // By default, the requirement is the one of the first child.
  requirement_ = children_[0]->requirement();

  // Propagate the focused requirement.
  for (size_t i = 1; i < children_.size(); ++i) {
    if (requirement_.focused.Prefer(children_[i]->requirement().focused)) {
      requirement_.focused = children_[i]->requirement().focused;
    }
  }
}

/// @brief Assign a position and a dimension to an element for drawing.
void Node::SetBox(Box box) {
  box_ = box;
}

/// @brief Compute the selection of an element.
void Node::Select(Selection& selection) {
  // If this Node box_ doesn't intersect with the selection, then no selection.
  if (Box::Intersection(selection.GetBox(), box_).IsEmpty()) {
    return;
  }

  // By default we defer the selection to the children.
  for (auto& child : children_) {
    child->Select(selection);
  }
}

/// @brief Display an element on a ftxui::Screen.
void Node::Render(Screen& screen) {
  for (auto& child : children_) {
    child->Render(screen);
  }
}

void Node::Check(Status* status) {
  for (auto& child : children_) {
    child->Check(status);
  }
  status->need_iteration |= (status->iteration == 0);
}

std::string Node::GetSelectedContent(Selection& selection) {
  std::string content;

  for (auto& child : children_) {
    content += child->GetSelectedContent(selection);
  }

  return content;
}

void Node::Reserved1() {}
void Node::Reserved2() {}
void Node::Reserved3() {}
void Node::Reserved4() {}
void Node::Reserved5() {}
void Node::Reserved6() {}
void Node::Reserved7() {}
void Node::Reserved8() {}

/// @brief Display an element on a ftxui::Screen.
/// @ingroup dom
void Render(Screen& screen, const Element& element) {
  Selection selection;
  Render(screen, element.get(), selection);
}

/// @brief Display an element on a ftxui::Screen.
/// @ingroup dom
void Render(Screen& screen, Node* node) {
  Selection selection;
  Render(screen, node, selection);
}

void Render(Screen& screen, Node* node, Selection& selection) {
  Box box;
  box.x_min = 0;
  box.y_min = 0;
  box.x_max = screen.dimx() - 1;
  box.y_max = screen.dimy() - 1;

  Node::Status status;
  node->Check(&status);
  const int max_iterations = 20;
  while (status.need_iteration && status.iteration < max_iterations) {
    // Step 1: Find what dimension this elements wants to be.
    node->ComputeRequirement();

    // Step 2: Assign a dimension to the element.
    node->SetBox(box);

    // Check if the element needs another iteration of the layout algorithm.
    status.need_iteration = false;
    status.iteration++;
    node->Check(&status);
  }

  // Step 3: Selection
  if (!selection.IsEmpty()) {
    node->Select(selection);
  }

  bool use_cursor = node->requirement().focused.enabled;
  if (!Terminal::GetQuirks().CursorHiding() &&
      node->requirement().focused.cursor_shape ==
          Screen::Cursor::Shape::Hidden) {
    // Setting the cursor to the right position allow folks using CJK (China,
    // Japanese, Korean, ...) characters to see their [input method editor]
    // displayed at the right location. See [issue].
    //
    // [input method editor]:
    // https://en.wikipedia.org/wiki/Input_method
    //
    // [issue]:
    // https://github.com/ArthurSonzogni/FTXUI/issues/2#issuecomment-505282355
    //
    // Unfortunately, Microsoft terminal do not handle properly hiding the
    // cursor. Instead the character under the cursor is hidden, which is a
    // big problem. As a result, we can't enable setting cursor to the right
    // location. It will be displayed at the bottom right corner.
    // See:
    // https://github.com/microsoft/terminal/issues/1203
    // https://github.com/microsoft/terminal/issues/3093
    use_cursor = false;
  }

  if (use_cursor) {
    screen.SetCursor(Screen::Cursor{
        node->requirement().focused.node->box_.x_min,
        node->requirement().focused.node->box_.y_min,
        node->requirement().focused.cursor_shape,
    });
  } else {
    screen.SetCursor(Screen::Cursor{
        screen.dimx() - 1,
        screen.dimy() - 1,
        Screen::Cursor::Shape::Hidden,
    });
  }

  // Step 4: Draw the element.
  screen.stencil = box;
  node->Render(screen);

  // Step 5: Apply shaders
  screen.ApplyShader();
}

std::string GetNodeSelectedContent(Screen& screen,
                                   Node* node,
                                   Selection& selection) {
  Box box;
  box.x_min = 0;
  box.y_min = 0;
  box.x_max = screen.dimx() - 1;
  box.y_max = screen.dimy() - 1;

  Node::Status status;
  node->Check(&status);
  const int max_iterations = 20;
  while (status.need_iteration && status.iteration < max_iterations) {
    // Step 1: Find what dimension this elements wants to be.
    node->ComputeRequirement();

    // Step 2: Assign a dimension to the element.
    node->SetBox(box);

    // Check if the element needs another iteration of the layout algorithm.
    status.need_iteration = false;
    status.iteration++;
    node->Check(&status);
  }

  // Step 3: Selection
  node->Select(selection);

  // Step 4: get the selected content.
  return node->GetSelectedContent(selection);
}

}  // namespace ftxui

// ---- dom\node_decorator.cpp --------------------------------------


namespace ftxui {

void NodeDecorator::ComputeRequirement() {
  Node::ComputeRequirement();
  requirement_ = children_[0]->requirement();
}

void NodeDecorator::SetBox(Box box) {
  Node::SetBox(box);
  children_[0]->SetBox(box);
}

}  // namespace ftxui

// ---- dom\paragraph.cpp -------------------------------------------


namespace ftxui {

namespace {
Elements Split(std::string_view the_text) {
  Elements output;
  size_t start = 0;
  size_t end = the_text.find(' ');
  while (end != std::string_view::npos) {
    output.push_back(text(the_text.substr(start, end - start)));
    start = end + 1;
    end = the_text.find(' ', start);
  }
  output.push_back(text(the_text.substr(start)));
  return output;
}

Element Split(std::string_view paragraph,
              const std::function<Element(std::string_view)>& f) {
  Elements output;
  size_t start = 0;
  size_t end = paragraph.find('\n');
  while (end != std::string_view::npos) {
    output.push_back(f(paragraph.substr(start, end - start)));
    start = end + 1;
    end = paragraph.find('\n', start);
  }
  output.push_back(f(paragraph.substr(start)));
  return vbox(std::move(output));
}

}  // namespace

/// @brief Return an element drawing the paragraph on multiple lines.
/// @ingroup dom
/// @see flexbox.
Element paragraph(std::string_view the_text) {
  return paragraphAlignLeft(the_text);
}

/// @brief Return an element drawing the paragraph on multiple lines, aligned on
/// the left.
/// @ingroup dom
/// @see flexbox.
Element paragraphAlignLeft(std::string_view the_text) {
  return Split(the_text, [](std::string_view line) {
    static const auto config = FlexboxConfig().SetGap(1, 0);
    return flexbox(Split(line), config);
  });
};

/// @brief Return an element drawing the paragraph on multiple lines, aligned on
/// the right.
/// @ingroup dom
/// @see flexbox.
Element paragraphAlignRight(std::string_view the_text) {
  return Split(the_text, [](std::string_view line) {
    static const auto config = FlexboxConfig().SetGap(1, 0).Set(
        FlexboxConfig::JustifyContent::FlexEnd);
    return flexbox(Split(line), config);
  });
}

/// @brief Return an element drawing the paragraph on multiple lines, aligned on
/// the center.
/// @ingroup dom
/// @see flexbox.
Element paragraphAlignCenter(std::string_view the_text) {
  return Split(the_text, [](std::string_view line) {
    static const auto config =
        FlexboxConfig().SetGap(1, 0).Set(FlexboxConfig::JustifyContent::Center);
    return flexbox(Split(line), config);
  });
}

/// @brief Return an element drawing the paragraph on multiple lines, aligned
/// using a justified alignment.
/// the center.
/// @ingroup dom
/// @see flexbox.
Element paragraphAlignJustify(std::string_view the_text) {
  return Split(the_text, [](std::string_view line) {
    static const auto config = FlexboxConfig().SetGap(1, 0).Set(
        FlexboxConfig::JustifyContent::SpaceBetween);
    Elements words = Split(line);
    words.push_back(text("") | xflex);
    return flexbox(std::move(words), config);
  });
}

}  // namespace ftxui

// ---- dom\reflect.cpp ---------------------------------------------


namespace ftxui {
namespace {

// Helper class.
class Reflect : public Node {
 public:
  Reflect(Element child, Box& box)
      : Node(unpack(std::move(child))), reflected_box_(box) {}

  void ComputeRequirement() final {
    Node::ComputeRequirement();
    requirement_ = children_[0]->requirement();
  }

  void SetBox(Box box) final {
    reflected_box_ = box;
    Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(Screen& screen) final {
    reflected_box_ = Box::Intersection(screen.stencil, reflected_box_);
    Node::Render(screen);
  }

 private:
  Box& reflected_box_;
};
}  // namespace

Decorator reflect(Box& box) {
  return [&](Element child) -> Element {
    return std::make_shared<Reflect>(std::move(child), box);
  };
}

}  // namespace ftxui

// ---- dom\scroll_indicator.cpp ------------------------------------


namespace ftxui {

/// @brief Display a vertical scrollbar on the right.
/// The background follows the content, the rest follows the parent.
/// @ingroup dom
Element vscroll_indicator(Element child) {
  class Impl : public NodeDecorator {
    using NodeDecorator::NodeDecorator;

    void ComputeRequirement() override {
      NodeDecorator::ComputeRequirement();
      requirement_ = children_[0]->requirement();
      requirement_.min_x++;
    }

    void SetBox(Box box) override {
      box_ = box;
      box.x_max--;
      children_[0]->SetBox(box);
    }

    void Render(Screen& screen) final {
      // Save the style from the parents, before the content draws over it.
      const Box stencil = screen.stencil;
      std::vector<Cell> saved;
      for (int y = stencil.y_min; y <= stencil.y_max; ++y) {
        saved.push_back(screen.CellAt(stencil.x_max, y));
      }

      NodeDecorator::Render(screen);

      const int size_inner = box_.y_max - box_.y_min;
      if (size_inner <= 0) {
        return;
      }
      const int size_outter = stencil.y_max - stencil.y_min + 1;
      if (size_outter >= size_inner) {
        return;
      }

      int size = 2 * size_outter * size_outter / size_inner;
      size = std::max(size, 1);

      const int start_y =
          2 * stencil.y_min +  //
          2 * (stencil.y_min - box_.y_min) * size_outter / size_inner;

      const int x = stencil.x_max;
      for (int y = stencil.y_min; y <= stencil.y_max; ++y) {
        const int y_up = 2 * y + 0;
        const int y_down = 2 * y + 1;
        const bool up = (start_y <= y_up) && (y_up <= start_y + size);
        const bool down = (start_y <= y_down) && (y_down <= start_y + size);

        const char* c = up ? (down ? "┃" : "╹") : (down ? "╻" : " ");  // NOLINT
        Cell& cell = screen.CellAt(x, y);
        const Color background = cell.background_color;
        cell = saved[y - stencil.y_min];
        cell.background_color = background;
        cell.character = c;
      }
    }
  };
  return std::make_shared<Impl>(std::move(child));
}

/// @brief Display a horizontal scrollbar at the bottom.
/// The background follows the content, the rest follows the parent.
/// @ingroup dom
Element hscroll_indicator(Element child) {
  class Impl : public NodeDecorator {
    using NodeDecorator::NodeDecorator;

    void ComputeRequirement() override {
      NodeDecorator::ComputeRequirement();
      requirement_ = children_[0]->requirement();
      requirement_.min_y++;
    }

    void SetBox(Box box) override {
      box_ = box;
      box.y_max--;
      children_[0]->SetBox(box);
    }

    void Render(Screen& screen) final {
      // Save the style from the parents, before the content draws over it.
      const Box stencil = screen.stencil;
      std::vector<Cell> saved;
      for (int x = stencil.x_min; x <= stencil.x_max; ++x) {
        saved.push_back(screen.CellAt(x, stencil.y_max));
      }

      NodeDecorator::Render(screen);

      const int size_inner = box_.x_max - box_.x_min;
      if (size_inner <= 0) {
        return;
      }
      const int size_outter = stencil.x_max - stencil.x_min + 1;
      if (size_outter >= size_inner) {
        return;
      }

      int size = 2 * size_outter * size_outter / size_inner;
      size = std::max(size, 1);

      const int start_x =
          2 * stencil.x_min +  //
          2 * (stencil.x_min - box_.x_min) * size_outter / size_inner;

      const int y = stencil.y_max;
      for (int x = stencil.x_min; x <= stencil.x_max; ++x) {
        const int x_left = 2 * x + 0;
        const int x_right = 2 * x + 1;
        const bool left = (start_x <= x_left) && (x_left <= start_x + size);
        const bool right = (start_x <= x_right) && (x_right <= start_x + size);

        const char* c =
            left ? (right ? "─" : "╴") : (right ? "╶" : " ");  // NOLINT
        Cell& cell = screen.CellAt(x, y);
        const Color background = cell.background_color;
        cell = saved[x - stencil.x_min];
        cell.background_color = background;
        cell.character = c;
      }
    }
  };
  return std::make_shared<Impl>(std::move(child));
}

}  // namespace ftxui

// ---- dom\selection.cpp -------------------------------------------



namespace ftxui {

namespace {
class Unselectable : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Select(Selection& ignored) override {
    std::ignore = ignored;
    // Overwrite the select method to do nothing.
  }
};
}  // namespace

/// @brief Create an empty selection.
Selection::Selection() = default;

/// @brief Create a selection.
/// @param start_x The x coordinate of the start of the selection.
/// @param start_y The y coordinate of the start of the selection.
/// @param end_x The x coordinate of the end of the selection.
/// @param end_y The y coordinate of the end of the selection.
Selection::Selection(int start_x, int start_y, int end_x, int end_y)
    : start_x_(start_x),
      start_y_(start_y),
      end_x_(end_x),
      end_y_(end_y),
      box_{
          std::min(start_x, end_x),
          std::max(start_x, end_x),
          std::min(start_y, end_y),
          std::max(start_y, end_y),
      },
      empty_(false) {}

Selection::Selection(int start_x,
                     int start_y,
                     int end_x,
                     int end_y,
                     Selection* parent)
    : start_x_(start_x),
      start_y_(start_y),
      end_x_(end_x),
      end_y_(end_y),
      box_{
          std::min(start_x, end_x),
          std::max(start_x, end_x),
          std::min(start_y, end_y),
          std::max(start_y, end_y),
      },
      parent_(parent),
      empty_(false) {}

/// @brief Get the box of the selection.
/// @return The box of the selection.
const Box& Selection::GetBox() const {
  return box_;
}

/// @brief Saturate the selection to be inside the box.
/// This is called by `hbox` to propagate the selection to its children.
/// @param box The box to saturate the selection in.
/// @return The saturated selection.
Selection Selection::SaturateHorizontal(Box box) {
  int start_x = start_x_;
  int start_y = start_y_;
  int end_x = end_x_;
  int end_y = end_y_;

  const bool start_outside = !box.Contain(start_x, start_y);
  const bool end_outside = !box.Contain(end_x, end_y);
  const bool properly_ordered =
      start_y < end_y || (start_y == end_y && start_x <= end_x);
  if (properly_ordered) {
    if (start_outside) {
      start_x = box.x_min;
      start_y = box.y_min;
    }
    if (end_outside) {
      end_x = box.x_max;
      end_y = box.y_max;
    }
  } else {
    if (start_outside) {
      start_x = box.x_max;
      start_y = box.y_max;
    }
    if (end_outside) {
      end_x = box.x_min;
      end_y = box.y_min;
    }
  }
  return {
      start_x, start_y, end_x, end_y, parent_,
  };
}

/// @brief Saturate the selection to be inside the box.
/// This is called by `vbox` to propagate the selection to its children.
/// @param box The box to saturate the selection in.
/// @return The saturated selection.
Selection Selection::SaturateVertical(Box box) {
  int start_x = start_x_;
  int start_y = start_y_;
  int end_x = end_x_;
  int end_y = end_y_;

  const bool start_outside = !box.Contain(start_x, start_y);
  const bool end_outside = !box.Contain(end_x, end_y);
  const bool properly_ordered =
      start_y < end_y || (start_y == end_y && start_x <= end_x);

  if (properly_ordered) {
    if (start_outside) {
      start_x = box.x_min;
      start_y = box.y_min;
    }
    if (end_outside) {
      end_x = box.x_max;
      end_y = box.y_max;
    }
  } else {
    if (start_outside) {
      start_x = box.x_max;
      start_y = box.y_max;
    }
    if (end_outside) {
      end_x = box.x_min;
      end_y = box.y_min;
    }
  }
  return {start_x, start_y, end_x, end_y, parent_};
}

void Selection::AddPart(std::string_view part, int y, int left, int right) {
  if (parent_ != this) {
    parent_->AddPart(part, y, left, right);
    return;
  }
  [&] {
    if (parts_.str().empty()) {
      parts_ << part;
      return;
    }

    if (y_ != y) {
      parts_ << '\n' << part;
      return;
    }

    if (x_ == left + 1) {
      parts_ << part;
      return;
    }

    // There is a horizontal gap of blank cells between the previously
    // recorded part and this one. Such gaps arise from layout that
    // separates selectable text with empty columns instead of literal
    // space characters (e.g. flexbox gaps from FlexboxConfig::SetGap,
    // fillers, spacing decorators). Those cells lie inside the selected
    // region and read as spaces on screen, so the copied text must
    // contain them too. Only forward gaps are filled; overlapping or
    // out-of-order parts fall through to a plain append.
    for (int x = x_ + 1; x < left; ++x) {
      parts_ << ' ';
    }
    parts_ << part;
  }();
  y_ = y;
  x_ = right;
}

}  // namespace ftxui

// ---- dom\selection_style.cpp -------------------------------------


namespace ftxui {

namespace {

class SelectionStyleReset : public NodeDecorator {
 public:
  explicit SelectionStyleReset(Element child)
      : NodeDecorator(std::move(child)) {}

  void Render(Screen& screen) final {
    auto old_style = screen.GetSelectionStyle();
    screen.SetSelectionStyle([](Cell&) {});
    NodeDecorator::Render(screen);
    screen.SetSelectionStyle(old_style);
  }
};

class SelectionStyle : public NodeDecorator {
 public:
  SelectionStyle(Element child, const std::function<void(Cell&)>& style)
      : NodeDecorator(std::move(child)), style_(style) {}

  void Render(Screen& screen) final {
    auto old_style = screen.GetSelectionStyle();
    auto new_style = [&, old_style](Cell& pixel) {
      old_style(pixel);
      style_(pixel);
    };
    screen.SetSelectionStyle(new_style);
    NodeDecorator::Render(screen);
    screen.SetSelectionStyle(old_style);
  }

  std::function<void(Cell&)> style_;
};

}  // namespace

/// @brief Reset the selection style of an element.
/// @param child The input element.
/// @return The output element with the selection style reset.
Element selectionStyleReset(Element child) {
  return std::make_shared<SelectionStyleReset>(std::move(child));
}

/// @brief Set the background color of an element when selected.
/// Note that the style is applied on top of the existing style.
Decorator selectionBackgroundColor(Color foreground) {
  return selectionStyle([foreground](Cell& pixel) {  //
    pixel.background_color = foreground;
  });
}

/// @brief Set the foreground color of an element when selected.
/// Note that the style is applied on top of the existing style.
Decorator selectionForegroundColor(Color foreground) {
  return selectionStyle([foreground](Cell& pixel) {  //
    pixel.foreground_color = foreground;
  });
}

/// @brief Set the color of an element when selected.
/// @param foreground The color to be applied.
/// Note that the style is applied on top of the existing style.
Decorator selectionColor(Color foreground) {
  return selectionForegroundColor(foreground);
}

/// @brief Set the style of an element when selected.
/// @param style The style to be applied.
/// Note that the style is applied on top of the existing style.
// NOLINTNEXTLINE
Decorator selectionStyle(std::function<void(Cell&)> style) {
  return [style](Element child) -> Element {
    return std::make_shared<SelectionStyle>(std::move(child), style);
  };
}

}  // namespace ftxui

// ---- dom\separator.cpp -------------------------------------------


namespace ftxui {

namespace {
using SeparatorCharset = std::array<std::string, 2>;        // NOLINT
using SeparatorCharsets = std::array<SeparatorCharset, 6>;  // NOLINT

const SeparatorCharsets charsets = {
    // NOLINT
    SeparatorCharset{"│", "─"},  // LIGHT
    SeparatorCharset{"╏", "╍"},  // DASHED
    SeparatorCharset{"┃", "━"},  // HEAVY
    SeparatorCharset{"║", "═"},  // DOUBLE
    SeparatorCharset{"│", "─"},  // ROUNDED
    SeparatorCharset{" ", " "},  // EMPTY
};

class Separator : public Node {
 public:
  explicit Separator(std::string value) : value_(std::move(value)) {}

  void ComputeRequirement() override {
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(Screen& screen) override {
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        Cell& pixel = screen.CellAt(x, y);
        pixel.character = value_;
        pixel.automerge = true;
      }
    }
  }

  std::string value_;
};

class SeparatorAuto : public Node {
 public:
  explicit SeparatorAuto(BorderStyle style) : style_(style) {}

  void ComputeRequirement() override {
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(Screen& screen) override {
    const bool is_column = (box_.x_max == box_.x_min);
    const bool is_line = (box_.y_min == box_.y_max);

    const std::string c =
        charsets[style_][int(is_line && !is_column)];  // NOLINT

    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        Cell& pixel = screen.CellAt(x, y);
        pixel.character = c;
        pixel.automerge = true;
      }
    }
  }

  BorderStyle style_;
};

class SeparatorWithCell : public SeparatorAuto {
 public:
  explicit SeparatorWithCell(Cell pixel)
      : SeparatorAuto(LIGHT), pixel_(std::move(pixel)) {
    pixel_.automerge = true;
  }
  void Render(Screen& screen) override {
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y) = pixel_;
      }
    }
  }

 private:
  Cell pixel_;
};
}  // namespace

/// @brief Draw a vertical or horizontal separation in between two other
/// elements.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separator(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ────
/// down
/// ```
Element separator() {
  return std::make_shared<SeparatorAuto>(LIGHT);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements.
/// @param style the style of the separator.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separatorStyled(DOUBLE),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ════
/// down
/// ```
Element separatorStyled(BorderStyle style) {
  return std::make_shared<SeparatorAuto>(style);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements, using the LIGHT style.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separatorLight(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ────
/// down
/// ```
Element separatorLight() {
  return std::make_shared<SeparatorAuto>(LIGHT);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements, using the DASHED style.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separatorLight(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ╍╍╍╍
/// down
/// ```
Element separatorDashed() {
  return std::make_shared<SeparatorAuto>(DASHED);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements, using the HEAVY style.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separatorHeavy(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ━━━━
/// down
/// ```
Element separatorHeavy() {
  return std::make_shared<SeparatorAuto>(HEAVY);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements, using the DOUBLE style.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separatorDouble(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ════
/// down
/// ```
Element separatorDouble() {
  return std::make_shared<SeparatorAuto>(DOUBLE);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements, using the EMPTY style.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separator(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
///
/// down
/// ```
Element separatorEmpty() {
  return std::make_shared<SeparatorAuto>(EMPTY);
}

/// @brief Draw a vertical or horizontal separation in between two other
/// elements.
/// @param value the character to fill the separator area.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorDouble
/// @see separatorHeavy
/// @see separatorEmpty
/// @see separatorRounded
/// @see separatorStyled
/// @see separatorCharacter
///
/// Add a visual separation in between two elements.
///
/// ### Example
///
/// ```cpp
/// // Use 'border' as a function...
/// Element document = vbox({
///   text("up"),
///   separator(),
///   text("down"),
/// });
/// ```
///
/// ### Output
///
/// ```bash
/// up
/// ────
/// down
/// ```
Element separatorCharacter(std::string_view value) {
  return std::make_shared<Separator>(std::string(value));
}

/// @brief Draw a separator in between two element filled with a given pixel.
/// @ingroup dom
/// @see separator
/// @see separatorLight
/// @see separatorDashed
/// @see separatorHeavy
/// @see separatorDouble
/// @see separatorStyled
///
/// ### Example
///
/// ```cpp
/// Cell empty;
/// Element document = vbox({
///   text("Up"),
///   separator(empty),
///   text("Down"),
/// })
/// ```
///
/// ### Output
///
/// ```bash
/// Up
///
/// Down
/// ```
Element separator(Cell pixel) {
  return std::make_shared<SeparatorWithCell>(std::move(pixel));
}

/// @brief Draw a horizontal bar, with the area in between left/right colored
/// differently.
/// @param left the left limit of the active area.
/// @param right the right limit of the active area.
/// @param selected_color the color of the selected area.
/// @param unselected_color the color of the unselected area.
///
/// ### Example
///
/// ```cpp
/// Element document = separatorHSelector(2,5, Color::White, Color::Blue);
/// ```
Element separatorHSelector(float left,
                           float right,
                           Color unselected_color,
                           Color selected_color) {
  class Impl : public Node {
   public:
    Impl(float left, float right, Color selected_color, Color unselected_color)
        : left_(left),
          right_(right),
          unselected_color_(unselected_color),
          selected_color_(selected_color) {}
    void ComputeRequirement() override {
      requirement_.min_x = 1;
      requirement_.min_y = 1;
    }

    void Render(Screen& screen) override {
      if (box_.y_max < box_.y_min) {
        return;
      }

      // This are the two location with an empty demi-cell.
      int demi_cell_left = int(left_ * 2.F - 1.F);    // NOLINT
      int demi_cell_right = int(right_ * 2.F + 2.F);  // NOLINT

      const int y = box_.y_min;
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        Cell& pixel = screen.CellAt(x, y);

        const int a = (x - box_.x_min) * 2;
        const int b = a + 1;
        const bool a_empty = demi_cell_left == a || demi_cell_right == a;
        const bool b_empty = demi_cell_left == b || demi_cell_right == b;

        if (!a_empty && !b_empty) {
          pixel.character = "─";
          pixel.automerge = true;
        } else {
          pixel.character = a_empty ? "╶" : "╴";  // NOLINT
          pixel.automerge = false;
        }

        if (demi_cell_left <= a && b <= demi_cell_right) {
          pixel.foreground_color = selected_color_;
        } else {
          pixel.foreground_color = unselected_color_;
        }
      }
    }

    float left_;
    float right_;
    Color unselected_color_;
    Color selected_color_;
  };
  return std::make_shared<Impl>(left, right, unselected_color, selected_color);
}

/// @brief Draw an vertical bar, with the area in between up/downcolored
/// differently.
/// @param up the left limit of the active area.
/// @param down the right limit of the active area.
/// @param selected_color the color of the selected area.
/// @param unselected_color the color of the unselected area.
///
/// ### Example
///
/// ```cpp
/// Element document = separatorHSelector(2,5, Color::White, Color::Blue);
/// ```
Element separatorVSelector(float up,
                           float down,
                           Color unselected_color,
                           Color selected_color) {
  class Impl : public Node {
   public:
    Impl(float up, float down, Color unselected_color, Color selected_color)
        : up_(up),
          down_(down),
          unselected_color_(unselected_color),
          selected_color_(selected_color) {}
    void ComputeRequirement() override {
      requirement_.min_x = 1;
      requirement_.min_y = 1;
    }

    void Render(Screen& screen) override {
      if (box_.x_max < box_.x_min) {
        return;
      }

      // This are the two location with an empty demi-cell.
      const int demi_cell_up = int(up_ * 2 - 1);
      const int demi_cell_down = int(down_ * 2 + 2);

      const int x = box_.x_min;
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        Cell& pixel = screen.CellAt(x, y);

        const int a = (y - box_.y_min) * 2;
        const int b = a + 1;
        const bool a_empty = demi_cell_up == a || demi_cell_down == a;
        const bool b_empty = demi_cell_up == b || demi_cell_down == b;

        if (!a_empty && !b_empty) {
          pixel.character = "│";
          pixel.automerge = true;
        } else {
          pixel.character = a_empty ? "╷" : "╵";  // NOLINT
          pixel.automerge = false;
        }

        if (demi_cell_up <= a && b <= demi_cell_down) {
          pixel.foreground_color = selected_color_;
        } else {
          pixel.foreground_color = unselected_color_;
        }
      }
    }

    float up_;
    float down_;
    Color unselected_color_;
    Color selected_color_;
  };
  return std::make_shared<Impl>(up, down, unselected_color, selected_color);
}

}  // namespace ftxui

// ---- dom\size.cpp ------------------------------------------------


namespace ftxui {

namespace {
class Size : public Node {
 public:
  Size(Element child, WidthOrHeight direction, Constraint constraint, int value)
      : Node(unpack(std::move(child))),
        direction_(direction),
        constraint_(constraint),
        value_(std::max(0, value)) {}

  void ComputeRequirement() override {
    Node::ComputeRequirement();
    requirement_ = children_[0]->requirement();

    auto& value = direction_ == WIDTH ? requirement_.min_x : requirement_.min_y;

    switch (constraint_) {
      case LESS_THAN:
        value = std::min(value, value_);
        break;
      case EQUAL:
        value = value_;
        break;
      case GREATER_THAN:
        value = std::max(value, value_);
        break;
    }

    if (direction_ == WIDTH) {
      requirement_.flex_grow_x = 0;
      requirement_.flex_shrink_x = 0;
    } else {
      requirement_.flex_grow_y = 0;
      requirement_.flex_shrink_y = 0;
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    if (direction_ == WIDTH) {
      switch (constraint_) {
        case LESS_THAN:
        case EQUAL:
          box.x_max = std::min(box.x_min + value_ + 1, box.x_max);
          break;
        case GREATER_THAN:
          break;
      }
    } else {
      switch (constraint_) {
        case LESS_THAN:
        case EQUAL:
          box.y_max = std::min(box.y_min + value_ + 1, box.y_max);
          break;
        case GREATER_THAN:
          break;
      }
    }
    children_[0]->SetBox(box);
  }

 private:
  WidthOrHeight direction_;
  Constraint constraint_;
  int value_;
};
}  // namespace

/// @brief Apply a constraint on the size of an element.
/// @param direction Whether the WIDTH or the HEIGHT of the element must be
///                  constrained.
/// @param constraint The type of constraint.
/// @param value The value.
/// @ingroup dom
Decorator size(WidthOrHeight direction, Constraint constraint, int value) {
  return [=](Element e) {
    return std::make_shared<Size>(std::move(e), direction, constraint, value);
  };
}

}  // namespace ftxui

// ---- dom\spinner.cpp ---------------------------------------------


namespace ftxui {

namespace {
// NOLINTNEXTLINE
const std::vector<std::vector<std::vector<std::string>>> elements = {
    {
        {"Replaced by the gauge"},
    },
    {
        {".  "},
        {".. "},
        {"..."},
    },
    {
        {"|"},
        {"/"},
        {"-"},
        {"\\"},
    },
    {
        {"+"},
        {"x"},
    },
    {
        {"|  "},
        {"|| "},
        {"|||"},
    },
    {
        {"←"},
        {"↖"},
        {"↑"},
        {"↗"},
        {"→"},
        {"↘"},
        {"↓"},
        {"↙"},
    },
    {
        {"▁"},
        {"▂"},
        {"▃"},
        {"▄"},
        {"▅"},
        {"▆"},
        {"▇"},
        {"█"},
        {"▇"},
        {"▆"},
        {"▅"},
        {"▄"},
        {"▃"},
        {"▁"},
    },
    {
        {"▉"},
        {"▊"},
        {"▋"},
        {"▌"},
        {"▍"},
        {"▎"},
        {"▏"},
        {"▎"},
        {"▍"},
        {"▌"},
        {"▋"},
        {"▊"},
    },
    {
        {"▖"},
        {"▘"},
        {"▝"},
        {"▗"},
    },
    {
        {"◢"},
        {"◣"},
        {"◤"},
        {"◥"},
    },
    {
        {"◰"},
        {"◳"},
        {"◲"},
        {"◱"},
    },
    {
        {"◴"},
        {"◷"},
        {"◶"},
        {"◵"},
    },
    {
        {"◐"},
        {"◓"},
        {"◑"},
        {"◒"},
    },
    {
        {"◡"},
        {"⊙"},
        {"◠"},
    },
    {
        {"⠁"},
        {"⠂"},
        {"⠄"},
        {"⡀"},
        {"⢀"},
        {"⠠"},
        {"⠐"},
        {"⠈"},
    },
    {
        {"⠋"},
        {"⠙"},
        {"⠹"},
        {"⠸"},
        {"⠼"},
        {"⠴"},
        {"⠦"},
        {"⠧"},
        {"⠇"},
        {"⠏"},
    },
    {
        {"(*----------)"}, {"(-*---------)"}, {"(--*--------)"},
        {"(---*-------)"}, {"(----*------)"}, {"(-----*-----)"},
        {"(------*----)"}, {"(-------*---)"}, {"(--------*--)"},
        {"(---------*-)"}, {"(----------*)"}, {"(---------*-)"},
        {"(--------*--)"}, {"(-------*---)"}, {"(------*----)"},
        {"(-----*-----)"}, {"(----*------)"}, {"(---*-------)"},
        {"(--*--------)"}, {"(-*---------)"},
    },
    {
        {"[      ]"},
        {"[=     ]"},
        {"[==    ]"},
        {"[===   ]"},
        {"[====  ]"},
        {"[===== ]"},
        {"[======]"},
        {"[===== ]"},
        {"[====  ]"},
        {"[===   ]"},
        {"[==    ]"},
        {"[=     ]"},
    },
    {
        {"[      ]"},
        {"[=     ]"},
        {"[==    ]"},
        {"[===   ]"},
        {"[====  ]"},
        {"[===== ]"},
        {"[======]"},
        {"[ =====]"},
        {"[  ====]"},
        {"[   ===]"},
        {"[    ==]"},
        {"[     =]"},
    },
    {
        {"[==    ]"},
        {"[==    ]"},
        {"[==    ]"},
        {"[==    ]"},
        {"[==    ]"},
        {" [==   ]"},
        {"[  ==  ]"},
        {"[   == ]"},
        {"[    ==]"},
        {"[    ==]"},
        {"[    ==]"},
        {"[    ==]"},
        {"[    ==]"},
        {"[   ==] "},
        {"[  ==  ]"},
        {"[ ==   ]"},
    },
    {
        {
            " ─╮",
            "  │",
            "   ",
        },
        {
            "  ╮",
            "  │",
            "  ╯",
        },
        {
            "   ",
            "  │",
            " ─╯",
        },
        {
            "   ",
            "   ",
            "╰─╯",
        },
        {
            "   ",
            "│  ",
            "╰─ ",
        },
        {
            "╭  ",
            "│  ",
            "╰  ",
        },
        {
            "╭─ ",
            "│  ",
            "   ",
        },
        {
            "╭─╮",
            "   ",
            "   ",
        },
    },
    {
        {
            "   /\\O ",
            "    /\\/",
            "   /\\  ",
            "  /  \\ ",
            "LOL  LOL",
        },
        {
            "    _O  ",
            "   //|_ ",
            "    |   ",
            "   /|   ",
            "   LLOL ",
        },
        {
            "     O  ",
            "    /_  ",
            "    |\\  ",
            "   / |  ",
            " LOLLOL ",
        },
    },
    {
        {"       ", "_______", "       "},
        {"       ", "______/", "       "},
        {"      _", "_____/ ", "       "},
        {"     _ ", "____/ \\", "       "},
        {"    _  ", "___/ \\ ", "      \\"},
        {"   _   ", "__/ \\  ", "     \\_"},
        {"  _    ", "_/ \\   ", "    \\_/"},
        {" _     ", "/ \\   _", "   \\_/ "},
        {"_      ", " \\   __", "  \\_/  "},
        {"       ", "\\   ___", " \\_/   "},
        {"       ", "    ___", "\\_/    "},
        {"       ", "  _____", "_/     "},
        {"       ", " ______", "/      "},
        {"       ", "_______", "       "},
    },
};

}  // namespace

/// @brief Useful to represent the effect of time and/or events. This displays
/// an ASCII art "video".
/// @param charset_index The type of "video".
/// @param image_index The "frame" of the video. You need to increase this for
/// every "step".
/// @ingroup dom
Element spinner(int charset_index, size_t image_index) {
  if (charset_index <= 0) {
    const int progress_size = 40;
    image_index %= progress_size;
    if (image_index > progress_size / 2) {
      image_index = progress_size - image_index;
    }
    return gauge(float(image_index) * 0.05F);  // NOLINT
  }
  charset_index %= int(elements.size());
  image_index %= int(elements[charset_index].size());
  std::vector<Element> lines;
  for (const auto& it : elements[charset_index][image_index]) {
    lines.push_back(text(it));
  }
  return vbox(std::move(lines));
}

}  // namespace ftxui

// ---- dom\strikethrough.cpp ---------------------------------------


namespace ftxui {

/// @brief Apply a strikethrough to text.
/// @ingroup dom
Element strikethrough(Element child) {
  class Impl : public NodeDecorator {
   public:
    using NodeDecorator::NodeDecorator;

    void Render(Screen& screen) override {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).strikethrough = true;
        }
      }
      Node::Render(screen);
    }
  };

  return std::make_shared<Impl>(std::move(child));
}

}  // namespace ftxui

// ---- dom\table.cpp -----------------------------------------------



namespace ftxui {
namespace {

bool IsCell(int x, int y) {
  return x % 2 == 1 && y % 2 == 1;
}

// NOLINTNEXTLINE
static std::string table_charset[6][6] = {
    {"┌", "┐", "└", "┘", "─", "│"},  // LIGHT
    {"┏", "┓", "┗", "┛", "╍", "╏"},  // DASHED
    {"┏", "┓", "┗", "┛", "━", "┃"},  // HEAVY
    {"╔", "╗", "╚", "╝", "═", "║"},  // DOUBLE
    {"╭", "╮", "╰", "╯", "─", "│"},  // ROUNDED
    {" ", " ", " ", " ", " ", " "},  // EMPTY
};

// Resolve negative indices as counting from the end.
int Wrap(int input, int modulo) {
  return input < 0 ? input + modulo : input;
}

bool InRange(int input, int modulo) {
  return input >= 0 && input < modulo;
}

void Order(int& a, int& b) {
  if (a >= b) {
    std::swap(a, b);
  }
}

}  // namespace

/// @brief Create an empty table.
Table::Table() {
  Initialize({});
}

/// @brief Create a table from a vector of vector of string.
/// @param input The input data.
Table::Table(const std::vector<std::vector<std::string>>& input) {
  std::vector<std::vector<Element>> output;
  output.reserve(input.size());
  for (const auto& row : input) {
    output.emplace_back();
    auto& output_row = output.back();
    output_row.reserve(row.size());
    for (const auto& cell : row) {
      output_row.push_back(text(std::move(cell)));
    }
  }
  Initialize(std::move(output));
}

/// @brief Create a table from a vector of vector of Element
/// @param input The input elements.
Table::Table(std::vector<std::vector<Element>> input) {
  Initialize(std::move(input));
}

/// @brief Create a table from a list of list of string.
/// @param init The input data.
Table::Table(std::initializer_list<std::vector<std::string>> init) {
  std::vector<std::vector<Element>> input;
  for (const auto& row : init) {
    std::vector<Element> output_row;
    output_row.reserve(row.size());
    for (const auto& cell : row) {
      output_row.push_back(text(cell));
    }
    input.push_back(std::move(output_row));
  }
  Initialize(std::move(input));
}

// private
void Table::Initialize(std::vector<std::vector<Element>> input) {
  input_dim_y_ = static_cast<int>(input.size());
  input_dim_x_ = 0;
  for (auto& row : input) {
    input_dim_x_ = std::max(input_dim_x_, int(row.size()));
  }

  dim_y_ = 2 * input_dim_y_ + 1;
  dim_x_ = 2 * input_dim_x_ + 1;

  // Reserve space.
  elements_.resize(dim_y_);
  for (int y = 0; y < dim_y_; ++y) {
    elements_[y].resize(dim_x_);
  }

  // Transfer elements_ from |input| toward |elements_|.
  {
    int y = 1;
    for (auto& row : input) {
      int x = 1;
      for (auto& cell : row) {
        elements_[y][x] = std::move(cell);
        x += 2;
      }
      y += 2;
    }
  }

  // Add empty element for the border.
  for (int y = 0; y < dim_y_; ++y) {
    for (int x = 0; x < dim_x_; ++x) {
      auto& element = elements_[y][x];

      if (IsCell(x, y)) {
        if (!element) {
          element = emptyElement();
        }
        continue;
      }

      element = emptyElement();
    }
  }
}

/// @brief Select a row of the table.
/// @param index The index of the row to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectRow(int index) {
  return SelectRectangle(0, -1, index, index);
}

/// @brief Select a range of rows of the table.
/// @param row_min The first row to select.
/// @param row_max The last row to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectRows(int row_min, int row_max) {
  return SelectRectangle(0, -1, row_min, row_max);
}

/// @brief Select a column of the table.
/// @param index The index of the column to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectColumn(int index) {
  return SelectRectangle(index, index, 0, -1);
}

/// @brief Select a range of columns of the table.
/// @param column_min The first column to select.
/// @param column_max The last column to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectColumns(int column_min, int column_max) {
  return SelectRectangle(column_min, column_max, 0, -1);
}

/// @brief Select a cell of the table.
/// @param column The column of the cell to select.
/// @param row The row of the cell to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectCell(int column, int row) {
  return SelectRectangle(column, column, row, row);
}

/// @brief Select a rectangle of the table.
/// @param column_min The first column to select.
/// @param column_max The last column to select.
/// @param row_min The first row to select.
/// @param row_max The last row to select.
/// @note You can use negative index to select from the end.
TableSelection Table::SelectRectangle(int column_min,
                                      int column_max,
                                      int row_min,
                                      int row_max) {
  TableSelection output;  // NOLINT
  output.table_ = this;

  column_min = Wrap(column_min, input_dim_x_);
  column_max = Wrap(column_max, input_dim_x_);
  row_min = Wrap(row_min, input_dim_y_);
  row_max = Wrap(row_max, input_dim_y_);

  // Out of range indices select nothing.
  if (!InRange(column_min, input_dim_x_) ||
      !InRange(column_max, input_dim_x_) || !InRange(row_min, input_dim_y_) ||
      !InRange(row_max, input_dim_y_)) {
    output.x_min_ = 0;
    output.x_max_ = -1;
    output.y_min_ = 0;
    output.y_max_ = -1;
    return output;
  }

  Order(column_min, column_max);
  Order(row_min, row_max);

  output.x_min_ = 2 * column_min;
  output.x_max_ = 2 * column_max + 2;
  output.y_min_ = 2 * row_min;
  output.y_max_ = 2 * row_max + 2;
  return output;
}

/// @brief Select all the table.
TableSelection Table::SelectAll() {
  TableSelection output;  // NOLINT
  output.table_ = this;
  output.x_min_ = 0;
  output.x_max_ = dim_x_ - 1;
  output.y_min_ = 0;
  output.y_max_ = dim_y_ - 1;
  return output;
}

/// @brief Render the table.
/// @return The rendered table. This is an element you can draw.
Element Table::Render() {
  for (int y = 0; y < dim_y_; ++y) {
    for (int x = 0; x < dim_x_; ++x) {
      auto& it = elements_[y][x];

      // Line
      if ((x + y) % 2 == 1) {
        it = std::move(it) | flex;
        continue;
      }

      // Cells
      if ((x % 2) == 1 && (y % 2) == 1) {
        it = std::move(it) | flex_shrink;
        continue;
      }

      // Corners
      it = std::move(it) | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);
    }
  }
  dim_x_ = 0;
  dim_y_ = 0;
  return gridbox(std::move(elements_));
}

/// @brief Apply the `decorator` to the selection.
/// This decorate both the cells, the lines and the corners.
/// @param decorator The decorator to apply.
// NOLINTNEXTLINE
void TableSelection::Decorate(const Decorator& decorator) {
  for (int y = y_min_; y <= y_max_; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      Element& e = table_->elements_[y][x];
      e = std::move(e) | decorator;
    }
  }
}

/// @brief Apply the `decorator` to the selection.
/// @param decorator The decorator to apply.
/// This decorate only the cells.
// NOLINTNEXTLINE
void TableSelection::DecorateCells(const Decorator& decorator) {
  for (int y = y_min_; y <= y_max_; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      if (y % 2 == 1 && x % 2 == 1) {
        Element& e = table_->elements_[y][x];
        e = std::move(e) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the selection.
/// This decorate only the lines modulo `modulo` with a shift of `shift`.
/// @param decorator The decorator to apply.
/// @param modulo The modulo of the lines to decorate.
/// @param shift The shift of the lines to decorate.
// NOLINTNEXTLINE
void TableSelection::DecorateAlternateColumn(const Decorator& decorator,
                                             int modulo,
                                             int shift) {
  for (int y = y_min_; y <= y_max_; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      if (y % 2 == 1 && (x / 2) % modulo == shift) {
        Element& e = table_->elements_[y][x];
        e = std::move(e) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the selection.
/// This decorate only the lines modulo `modulo` with a shift of `shift`.
/// @param decorator The decorator to apply.
/// @param modulo The modulo of the lines to decorate.
/// @param shift The shift of the lines to decorate.
// NOLINTNEXTLINE
void TableSelection::DecorateAlternateRow(const Decorator& decorator,
                                          int modulo,
                                          int shift) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      if (y % 2 == 1 && (y / 2) % modulo == shift) {
        Element& e = table_->elements_[y][x];
        e = std::move(e) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the selection.
/// This decorate only the corners modulo `modulo` with a shift of `shift`.
/// @param decorator The decorator to apply.
/// @param modulo The modulo of the corners to decorate.
/// @param shift The shift of the corners to decorate.
// NOLINTNEXTLINE
void TableSelection::DecorateCellsAlternateColumn(const Decorator& decorator,
                                                  int modulo,
                                                  int shift) {
  for (int y = y_min_; y <= y_max_; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      if (y % 2 == 1 && x % 2 == 1 && ((x / 2) % modulo == shift)) {
        Element& e = table_->elements_[y][x];
        e = std::move(e) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the selection.
/// This decorate only the corners modulo `modulo` with a shift of `shift`.
/// @param decorator The decorator to apply.
/// @param modulo The modulo of the corners to decorate.
/// @param shift The shift of the corners to decorate.
// NOLINTNEXTLINE
void TableSelection::DecorateCellsAlternateRow(const Decorator& decorator,
                                               int modulo,
                                               int shift) {
  for (int y = y_min_; y <= y_max_; ++y) {
    for (int x = x_min_; x <= x_max_; ++x) {
      if (y % 2 == 1 && x % 2 == 1 && ((y / 2) % modulo == shift)) {
        Element& e = table_->elements_[y][x];
        e = std::move(e) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the border of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateBorder(const Decorator& decorator) {
  for (int x = x_min_; x <= x_max_; ++x) {
    table_->elements_[y_min_][x] =
        std::move(table_->elements_[y_min_][x]) | decorator;
    table_->elements_[y_max_][x] =
        std::move(table_->elements_[y_max_][x]) | decorator;
  }
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    table_->elements_[y][x_min_] =
        std::move(table_->elements_[y][x_min_]) | decorator;
    table_->elements_[y][x_max_] =
        std::move(table_->elements_[y][x_max_]) | decorator;
  }
}

/// @brief Apply the `decorator` to the left border of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateBorderLeft(const Decorator& decorator) {
  for (int y = y_min_; y <= y_max_; y++) {
    table_->elements_[y][x_min_] =
        std::move(table_->elements_[y][x_min_]) | decorator;
  }
}

/// @brief Apply the `decorator` to the right border of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateBorderRight(const Decorator& decorator) {
  for (int y = y_min_; y <= y_max_; y++) {
    table_->elements_[y][x_max_] =
        std::move(table_->elements_[y][x_max_]) | decorator;
  }
}

/// @brief Apply the `decorator` to the top border of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateBorderTop(const Decorator& decorator) {
  for (int x = x_min_; x <= x_max_; x++) {
    table_->elements_[y_min_][x] =
        std::move(table_->elements_[y_min_][x]) | decorator;
  }
}

/// @brief Apply the `decorator` to the bottom border of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateBorderBottom(const Decorator& decorator) {
  for (int x = x_min_; x <= x_max_; x++) {
    table_->elements_[y_max_][x] =
        std::move(table_->elements_[y_max_][x]) | decorator;
  }
}

/// @brief Apply the `decorator` to the separators of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateSeparator(const Decorator& decorator) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (y % 2 == 0 || x % 2 == 0) {
        table_->elements_[y][x] =
            std::move(table_->elements_[y][x]) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the vertical separators of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateSeparatorVertical(const Decorator& decorator) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (x % 2 == 0) {
        table_->elements_[y][x] =
            std::move(table_->elements_[y][x]) | decorator;
      }
    }
  }
}

/// @brief Apply the `decorator` to the horizontal separators of the selection.
/// @param decorator The decorator to apply.
void TableSelection::DecorateSeparatorHorizontal(const Decorator& decorator) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (y % 2 == 0) {
        table_->elements_[y][x] =
            std::move(table_->elements_[y][x]) | decorator;
      }
    }
  }
}

/// @brief Apply a `border` around the selection.
/// @param border The border style to apply.
void TableSelection::Border(BorderStyle border) {
  if (x_min_ > x_max_ || y_min_ > y_max_) {
    return;
  }

  BorderLeft(border);
  BorderRight(border);
  BorderTop(border);
  BorderBottom(border);

  // NOLINTNEXTLINE
  table_->elements_[y_min_][x_min_] =
      text(table_charset[border][0]) | automerge;
  // NOLINTNEXTLINE
  table_->elements_[y_min_][x_max_] =
      text(table_charset[border][1]) | automerge;
  // NOLINTNEXTLINE
  table_->elements_[y_max_][x_min_] =
      text(table_charset[border][2]) | automerge;
  // NOLINTNEXTLINE
  table_->elements_[y_max_][x_max_] =
      text(table_charset[border][3]) | automerge;
}

/// @brief Apply a `border` around the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::Border(BorderStyle border, const Decorator& decorator) {
  Border(border);
  DecorateBorder(decorator);
}

/// @brief Draw some separator lines in the selection.
/// @param border The border style to apply.
void TableSelection::Separator(BorderStyle border) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (y % 2 == 0 || x % 2 == 0) {
        Element& e = table_->elements_[y][x];
        e = (y % 2 == 1) ? separatorCharacter(table_charset[border][5]) |
                               automerge  // NOLINT
                         : separatorCharacter(table_charset[border][4]) |
                               automerge;  // NOLINT
      }
    }
  }
}

/// @brief Draw some separator lines in the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::Separator(BorderStyle border, const Decorator& decorator) {
  Separator(border);
  DecorateSeparator(decorator);
}

/// @brief Draw some vertical separator lines in the selection.
/// @param border The border style to apply.
void TableSelection::SeparatorVertical(BorderStyle border) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (x % 2 == 0) {
        table_->elements_[y][x] =
            separatorCharacter(table_charset[border][5]) | automerge;  // NOLINT
      }
    }
  }
}

/// @brief Draw some vertical separator lines in the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::SeparatorVertical(BorderStyle border,
                                       const Decorator& decorator) {
  SeparatorVertical(border);
  DecorateSeparatorVertical(decorator);
}

/// @brief Draw some horizontal separator lines in the selection.
/// @param border The border style to apply.
void TableSelection::SeparatorHorizontal(BorderStyle border) {
  for (int y = y_min_ + 1; y <= y_max_ - 1; ++y) {
    for (int x = x_min_ + 1; x <= x_max_ - 1; ++x) {
      if (y % 2 == 0) {
        table_->elements_[y][x] =
            separatorCharacter(table_charset[border][4]) | automerge;  // NOLINT
      }
    }
  }
}

/// @brief Draw some horizontal separator lines in the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::SeparatorHorizontal(BorderStyle border,
                                         const Decorator& decorator) {
  SeparatorHorizontal(border);
  DecorateSeparatorHorizontal(decorator);
}

/// @brief Draw some separator lines to the left side of the selection.
/// @param border The border style to apply.
void TableSelection::BorderLeft(BorderStyle border) {
  for (int y = y_min_; y <= y_max_; y++) {
    table_->elements_[y][x_min_] =
        separatorCharacter(table_charset[border][5]) | automerge;  // NOLINT
  }
}

/// @brief Draw some separator lines to the left side of the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::BorderLeft(BorderStyle border,
                                const Decorator& decorator) {
  BorderLeft(border);
  DecorateBorderLeft(decorator);
}

/// @brief Draw some separator lines to the right side of the selection.
/// @param border The border style to apply.
void TableSelection::BorderRight(BorderStyle border) {
  for (int y = y_min_; y <= y_max_; y++) {
    table_->elements_[y][x_max_] =
        separatorCharacter(table_charset[border][5]) | automerge;  // NOLINT
  }
}

/// @brief Draw some separator lines to the right side of the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::BorderRight(BorderStyle border,
                                 const Decorator& decorator) {
  BorderRight(border);
  DecorateBorderRight(decorator);
}

/// @brief Draw some separator lines to the top side of the selection.
/// @param border The border style to apply.
void TableSelection::BorderTop(BorderStyle border) {
  for (int x = x_min_; x <= x_max_; x++) {
    table_->elements_[y_min_][x] =
        separatorCharacter(table_charset[border][4]) | automerge;  // NOLINT
  }
}

/// @brief Draw some separator lines to the top side of the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::BorderTop(BorderStyle border, const Decorator& decorator) {
  BorderTop(border);
  DecorateBorderTop(decorator);
}

/// @brief Draw some separator lines to the bottom side of the selection.
/// @param border The border style to apply.
void TableSelection::BorderBottom(BorderStyle border) {
  for (int x = x_min_; x <= x_max_; x++) {
    table_->elements_[y_max_][x] =
        separatorCharacter(table_charset[border][4]) | automerge;  // NOLINT
  }
}

/// @brief Draw some separator lines to the bottom side of the selection.
/// @param border The border style to apply.
/// @param decorator The decorator to apply.
void TableSelection::BorderBottom(BorderStyle border,
                                  const Decorator& decorator) {
  BorderBottom(border);
  DecorateBorderBottom(decorator);
}

}  // namespace ftxui

// ---- dom\text.cpp ------------------------------------------------


namespace ftxui {

namespace {
using ftxui::Screen;

class Text : public Node {
 public:
  explicit Text(std::string_view text) : glyphs_(Utf8ToGlyphs(text)) {
    int max_width = 0;
    int current_width = 0;
    int lines_count = 1;
    lines_offsets_.push_back(0);

    for (size_t i = 0; i < glyphs_.size(); ++i) {
      if (glyphs_[i] == "\n") {
        max_width = std::max(max_width, current_width);
        current_width = 0;
        lines_count++;
        lines_offsets_.push_back((int)i + 1);
      } else {
        current_width++;
      }
    }
    max_width = std::max(max_width, current_width);
    lines_offsets_.push_back((int)glyphs_.size() + 1);

    requirement_.min_x = max_width;
    requirement_.min_y = lines_count;
  }

  void ComputeRequirement() override {
    // The requirement was computed once in the constructor. This hook still
    // runs before every frame; use it to clear the selection, which Select()
    // re-populates while a selection is active.
    selection_rows_.clear();
  }

  void Select(Selection& selection) override {
    const Box selection_box = Box::Intersection(selection.GetBox(), box_);
    if (selection_box.IsEmpty()) {
      return;
    }

    // Only store the selected line range. Sizing per line would allocate one
    // entry per line of the whole text on every frame.
    const size_t lines_count = lines_offsets_.size() - 1;
    const size_t first = selection_box.y_min - box_.y_min;
    const size_t last =
        std::min<size_t>(selection_box.y_max - box_.y_min + 1, lines_count);
    if (first >= last) {
      return;
    }
    selection_first_line_ = first;
    selection_rows_.assign(last - first, {-1, -1});

    for (size_t i = first; i < last; ++i) {
      const int y = box_.y_min + (int)i;
      const Box row_box{box_.x_min, box_.x_max, y, y};
      const Selection row_sel = selection.SaturateHorizontal(row_box);
      const int sel_start = row_sel.GetBox().x_min;
      const int sel_end = row_sel.GetBox().x_max;
      selection_rows_[i - first] = {sel_start, sel_end};

      std::string part;
      int x = box_.x_min;
      const int start = lines_offsets_[i];
      const int end = lines_offsets_[i + 1] - 1;
      for (int j = start; j < end; ++j) {
        if (sel_start <= x && x <= sel_end) {
          part += glyphs_[j];
        }
        x++;
      }
      selection.AddPart(std::move(part), y, sel_start, sel_end);
    }
  }

  void Render(Screen& screen) override {
    const auto visible_box = Box::Intersection(screen.stencil, box_);
    if (visible_box.IsEmpty()) {
      return;
    }

    int y = visible_box.y_min;

    const size_t first_line = visible_box.y_min - box_.y_min;
    const size_t last_line = std::min<size_t>(
        visible_box.y_max - box_.y_min + 1, lines_offsets_.size() - 1);

    for (size_t line = first_line; line < last_line; ++line, ++y) {
      int x = box_.x_min;

      for (auto glyph = glyphs_.begin() + lines_offsets_[line];
           glyph != glyphs_.end() && *glyph != "\n"; ++glyph, ++x) {
        if (x > box_.x_max) {
          break;
        }

        auto& cell = screen.CellAt(x, y);
        cell.character = *glyph;

        const size_t sel_index = line - selection_first_line_;
        if (sel_index < selection_rows_.size()) {
          const auto& [sel_start, sel_end] = selection_rows_[sel_index];
          if (sel_start != -1 && x >= sel_start && x <= sel_end) {
            screen.GetSelectionStyle()(cell);
          }
        }
      }
    }
  }

 private:
  std::vector<std::string> glyphs_;
  std::vector<int> lines_offsets_;
  // Selection state for the line range [selection_first_line_,
  // selection_first_line_ + selection_rows_.size()).
  size_t selection_first_line_ = 0;
  std::vector<std::pair<int, int>> selection_rows_;
};

class VText : public Node {
 public:
  explicit VText(std::string_view text) : glyphs_(Utf8ToGlyphs(text)) {
    for (const auto& g : glyphs_) {
      if (g != "\n") {
        width_ = 1;
        break;
      }
    }
  }

  void ComputeRequirement() override {
    int max_height = 0;
    int current_height = 0;
    int columns = 1;

    for (const auto& cell : glyphs_) {
      if (cell == "\n") {
        max_height = std::max(max_height, current_height);
        current_height = 0;
        columns++;
      } else {
        current_height++;
      }
    }
    max_height = std::max(max_height, current_height);

    requirement_.min_x = width_ * columns;
    requirement_.min_y = max_height;
  }

  void Render(Screen& screen) override {
    int x = box_.x_min;
    int y = box_.y_min;
    if (x + width_ - 1 > box_.x_max) {
      return;
    }
    for (const auto& it : glyphs_) {
      if (it == "\n") {
        x += width_;
        y = box_.y_min;
        if (x + width_ - 1 > box_.x_max) {
          return;
        }
        continue;
      }
      if (y > box_.y_max) {
        continue;
      }
      screen.CellAt(x, y).character = it;
      y += 1;
    }
  }

 private:
  std::vector<std::string> glyphs_;
  int width_ = 0;
};

}  // namespace

/// @brief Display a piece of UTF8 encoded unicode text.
/// @ingroup dom
/// @see ftxui::to_wstring
///
/// ### Example
///
/// ```cpp
/// Element document = text("Hello world!");
/// ```
///
/// ### Output
///
/// ```bash
/// Hello world!
/// ```
Element text(std::string_view text) {
  return std::make_shared<Text>(std::string(text));
}

/// @brief Display a piece of unicode text.
/// @ingroup dom
/// @see ftxui::to_wstring
///
/// ### Example
///
/// ```cpp
/// Element document = text(L"Hello world!");
/// ```
///
/// ### Output
///
/// ```bash
/// Hello world!
/// ```
Element text(std::wstring_view text) {
  return ftxui::text(to_string(text));
}

/// @brief Display a piece of unicode text vertically.
/// @ingroup dom
/// @see ftxui::to_wstring
///
/// ### Example
///
/// ```cpp
/// Element document = vtext("Hello world!");
/// ```
///
/// ### Output
///
/// ```bash
/// H
/// e
/// l
/// l
/// o
///
/// w
/// o
/// r
/// l
/// d
/// !
/// ```
Element vtext(std::string_view text) {
  return std::make_shared<VText>(text);
}

/// @brief Display a piece unicode text vertically.
/// @ingroup dom
/// @see ftxui::to_wstring
///
/// ### Example
///
/// ```cpp
/// Element document = vtext(L"Hello world!");
/// ```
///
/// ### Output
///
/// ```bash
/// H
/// e
/// l
/// l
/// o
///
/// w
/// o
/// r
/// l
/// d
/// !
/// ```
Element vtext(std::wstring_view text) {  // NOLINT
  return vtext(to_string(text));
}

}  // namespace ftxui

// ---- dom\underlined.cpp ------------------------------------------


namespace ftxui {

namespace {
class Underlined : public NodeDecorator {
 public:
  using NodeDecorator::NodeDecorator;

  void Render(Screen& screen) override {
    Node::Render(screen);
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, y).underlined = true;
      }
    }
  }
};
}  // namespace

/// @brief Underline the given element.
/// @ingroup dom
Element underlined(Element child) {
  return std::make_shared<Underlined>(std::move(child));
}

}  // namespace ftxui

// ---- dom\underlined_double.cpp -----------------------------------


namespace ftxui {

/// @brief Apply a underlinedDouble to text.
/// @ingroup dom
Element underlinedDouble(Element child) {
  class Impl : public NodeDecorator {
   public:
    using NodeDecorator::NodeDecorator;

    void Render(Screen& screen) override {
      for (int y = box_.y_min; y <= box_.y_max; ++y) {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
          screen.CellAt(x, y).underlined_double = true;
        }
      }
      Node::Render(screen);
    }
  };

  return std::make_shared<Impl>(std::move(child));
}

}  // namespace ftxui

// ---- dom\util.cpp ------------------------------------------------


namespace ftxui {

namespace {
Decorator compose(Decorator a, Decorator b) {
  return [a = std::move(a), b = std::move(b)](Element element) {
    return b(a(std::move(element)));
  };
}
}  // namespace

/// @brief A decoration doing absolutely nothing.
/// @ingroup dom
Element nothing(Element element) {
  return element;
}

/// @brief Compose two decorator into one.
/// @ingroup dom
///
/// ### Example
///
/// ```cpp
/// auto decorator = bold | blink;
/// ```
Decorator operator|(Decorator a, Decorator b) {
  return compose(std::move(a),  //
                 std::move(b));
}

/// @brief From a set of element, apply a decorator to every elements.
/// @return the set of decorated element.
/// @ingroup dom
Elements operator|(Elements elements, Decorator decorator) {  // NOLINT
  Elements output;
  output.reserve(elements.size());
  for (auto& it : elements) {
    output.push_back(std::move(it) | decorator);
  }
  return output;
}

/// @brief From an element, apply a decorator.
/// @return the decorated element.
/// @ingroup dom
///
/// ### Example
///
/// Both of these are equivalent:
/// ```cpp
/// bold(text("Hello"));
/// ```
/// ```cpp
/// text("Hello") | bold;
/// ```
Element operator|(Element element, Decorator decorator) {  // NOLINT
  return decorator(std::move(element));
}

/// @brief Apply a decorator to an element.
/// @return the decorated element.
/// @ingroup dom
///
/// ### Example
///
/// Both of these are equivalent:
/// ```cpp
/// auto element = text("Hello");
/// element |= bold;
/// ```
Element& operator|=(Element& e, Decorator d) {
  e = e | std::move(d);
  return e;
}

/// The minimal dimension that will fit the given element.
/// @see Fixed
/// @see Full
Dimensions Dimension::Fit(Element& e, bool extend_beyond_screen) {
  const Dimensions fullsize = Dimension::Full();
  Box box;
  box.x_min = 0;
  box.y_min = 0;
  box.x_max = fullsize.dimx;
  box.y_max = fullsize.dimy;

  Node::Status status;
  e->Check(&status);
  const int max_iteration = 20;
  while (status.need_iteration && status.iteration < max_iteration) {
    e->ComputeRequirement();

    // Don't give the element more space than it needs:
    box.x_max = std::min(box.x_max, e->requirement().min_x);
    box.y_max = e->requirement().min_y;
    if (!extend_beyond_screen) {
      box.y_max = std::min(box.y_max, fullsize.dimy);
    }

    e->SetBox(box);
    status.need_iteration = false;
    status.iteration++;
    e->Check(&status);

    if (!status.need_iteration) {
      break;
    }
    // Increase the size of the box until it fits...
    box.x_max = std::min(e->requirement().min_x, fullsize.dimx);
    box.y_max = e->requirement().min_y;

    // ... but don't go beyond the screen size:
    if (!extend_beyond_screen) {
      box.y_max = std::min(box.y_max, fullsize.dimy);
    }
  }

  return {
      box.x_max,
      box.y_max,
  };
}

/// An element of size 0x0 drawing nothing.
/// @ingroup dom
Element emptyElement() {
  class Impl : public Node {
    void ComputeRequirement() override {
      requirement_.min_x = 0;
      requirement_.min_y = 0;
    }
  };
  return std::make_unique<Impl>();
}

}  // namespace ftxui

// ---- dom\vbox.cpp ------------------------------------------------


namespace ftxui {

namespace {
class VBox : public Node {
 public:
  explicit VBox(Elements children) : Node(std::move(children)) {}

 private:
  void ComputeRequirement() override {
    requirement_ = Requirement{};

    for (auto& child : children_) {
      child->ComputeRequirement();

      // Propagate the focused requirement.
      if (requirement_.focused.Prefer(child->requirement().focused)) {
        requirement_.focused = child->requirement().focused;
        requirement_.focused.box.Shift(0, requirement_.min_y);
      }

      // Extend the min_x and min_y to contain all the children
      requirement_.min_y += child->requirement().min_y;
      requirement_.min_x =
          std::max(requirement_.min_x, child->requirement().min_x);
    }
  }

  void SetBox(Box box) override {
    Node::SetBox(box);

    std::vector<box_helper::Element> elements(children_.size());
    for (size_t i = 0; i < children_.size(); ++i) {
      auto& element = elements[i];
      const auto& requirement = children_[i]->requirement();
      element.min_size = requirement.min_y;
      element.flex_grow = requirement.flex_grow_y;
      element.flex_shrink = requirement.flex_shrink_y;
    }
    const int target_size = box.y_max - box.y_min + 1;
    box_helper::Compute(&elements, target_size);

    int y = box.y_min;
    for (size_t i = 0; i < children_.size(); ++i) {
      box.y_min = y;
      box.y_max = y + elements[i].size - 1;
      children_[i]->SetBox(box);
      y = box.y_max + 1;
    }
  }

  void Select(Selection& selection) override {
    // If this Node box_ doesn't intersect with the selection, then no
    // selection.
    if (Box::Intersection(selection.GetBox(), box_).IsEmpty()) {
      return;
    }

    Selection selection_saturated = selection.SaturateVertical(box_);

    for (auto& child : children_) {
      child->Select(selection_saturated);
    }
  }
};
}  // namespace

/// @brief A container displaying elements vertically one by one.
/// @param children The elements in the container
/// @return The container.
/// @ingroup dom
///
/// #### Example
///
/// ```cpp
/// vbox({
///   text("Up"),
///   text("Down"),
/// });
/// ```
Element vbox(Elements children) {
  return std::make_shared<VBox>(std::move(children));
}

}  // namespace ftxui

// ---- screen\box.cpp ----------------------------------------------


namespace ftxui {
/// @return the biggest Box contained in both |a| and |b|.
// static
Box Box::Intersection(Box a, Box b) {
  return Box{
      std::max(a.x_min, b.x_min),
      std::min(a.x_max, b.x_max),
      std::max(a.y_min, b.y_min),
      std::min(a.y_max, b.y_max),
  };
}

/// @return the smallest Box containing both |a| and |b|.
// static
Box Box::Union(Box a, Box b) {
  return Box{
      std::min(a.x_min, b.x_min),
      std::max(a.x_max, b.x_max),
      std::min(a.y_min, b.y_min),
      std::max(a.y_max, b.y_max),
  };
}

/// Shift the box by (x,y).
/// @param x horizontal shift.
/// @param y vertical shift.
void Box::Shift(int x, int y) {
  x_min += x;
  x_max += x;
  y_min += y;
  y_max += y;
}

/// @return whether (x,y) is contained inside the box.
bool Box::Contain(int x, int y) const {
  return x_min <= x &&  //
         x_max >= x &&  //
         y_min <= y &&  //
         y_max >= y;
}

/// @return whether the box is empty.
bool Box::IsEmpty() const {
  return x_min > x_max || y_min > y_max;
}

/// @return whether |other| is the same as |this|
bool Box::operator==(const Box& other) const {
  return (x_min == other.x_min) && (x_max == other.x_max) &&
         (y_min == other.y_min) && (y_max == other.y_max);
}

/// @return whether |other| and |this| are different.
bool Box::operator!=(const Box& other) const {
  return !operator==(other);
}

}  // namespace ftxui

// ---- screen\color.cpp --------------------------------------------



#if defined(_MSC_VER)
#define FTXUI_NOINLINE __declspec(noinline)
#else
#define FTXUI_NOINLINE __attribute__((noinline))
#endif

namespace ftxui {
namespace {
const std::array<const char*, 32> palette16code = {
    "30", "40",   //
    "31", "41",   //
    "32", "42",   //
    "33", "43",   //
    "34", "44",   //
    "35", "45",   //
    "36", "46",   //
    "37", "47",   //
    "90", "100",  //
    "91", "101",  //
    "92", "102",  //
    "93", "103",  //
    "94", "104",  //
    "95", "105",  //
    "96", "106",  //
    "97", "107",  //
};

void AppendNumber(std::string& out, uint8_t n) {
  if (n >= 100) {
    out += static_cast<char>('0' + n / 100);
    n %= 100;
    out += static_cast<char>('0' + n / 10);
    n %= 10;
    out += static_cast<char>('0' + n);
  } else if (n >= 10) {
    out += static_cast<char>('0' + n / 10);
    n %= 10;
    out += static_cast<char>('0' + n);
  } else {
    out += static_cast<char>('0' + n);
  }
}

// Index of the Palette256 color closest to (r, g, b), matching an exhaustive
// search over [16, 255]: ties resolve to the lowest index.
uint8_t ClosestPalette256(int r, int g, int b) {
  // Colors [16, 231] form a 6x6x6 cube of levels {0, 95, 135, 175, 215, 255}.
  auto cube_index = [](int v) {
    return v < 48 ? 0 : v < 116 ? 1 : (v - 36) / 40;
  };
  auto cube_level = [](int i) { return i == 0 ? 0 : 55 + 40 * i; };
  const int ri = cube_index(r);
  const int gi = cube_index(g);
  const int bi = cube_index(b);
  const int cr = cube_level(ri) - r;
  const int cg = cube_level(gi) - g;
  const int cb = cube_level(bi) - b;
  const int cube_distance = cr * cr + cg * cg + cb * cb;

  // Colors [232, 255] are the grays {8, 18, ..., 238}. The closest one is the
  // closest to the mean (r + g + b) / 3.
  const int gray_index = std::clamp((r + g + b - 10) / 30, 0, 23);
  const int gray_level = 8 + 10 * gray_index;
  const int dr = gray_level - r;
  const int dg = gray_level - g;
  const int db = gray_level - b;
  const int gray_distance = dr * dr + dg * dg + db * db;

  return gray_distance < cube_distance ? 232 + gray_index
                                       : 16 + 36 * ri + 6 * gi + bi;
}

// Print the color the terminal supports closest to either the RGB color
// (r, g, b), or the Palette256 color r. Kept out of line, so that printing
// supported colors stays fast.
FTXUI_NOINLINE void PrintDegraded(std::string& out,
                                  bool is_background_color,
                                  bool is_rgb,
                                  uint8_t r,
                                  uint8_t g,
                                  uint8_t b,
                                  Terminal::Color support) {
  if (support == Terminal::Color::Palette1) {
    out.append(is_background_color ? "49" : "39", 2);
    return;
  }
  const uint8_t index = is_rgb ? ClosestPalette256(r, g, b) : r;
  if (support == Terminal::Color::Palette256) {
    out.append(is_background_color ? "48;5;" : "38;5;", 5);
    AppendNumber(out, index);
    return;
  }
  const uint8_t index_16 = GetColorInfo(Color::Palette256(index)).index_16;
  out.append(palette16code[2 * index_16 + (is_background_color ? 1 : 0)]);
}

}  // namespace

bool Color::operator==(const Color& rhs) const {
  return red_ == rhs.red_ && green_ == rhs.green_ && blue_ == rhs.blue_ &&
         type_ == rhs.type_;
}

bool Color::operator!=(const Color& rhs) const {
  return !operator==(rhs);
}

std::string Color::Print(bool is_background_color) const {
  std::string out;
  PrintTo(out, is_background_color);
  return out;
}

/// @brief Append the ANSI color code to a string (zero-allocation fast path).
/// @param out The string to append to.
/// @param is_background_color Whether this is a background color code.
void Color::PrintTo(std::string& out, bool is_background_color) const {
  PrintTo(out, is_background_color, Terminal::ColorSupport());
}

/// @brief Append the ANSI color code to a string, degraded to the given
/// terminal color support.
/// @param out The string to append to.
/// @param is_background_color Whether this is a background color code.
/// @param support The terminal color support.
void Color::PrintTo(std::string& out,
                    bool is_background_color,
                    Terminal::Color support) const {
  // Degrade the color to what the terminal supports. Both enums are ordered
  // the same way.
  if (static_cast<uint8_t>(type_) > static_cast<uint8_t>(support)) {
    PrintDegraded(out, is_background_color, type_ == ColorType::TrueColor, red_,
                  green_, blue_, support);
    return;
  }

  switch (type_) {
    case ColorType::Palette1:
      out.append(is_background_color ? "49" : "39", 2);
      return;
    case ColorType::Palette16:
      out.append(palette16code[2 * red_ + (is_background_color ? 1 : 0)]);
      return;
    case ColorType::Palette256:
      out.append(is_background_color ? "48;5;" : "38;5;", 5);
      AppendNumber(out, red_);
      return;
    case ColorType::TrueColor:
      out.append(is_background_color ? "48;2;" : "38;2;", 5);
      AppendNumber(out, red_);
      out += ';';
      AppendNumber(out, green_);
      out += ';';
      AppendNumber(out, blue_);
      return;
  }
}

/// @brief Build a transparent color.
Color::Color() = default;

/// @brief Build a transparent color.
Color::Color(Palette1 /*value*/) : Color() {}

/// @brief Build a color using the Palette16 colors.
Color::Color(Palette16 index)
    : type_(ColorType::Palette16), red_(index), alpha_(255) {}

/// @brief Build a color using Palette256 colors.
Color::Color(Palette256 index)
    : type_(ColorType::Palette256), red_(index), alpha_(255) {}

/// @brief Build a Color from its RGB representation.
/// https://en.wikipedia.org/wiki/RGB_color_model
///
/// @param red The quantity of red [0,255]
/// @param green The quantity of green [0,255]
/// @param blue The quantity of blue [0,255]
/// @param alpha The quantity of alpha [0,255]
Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    : type_(ColorType::TrueColor),
      red_(red),
      green_(green),
      blue_(blue),
      alpha_(alpha) {}

/// @brief Build a Color from its RGB representation.
/// https://en.wikipedia.org/wiki/RGB_color_model
///
/// @param red The quantity of red [0,255]
/// @param green The quantity of green [0,255]
/// @param blue The quantity of blue [0,255]
// static
Color Color::RGB(uint8_t red, uint8_t green, uint8_t blue) {
  return RGBA(red, green, blue, 255);
}

/// @brief Build a Color from its RGBA representation.
/// https://en.wikipedia.org/wiki/RGB_color_model
/// @param red The quantity of red [0,255]
/// @param green The quantity of green [0,255]
/// @param blue The quantity of blue [0,255]
/// @param alpha The quantity of alpha [0,255]
/// @see Color::RGB
// static
Color Color::RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
  return {red, green, blue, alpha};
}

/// @brief Build a Color from its HSV representation.
/// https://en.wikipedia.org/wiki/HSL_and_HSV
///
/// @param h The hue of the color [0,255]
/// @param s The "colorfulness" [0,255].
/// @param v The "Lightness" [0,255]
/// @param alpha The quantity of alpha [0,255]
// static
Color Color::HSVA(uint8_t h, uint8_t s, uint8_t v, uint8_t alpha) {
  uint8_t region = h / 43;                                        // NOLINT
  uint8_t remainder = (h - (region * 43)) * 6;                    // NOLINT
  uint8_t p = (v * (255 - s)) >> 8;                               // NOLINT
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;          // NOLINT
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;  // NOLINT

  // clang-format off
  switch (region) {                     // NOLINT
    case 0: return Color(v,t,p, alpha); // NOLINT
    case 1: return Color(q,v,p, alpha); // NOLINT
    case 2: return Color(p,v,t, alpha); // NOLINT
    case 3: return Color(p,q,v, alpha); // NOLINT
    case 4: return Color(t,p,v, alpha); // NOLINT
    case 5: return Color(v,p,q, alpha); // NOLINT
  }                                     // NOLINT
  // clang-format on
  return {0, 0, 0, alpha};
}

/// @brief Build a Color from its HSV representation.
/// https://en.wikipedia.org/wiki/HSL_and_HSV
///
/// @param h The hue of the color [0,255]
/// @param s The "colorfulness" [0,255].
/// @param v The "Lightness" [0,255]
// static
Color Color::HSV(uint8_t h, uint8_t s, uint8_t v) {
  return HSVA(h, s, v, 255);
}

// static
Color Color::Interpolate(float t, const Color& a, const Color& b) {
  if (a.type_ == ColorType::Palette1 ||  //
      b.type_ == ColorType::Palette1) {
    if (t < 0.5F) {  // NOLINT
      return a;
    } else {
      return b;
    }
  }

  // Gamma correction:
  // https://en.wikipedia.org/wiki/Gamma_correction
  auto interp = [t](uint8_t a_u, uint8_t b_u) {
    constexpr float gamma = 2.2F;
    const float a_f = std::pow(a_u, gamma);
    const float b_f = std::pow(b_u, gamma);
    const float c_f = a_f * (1.0F - t) +  //
                      b_f * t;
    return static_cast<uint8_t>(std::pow(c_f, 1.F / gamma));
  };
  return Color::RGB(interp(a.GetRed(), b.GetRed()),      //
                    interp(a.GetGreen(), b.GetGreen()),  //
                    interp(a.GetBlue(), b.GetBlue()));   //
}

/// @brief The red component of the color [0,255].
/// Palette colors are resolved to their RGB approximation. Returns 0 for the
/// default (transparent) color.
uint8_t Color::GetRed() const {
  switch (type_) {
    case ColorType::Palette1:
      return 0;
    case ColorType::Palette16:
    case ColorType::Palette256:
      return GetColorInfo(Palette256(red_)).red;
    case ColorType::TrueColor:
      return red_;
  }
  return 0;
}

/// @brief The green component of the color [0,255].
/// Palette colors are resolved to their RGB approximation. Returns 0 for the
/// default (transparent) color.
uint8_t Color::GetGreen() const {
  switch (type_) {
    case ColorType::Palette1:
      return 0;
    case ColorType::Palette16:
    case ColorType::Palette256:
      return GetColorInfo(Palette256(red_)).green;
    case ColorType::TrueColor:
      return green_;
  }
  return 0;
}

/// @brief The blue component of the color [0,255].
/// Palette colors are resolved to their RGB approximation. Returns 0 for the
/// default (transparent) color.
uint8_t Color::GetBlue() const {
  switch (type_) {
    case ColorType::Palette1:
      return 0;
    case ColorType::Palette16:
    case ColorType::Palette256:
      return GetColorInfo(Palette256(red_)).blue;
    case ColorType::TrueColor:
      return blue_;
  }
  return 0;
}

/// @brief The alpha component of the color [0,255]. 0 is fully transparent.
uint8_t Color::GetAlpha() const {
  return alpha_;
}

/// @brief Blend two colors together using the alpha channel.
// static
Color Color::Blend(const Color& lhs, const Color& rhs) {
  Color out = Interpolate(float(rhs.alpha_) / 255.F, lhs, rhs);
  out.alpha_ = lhs.alpha_ + rhs.alpha_ - lhs.alpha_ * rhs.alpha_ / 255;
  return out;
}

inline namespace literals {

Color operator""_rgb(unsigned long long int combined) {
  // assert(combined <= 0xffffffU);
  auto const red = static_cast<uint8_t>(combined >> 16U);
  auto const green = static_cast<uint8_t>(combined >> 8U);
  auto const blue = static_cast<uint8_t>(combined);
  return {red, green, blue};
}

}  // namespace literals

}  // namespace ftxui

// ---- screen\color_info.cpp ---------------------------------------



namespace ftxui {

// clang-format off
const std::array<ColorInfo, 256> palette256 = {{
     {"Black"              , 0   , 0  , 0   , 0   , 0   , 0   , 0   , 0   } ,
     {"Red"                , 1   , 1  , 128 , 0   , 0   , 0   , 255 , 128 } ,
     {"Green"              , 2   , 2  , 0   , 128 , 0   , 85  , 255 , 128 } ,
     {"Yellow"             , 3   , 3  , 128 , 128 , 0   , 43  , 255 , 128 } ,
     {"Blue"               , 4   , 4  , 0   , 0   , 128 , 171 , 255 , 128 } ,
     {"Magenta"            , 5   , 5  , 128 , 0   , 128 , 213 , 255 , 128 } ,
     {"Cyan"               , 6   , 6  , 0   , 128 , 128 , 128 , 255 , 128 } ,
     {"GrayLight"          , 7   , 7  , 192 , 192 , 192 , 0   , 0   , 192 } ,
     {"GrayDark"           , 8   , 8  , 128 , 128 , 128 , 0   , 0   , 128 } ,
     {"RedLight"           , 9   , 9  , 255 , 0   , 0   , 0   , 255 , 255 } ,
     {"GreenLight"         , 10  , 10 , 0   , 255 , 0   , 85  , 255 , 255 } ,
     {"YellowLight"        , 11  , 11 , 255 , 255 , 0   , 43  , 255 , 255 } ,
     {"BlueLight"          , 12  , 12 , 0   , 0   , 255 , 171 , 255 , 255 } ,
     {"MagentaLight"       , 13  , 13 , 255 , 0   , 255 , 213 , 255 , 255 } ,
     {"CyanLight"          , 14  , 14 , 0   , 255 , 255 , 128 , 255 , 255 } ,
     {"White"              , 15  , 15 , 255 , 255 , 255 , 0   , 0   , 255 } ,
     {"Grey0"              , 16  , 0  , 0   , 0   , 0   , 0   , 0   , 0   } ,
     {"NavyBlue"           , 17  , 4  , 0   , 0   , 95  , 171 , 255 , 95  } ,
     {"DarkBlue"           , 18  , 4  , 0   , 0   , 135 , 171 , 255 , 135 } ,
     {"Blue3"              , 19  , 4  , 0   , 0   , 175 , 171 , 255 , 175 } ,
     {"Blue3Bis"           , 20  , 12 , 0   , 0   , 215 , 171 , 255 , 215 } ,
     {"Blue1"              , 21  , 12 , 0   , 0   , 255 , 171 , 255 , 255 } ,
     {"DarkGreen"          , 22  , 2  , 0   , 95  , 0   , 85  , 255 , 95  } ,
     {"DeepSkyBlue4"       , 23  , 6  , 0   , 95  , 95  , 128 , 255 , 95  } ,
     {"DeepSkyBlue4Bis"    , 24  , 6  , 0   , 95  , 135 , 141 , 255 , 135 } ,
     {"DeepSkyBlue4Ter"    , 25  , 6  , 0   , 95  , 175 , 148 , 255 , 175 } ,
     {"DodgerBlue3"        , 26  , 12 , 0   , 95  , 215 , 152 , 255 , 215 } ,
     {"DodgerBlue2"        , 27  , 12 , 0   , 95  , 255 , 155 , 255 , 255 } ,
     {"Green4"             , 28  , 2  , 0   , 135 , 0   , 85  , 255 , 135 } ,
     {"SpringGreen4"       , 29  , 6  , 0   , 135 , 95  , 115 , 255 , 135 } ,
     {"Turquoise4"         , 30  , 6  , 0   , 135 , 135 , 128 , 255 , 135 } ,
     {"DeepSkyBlue3"       , 31  , 6  , 0   , 135 , 175 , 138 , 255 , 175 } ,
     {"DeepSkyBlue3Bis"    , 32  , 14 , 0   , 135 , 215 , 144 , 255 , 215 } ,
     {"DodgerBlue1"        , 33  , 14 , 0   , 135 , 255 , 149 , 255 , 255 } ,
     {"Green3"             , 34  , 2  , 0   , 175 , 0   , 85  , 255 , 175 } ,
     {"SpringGreen3"       , 35  , 6  , 0   , 175 , 95  , 108 , 255 , 175 } ,
     {"DarkCyan"           , 36  , 6  , 0   , 175 , 135 , 118 , 255 , 175 } ,
     {"LightSeaGreen"      , 37  , 6  , 0   , 175 , 175 , 128 , 255 , 175 } ,
     {"DeepSkyBlue2"       , 38  , 14 , 0   , 175 , 215 , 136 , 255 , 215 } ,
     {"DeepSkyBlue1"       , 39  , 14 , 0   , 175 , 255 , 142 , 255 , 255 } ,
     {"Green3Bis"          , 40  , 10 , 0   , 215 , 0   , 85  , 255 , 215 } ,
     {"SpringGreen3Bis"    , 41  , 10 , 0   , 215 , 95  , 104 , 255 , 215 } ,
     {"SpringGreen2"       , 42  , 14 , 0   , 215 , 135 , 112 , 255 , 215 } ,
     {"Cyan3"              , 43  , 14 , 0   , 215 , 175 , 120 , 255 , 215 } ,
     {"DarkTurquoise"      , 44  , 14 , 0   , 215 , 215 , 128 , 255 , 215 } ,
     {"Turquoise2"         , 45  , 14 , 0   , 215 , 255 , 135 , 255 , 255 } ,
     {"Green1"             , 46  , 10 , 0   , 255 , 0   , 85  , 255 , 255 } ,
     {"SpringGreen2Bis"    , 47  , 10 , 0   , 255 , 95  , 101 , 255 , 255 } ,
     {"SpringGreen1"       , 48  , 14 , 0   , 255 , 135 , 107 , 255 , 255 } ,
     {"MediumSpringGreen"  , 49  , 14 , 0   , 255 , 175 , 114 , 255 , 255 } ,
     {"Cyan2"              , 50  , 14 , 0   , 255 , 215 , 121 , 255 , 255 } ,
     {"Cyan1"              , 51  , 14 , 0   , 255 , 255 , 128 , 255 , 255 } ,
     {"DarkRed"            , 52  , 1  , 95  , 0   , 0   , 0   , 255 , 95  } ,
     {"DeepPink4Ter"       , 53  , 5  , 95  , 0   , 95  , 213 , 255 , 95  } ,
     {"Purple4"            , 54  , 5  , 95  , 0   , 135 , 201 , 255 , 135 } ,
     {"Purple4Bis"         , 55  , 5  , 95  , 0   , 175 , 194 , 255 , 175 } ,
     {"Purple3"            , 56  , 12 , 95  , 0   , 215 , 190 , 255 , 215 } ,
     {"BlueViolet"         , 57  , 12 , 95  , 0   , 255 , 187 , 255 , 255 } ,
     {"Orange4"            , 58  , 3  , 95  , 95  , 0   , 43  , 255 , 95  } ,
     {"Grey37"             , 59  , 8  , 95  , 95  , 95  , 0   , 0   , 95  } ,
     {"MediumPurple4"      , 60  , 4  , 95  , 95  , 135 , 171 , 75  , 135 } ,
     {"SlateBlue3"         , 61  , 4  , 95  , 95  , 175 , 171 , 116 , 175 } ,
     {"SlateBlue3Bis"      , 62  , 12 , 95  , 95  , 215 , 171 , 142 , 215 } ,
     {"RoyalBlue1"         , 63  , 12 , 95  , 95  , 255 , 171 , 160 , 255 } ,
     {"Chartreuse4"        , 64  , 3  , 95  , 135 , 0   , 55  , 255 , 135 } ,
     {"DarkSeaGreen4"      , 65  , 8  , 95  , 135 , 95  , 85  , 75  , 135 } ,
     {"PaleTurquoise4"     , 66  , 6  , 95  , 135 , 135 , 128 , 75  , 135 } ,
     {"SteelBlue"          , 67  , 4  , 95  , 135 , 175 , 150 , 116 , 175 } ,
     {"SteelBlue3"         , 68  , 12 , 95  , 135 , 215 , 157 , 142 , 215 } ,
     {"CornflowerBlue"     , 69  , 12 , 95  , 135 , 255 , 161 , 160 , 255 } ,
     {"Chartreuse3"        , 70  , 3  , 95  , 175 , 0   , 62  , 255 , 175 } ,
     {"DarkSeaGreen4Bis"   , 71  , 2  , 95  , 175 , 95  , 85  , 116 , 175 } ,
     {"CadetBlue"          , 72  , 2  , 95  , 175 , 135 , 106 , 116 , 175 } ,
     {"CadetBlueBis"       , 73  , 6  , 95  , 175 , 175 , 128 , 116 , 175 } ,
     {"SkyBlue3"           , 74  , 14 , 95  , 175 , 215 , 143 , 142 , 215 } ,
     {"SteelBlue1"         , 75  , 12 , 95  , 175 , 255 , 150 , 160 , 255 } ,
     {"Chartreuse3Bis"     , 76  , 10 , 95  , 215 , 0   , 66  , 255 , 215 } ,
     {"PaleGreen3Bis"      , 77  , 10 , 95  , 215 , 95  , 85  , 142 , 215 } ,
     {"SeaGreen3"          , 78  , 10 , 95  , 215 , 135 , 99  , 142 , 215 } ,
     {"Aquamarine3"        , 79  , 14 , 95  , 215 , 175 , 113 , 142 , 215 } ,
     {"MediumTurquoise"    , 80  , 14 , 95  , 215 , 215 , 128 , 142 , 215 } ,
     {"SteelBlue1Bis"      , 81  , 14 , 95  , 215 , 255 , 139 , 160 , 255 } ,
     {"Chartreuse2Bis"     , 82  , 10 , 95  , 255 , 0   , 69  , 255 , 255 } ,
     {"SeaGreen2"          , 83  , 10 , 95  , 255 , 95  , 85  , 160 , 255 } ,
     {"SeaGreen1"          , 84  , 10 , 95  , 255 , 135 , 95  , 160 , 255 } ,
     {"SeaGreen1Bis"       , 85  , 10 , 95  , 255 , 175 , 106 , 160 , 255 } ,
     {"Aquamarine1Bis"     , 86  , 14 , 95  , 255 , 215 , 117 , 160 , 255 } ,
     {"DarkSlateGray2"     , 87  , 14 , 95  , 255 , 255 , 128 , 160 , 255 } ,
     {"DarkRedBis"         , 88  , 1  , 135 , 0   , 0   , 0   , 255 , 135 } ,
     {"DeepPink4Bis"       , 89  , 5  , 135 , 0   , 95  , 226 , 255 , 135 } ,
     {"DarkMagenta"        , 90  , 5  , 135 , 0   , 135 , 213 , 255 , 135 } ,
     {"DarkMagentaBis"     , 91  , 5  , 135 , 0   , 175 , 204 , 255 , 175 } ,
     {"DarkVioletBis"      , 92  , 13 , 135 , 0   , 215 , 198 , 255 , 215 } ,
     {"PurpleBis"          , 93  , 13 , 135 , 0   , 255 , 193 , 255 , 255 } ,
     {"Orange4Bis"         , 94  , 3  , 135 , 95  , 0   , 30  , 255 , 135 } ,
     {"LightPink4"         , 95  , 8  , 135 , 95  , 95  , 0   , 75  , 135 } ,
     {"Plum4"              , 96  , 5  , 135 , 95  , 135 , 213 , 75  , 135 } ,
     {"MediumPurple3"      , 97  , 4  , 135 , 95  , 175 , 192 , 116 , 175 } ,
     {"MediumPurple3Bis"   , 98  , 12 , 135 , 95  , 215 , 185 , 142 , 215 } ,
     {"SlateBlue1"         , 99  , 12 , 135 , 95  , 255 , 181 , 160 , 255 } ,
     {"Yellow4"            , 100 , 3  , 135 , 135 , 0   , 43  , 255 , 135 } ,
     {"Wheat4"             , 101 , 8  , 135 , 135 , 95  , 43  , 75  , 135 } ,
     {"Grey53"             , 102 , 8  , 135 , 135 , 135 , 0   , 0   , 135 } ,
     {"LightSlateGrey"     , 103 , 4  , 135 , 135 , 175 , 171 , 58  , 175 } ,
     {"MediumPurple"       , 104 , 12 , 135 , 135 , 215 , 171 , 94  , 215 } ,
     {"LightSlateBlue"     , 105 , 12 , 135 , 135 , 255 , 171 , 120 , 255 } ,
     {"Yellow4Bis"         , 106 , 3  , 135 , 175 , 0   , 52  , 255 , 175 } ,
     {"DarkOliveGreen3"    , 107 , 7  , 135 , 175 , 95  , 64  , 116 , 175 } ,
     {"DarkSeaGreen"       , 108 , 7  , 135 , 175 , 135 , 85  , 58  , 175 } ,
     {"LightSkyBlue3"      , 109 , 7  , 135 , 175 , 175 , 128 , 58  , 175 } ,
     {"LightSkyBlue3Bis"   , 110 , 12 , 135 , 175 , 215 , 150 , 94  , 215 } ,
     {"SkyBlue2"           , 111 , 12 , 135 , 175 , 255 , 157 , 120 , 255 } ,
     {"Chartreuse2"        , 112 , 11 , 135 , 215 , 0   , 58  , 255 , 215 } ,
     {"DarkOliveGreen3Bis" , 113 , 10 , 135 , 215 , 95  , 71  , 142 , 215 } ,
     {"PaleGreen3"         , 114 , 7  , 135 , 215 , 135 , 85  , 94  , 215 } ,
     {"DarkSeaGreen3"      , 115 , 10 , 135 , 215 , 175 , 106 , 94  , 215 } ,
     {"DarkSlateGray3"     , 116 , 14 , 135 , 215 , 215 , 128 , 94  , 215 } ,
     {"SkyBlue1"           , 117 , 14 , 135 , 215 , 255 , 143 , 120 , 255 } ,
     {"Chartreuse1"        , 118 , 11 , 135 , 255 , 0   , 63  , 255 , 255 } ,
     {"LightGreen"         , 119 , 10 , 135 , 255 , 95  , 75  , 160 , 255 } ,
     {"LightGreenBis"      , 120 , 10 , 135 , 255 , 135 , 85  , 120 , 255 } ,
     {"PaleGreen1"         , 121 , 10 , 135 , 255 , 175 , 99  , 120 , 255 } ,
     {"Aquamarine1"        , 122 , 14 , 135 , 255 , 215 , 113 , 120 , 255 } ,
     {"DarkSlateGray1"     , 123 , 14 , 135 , 255 , 255 , 128 , 120 , 255 } ,
     {"Red3"               , 124 , 1  , 175 , 0   , 0   , 0   , 255 , 175 } ,
     {"DeepPink4"          , 125 , 5  , 175 , 0   , 95  , 233 , 255 , 175 } ,
     {"MediumVioletRed"    , 126 , 5  , 175 , 0   , 135 , 223 , 255 , 175 } ,
     {"Magenta3"           , 127 , 5  , 175 , 0   , 175 , 213 , 255 , 175 } ,
     {"DarkViolet"         , 128 , 13 , 175 , 0   , 215 , 206 , 255 , 215 } ,
     {"Purple"             , 129 , 13 , 175 , 0   , 255 , 200 , 255 , 255 } ,
     {"DarkOrange3"        , 130 , 3  , 175 , 95  , 0   , 23  , 255 , 175 } ,
     {"IndianRed"          , 131 , 7  , 175 , 95  , 95  , 0   , 116 , 175 } ,
     {"HotPink3"           , 132 , 5  , 175 , 95  , 135 , 235 , 116 , 175 } ,
     {"MediumOrchid3"      , 133 , 5  , 175 , 95  , 175 , 213 , 116 , 175 } ,
     {"MediumOrchid"       , 134 , 13 , 175 , 95  , 215 , 199 , 142 , 215 } ,
     {"MediumPurple2"      , 135 , 12 , 175 , 95  , 255 , 192 , 160 , 255 } ,
     {"DarkGoldenrod"      , 136 , 3  , 175 , 135 , 0   , 33  , 255 , 175 } ,
     {"LightSalmon3"       , 137 , 7  , 175 , 135 , 95  , 21  , 116 , 175 } ,
     {"RosyBrown"          , 138 , 7  , 175 , 135 , 135 , 0   , 58  , 175 } ,
     {"Grey63"             , 139 , 5  , 175 , 135 , 175 , 213 , 58  , 175 } ,
     {"MediumPurple2Bis"   , 140 , 12 , 175 , 135 , 215 , 192 , 94  , 215 } ,
     {"MediumPurple1"      , 141 , 12 , 175 , 135 , 255 , 185 , 120 , 255 } ,
     {"Gold3"              , 142 , 3  , 175 , 175 , 0   , 43  , 255 , 175 } ,
     {"DarkKhaki"          , 143 , 7  , 175 , 175 , 95  , 43  , 116 , 175 } ,
     {"NavajoWhite3"       , 144 , 7  , 175 , 175 , 135 , 43  , 58  , 175 } ,
     {"Grey69"             , 145 , 7  , 175 , 175 , 175 , 0   , 0   , 175 } ,
     {"LightSteelBlue3"    , 146 , 12 , 175 , 175 , 215 , 171 , 47  , 215 } ,
     {"LightSteelBlue"     , 147 , 12 , 175 , 175 , 255 , 171 , 80  , 255 } ,
     {"Yellow3"            , 148 , 11 , 175 , 215 , 0   , 50  , 255 , 215 } ,
     {"DarkOliveGreen3Ter" , 149 , 11 , 175 , 215 , 95  , 57  , 142 , 215 } ,
     {"DarkSeaGreen3Bis"   , 150 , 7  , 175 , 215 , 135 , 64  , 94  , 215 } ,
     {"DarkSeaGreen2"      , 151 , 7  , 175 , 215 , 175 , 85  , 47  , 215 } ,
     {"LightCyan3"         , 152 , 7  , 175 , 215 , 215 , 128 , 47  , 215 } ,
     {"LightSkyBlue1"      , 153 , 12 , 175 , 215 , 255 , 150 , 80  , 255 } ,
     {"GreenYellow"        , 154 , 11 , 175 , 255 , 0   , 56  , 255 , 255 } ,
     {"DarkOliveGreen2"    , 155 , 10 , 175 , 255 , 95  , 64  , 160 , 255 } ,
     {"PaleGreen1Bis"      , 156 , 10 , 175 , 255 , 135 , 71  , 120 , 255 } ,
     {"DarkSeaGreen2Bis"   , 157 , 15 , 175 , 255 , 175 , 85  , 80  , 255 } ,
     {"DarkSeaGreen1"      , 158 , 15 , 175 , 255 , 215 , 106 , 80  , 255 } ,
     {"PaleTurquoise1"     , 159 , 14 , 175 , 255 , 255 , 128 , 80  , 255 } ,
     {"Red3Bis"            , 160 , 9  , 215 , 0   , 0   , 0   , 255 , 215 } ,
     {"DeepPink3"          , 161 , 13 , 215 , 0   , 95  , 237 , 255 , 215 } ,
     {"DeepPink3Bis"       , 162 , 13 , 215 , 0   , 135 , 229 , 255 , 215 } ,
     {"Magenta3Bis"        , 163 , 13 , 215 , 0   , 175 , 221 , 255 , 215 } ,
     {"Magenta3Ter"        , 164 , 13 , 215 , 0   , 215 , 213 , 255 , 215 } ,
     {"Magenta2"           , 165 , 13 , 215 , 0   , 255 , 207 , 255 , 255 } ,
     {"DarkOrange3Bis"     , 166 , 9  , 215 , 95  , 0   , 19  , 255 , 215 } ,
     {"IndianRedBis"       , 167 , 9  , 215 , 95  , 95  , 0   , 142 , 215 } ,
     {"HotPink3Bis"        , 168 , 13 , 215 , 95  , 135 , 242 , 142 , 215 } ,
     {"HotPink2"           , 169 , 13 , 215 , 95  , 175 , 228 , 142 , 215 } ,
     {"Orchid"             , 170 , 13 , 215 , 95  , 215 , 213 , 142 , 215 } ,
     {"MediumOrchid1"      , 171 , 13 , 215 , 95  , 255 , 203 , 160 , 255 } ,
     {"Orange3"            , 172 , 11 , 215 , 135 , 0   , 27  , 255 , 215 } ,
     {"LightSalmon3"       , 173 , 9  , 215 , 135 , 95  , 14  , 142 , 215 } ,
     {"LightPink3"         , 174 , 7  , 215 , 135 , 135 , 0   , 94  , 215 } ,
     {"Pink3"              , 175 , 13 , 215 , 135 , 175 , 235 , 94  , 215 } ,
     {"Plum3"              , 176 , 13 , 215 , 135 , 215 , 213 , 94  , 215 } ,
     {"Violet"             , 177 , 13 , 215 , 135 , 255 , 199 , 120 , 255 } ,
     {"Gold3Bis"           , 178 , 11 , 215 , 175 , 0   , 35  , 255 , 215 } ,
     {"LightGoldenrod3"    , 179 , 11 , 215 , 175 , 95  , 28  , 142 , 215 } ,
     {"Tan"                , 180 , 7  , 215 , 175 , 135 , 21  , 94  , 215 } ,
     {"MistyRose3"         , 181 , 7  , 215 , 175 , 175 , 0   , 47  , 215 } ,
     {"Thistle3"           , 182 , 13 , 215 , 175 , 215 , 213 , 47  , 215 } ,
     {"Plum2"              , 183 , 12 , 215 , 175 , 255 , 192 , 80  , 255 } ,
     {"Yellow3Bis"         , 184 , 11 , 215 , 215 , 0   , 43  , 255 , 215 } ,
     {"Khaki3"             , 185 , 11 , 215 , 215 , 95  , 43  , 142 , 215 } ,
     {"LightGoldenrod2"    , 186 , 7  , 215 , 215 , 135 , 43  , 94  , 215 } ,
     {"LightYellow3"       , 187 , 7  , 215 , 215 , 175 , 43  , 47  , 215 } ,
     {"Grey84"             , 188 , 7  , 215 , 215 , 215 , 0   , 0   , 215 } ,
     {"LightSteelBlue1"    , 189 , 12 , 215 , 215 , 255 , 171 , 40  , 255 } ,
     {"Yellow2"            , 190 , 11 , 215 , 255 , 0   , 49  , 255 , 255 } ,
     {"DarkOliveGreen1"    , 191 , 11 , 215 , 255 , 95  , 53  , 160 , 255 } ,
     {"DarkOliveGreen1Bis" , 192 , 11 , 215 , 255 , 135 , 57  , 120 , 255 } ,
     {"DarkSeaGreen1Bis"   , 193 , 15 , 215 , 255 , 175 , 64  , 80  , 255 } ,
     {"Honeydew2"          , 194 , 15 , 215 , 255 , 215 , 85  , 40  , 255 } ,
     {"LightCyan1Bis"      , 195 , 15 , 215 , 255 , 255 , 128 , 40  , 255 } ,
     {"Red1"               , 196 , 9  , 255 , 0   , 0   , 0   , 255 , 255 } ,
     {"DeepPink2"          , 197 , 13 , 255 , 0   , 95  , 240 , 255 , 255 } ,
     {"DeepPink1"          , 198 , 13 , 255 , 0   , 135 , 234 , 255 , 255 } ,
     {"DeepPink1Bis"       , 199 , 13 , 255 , 0   , 175 , 227 , 255 , 255 } ,
     {"Magenta2Bis"        , 200 , 13 , 255 , 0   , 215 , 220 , 255 , 255 } ,
     {"Magenta1"           , 201 , 13 , 255 , 0   , 255 , 213 , 255 , 255 } ,
     {"OrangeRed1"         , 202 , 9  , 255 , 95  , 0   , 16  , 255 , 255 } ,
     {"IndianRed1"         , 203 , 9  , 255 , 95  , 95  , 0   , 160 , 255 } ,
     {"IndianRed1Bis"      , 204 , 13 , 255 , 95  , 135 , 246 , 160 , 255 } ,
     {"HotPink"            , 205 , 13 , 255 , 95  , 175 , 235 , 160 , 255 } ,
     {"HotPinkBis"         , 206 , 13 , 255 , 95  , 215 , 224 , 160 , 255 } ,
     {"MediumOrchid1Bis"   , 207 , 13 , 255 , 95  , 255 , 213 , 160 , 255 } ,
     {"DarkOrange"         , 208 , 11 , 255 , 135 , 0   , 22  , 255 , 255 } ,
     {"Salmon1"            , 209 , 9  , 255 , 135 , 95  , 10  , 160 , 255 } ,
     {"LightCoral"         , 210 , 15 , 255 , 135 , 135 , 0   , 120 , 255 } ,
     {"PaleVioletRed1"     , 211 , 13 , 255 , 135 , 175 , 242 , 120 , 255 } ,
     {"Orchid2"            , 212 , 13 , 255 , 135 , 215 , 228 , 120 , 255 } ,
     {"Orchid1"            , 213 , 13 , 255 , 135 , 255 , 213 , 120 , 255 } ,
     {"Orange1"            , 214 , 11 , 255 , 175 , 0   , 29  , 255 , 255 } ,
     {"SandyBrown"         , 215 , 9  , 255 , 175 , 95  , 21  , 160 , 255 } ,
     {"LightSalmon1"       , 216 , 15 , 255 , 175 , 135 , 14  , 120 , 255 } ,
     {"LightPink1"         , 217 , 15 , 255 , 175 , 175 , 0   , 80  , 255 } ,
     {"Pink1"              , 218 , 13 , 255 , 175 , 215 , 235 , 80  , 255 } ,
     {"Plum1"              , 219 , 13 , 255 , 175 , 255 , 213 , 80  , 255 } ,
     {"Gold1"              , 220 , 11 , 255 , 215 , 0   , 36  , 255 , 255 } ,
     {"LightGoldenrod2Bis" , 221 , 11 , 255 , 215 , 95  , 32  , 160 , 255 } ,
     {"LightGoldenrod2Ter" , 222 , 15 , 255 , 215 , 135 , 28  , 120 , 255 } ,
     {"NavajoWhite1"       , 223 , 15 , 255 , 215 , 175 , 21  , 80  , 255 } ,
     {"MistyRose1"         , 224 , 15 , 255 , 215 , 215 , 0   , 40  , 255 } ,
     {"Thistle1"           , 225 , 13 , 255 , 215 , 255 , 213 , 40  , 255 } ,
     {"Yellow1"            , 226 , 11 , 255 , 255 , 0   , 43  , 255 , 255 } ,
     {"LightGoldenrod1"    , 227 , 11 , 255 , 255 , 95  , 43  , 160 , 255 } ,
     {"Khaki1"             , 228 , 15 , 255 , 255 , 135 , 43  , 120 , 255 } ,
     {"Wheat1"             , 229 , 15 , 255 , 255 , 175 , 43  , 80  , 255 } ,
     {"Cornsilk1"          , 230 , 15 , 255 , 255 , 215 , 43  , 40  , 255 } ,
     {"Grey100"            , 231 , 15 , 255 , 255 , 255 , 0   , 0   , 255 } ,
     {"Grey3"              , 232 , 0  , 8   , 8   , 8   , 0   , 0   , 8   } ,
     {"Grey7"              , 233 , 0  , 18  , 18  , 18  , 0   , 0   , 18  } ,
     {"Grey11"             , 234 , 0  , 28  , 28  , 28  , 0   , 0   , 28  } ,
     {"Grey15"             , 235 , 0  , 38  , 38  , 38  , 0   , 0   , 38  } ,
     {"Grey19"             , 236 , 0  , 48  , 48  , 48  , 0   , 0   , 48  } ,
     {"Grey23"             , 237 , 0  , 58  , 58  , 58  , 0   , 0   , 58  } ,
     {"Grey27"             , 238 , 8  , 68  , 68  , 68  , 0   , 0   , 68  } ,
     {"Grey30"             , 239 , 8  , 78  , 78  , 78  , 0   , 0   , 78  } ,
     {"Grey35"             , 240 , 8  , 88  , 88  , 88  , 0   , 0   , 88  } ,
     {"Grey39"             , 241 , 8  , 98  , 98  , 98  , 0   , 0   , 98  } ,
     {"Grey42"             , 242 , 8  , 108 , 108 , 108 , 0   , 0   , 108 } ,
     {"Grey46"             , 243 , 8  , 118 , 118 , 118 , 0   , 0   , 118 } ,
     {"Grey50"             , 244 , 8  , 128 , 128 , 128 , 0   , 0   , 128 } ,
     {"Grey54"             , 245 , 8  , 138 , 138 , 138 , 0   , 0   , 138 } ,
     {"Grey58"             , 246 , 8  , 148 , 148 , 148 , 0   , 0   , 148 } ,
     {"Grey62"             , 247 , 8  , 158 , 158 , 158 , 0   , 0   , 158 } ,
     {"Grey66"             , 248 , 7  , 168 , 168 , 168 , 0   , 0   , 168 } ,
     {"Grey70"             , 249 , 7  , 178 , 178 , 178 , 0   , 0   , 178 } ,
     {"Grey74"             , 250 , 7  , 188 , 188 , 188 , 0   , 0   , 188 } ,
     {"Grey78"             , 251 , 7  , 198 , 198 , 198 , 0   , 0   , 198 } ,
     {"Grey82"             , 252 , 7  , 208 , 208 , 208 , 0   , 0   , 208 } ,
     {"Grey85"             , 253 , 7  , 218 , 218 , 218 , 0   , 0   , 218 } ,
     {"Grey89"             , 254 , 15 , 228 , 228 , 228 , 0   , 0   , 228 } ,
     {"Grey93"             , 255 , 15 , 238 , 238 , 238 , 0   , 0   , 238 } ,
}};

ColorInfo GetColorInfo(Color::Palette256 index) {
  return palette256[index]; // NOLINT;
}

ColorInfo GetColorInfo(Color::Palette16 index) {
  return palette256[index]; // NOLINT;
}
// clang-format off

std::vector<std::vector<ColorInfo>> ColorInfoSorted2D() {
  // Acquire the color information for the palette256.
  std::vector<ColorInfo> info_gray;
  std::vector<ColorInfo> info_color;
  for (int i = 16; i < 256; ++i) {
    const ColorInfo info = GetColorInfo(Color::Palette256(i));
    if (info.saturation == 0) {
      info_gray.push_back(info);
    } else {
      info_color.push_back(info);
    }
  }

  // Sort info_color by hue.
  std::sort(
      info_color.begin(), info_color.end(),
      [](const ColorInfo& A, const ColorInfo& B) { return A.hue < B.hue; });

  // Make 8 columns, one gray and seven colored.
  std::vector<std::vector<ColorInfo>> info_columns(8);
  info_columns[0] = info_gray;
  for (size_t i = 0; i < info_color.size(); ++i) {
    info_columns[1 + 7 * i / info_color.size()].push_back(info_color[i]);
  }

  // Minimize discontinuities for every columns.
  for (auto& column : info_columns) {
    std::sort(column.begin(), column.end(),
              [](const ColorInfo& A, const ColorInfo& B) {
                return A.value < B.value;
              });
    for (int i = 0; i < int(column.size()) - 1; ++i) {
      int best_index = i + 1;
      int best_distance = 255 * 255 * 3;
      for (size_t j = i + 1; j < column.size(); ++j) {
        const int dx = (int)column[i].red - (int)column[j].red;
        const int dy = (int)column[i].green - (int)column[j].green;
        const int dz = (int)column[i].blue - (int)column[j].blue;
        const int distance = dx * dx + dy * dy + dz * dz;
        if (best_distance > distance) {
          best_distance = distance;
          best_index = (int)j;
        }
      }
      std::swap(column[i + 1], column[best_index]);
    }
  }

  return info_columns;
}

}

// ---- screen\screen.cpp -------------------------------------------


#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Macro for hinting that an expression is likely to be false.
#if !defined(FTXUI_UNLIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define FTXUI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FTXUI_UNLIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(FTXUI_UNLIKELY)

#if !defined(FTXUI_LIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define FTXUI_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define FTXUI_LIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(FTXUI_LIKELY)

namespace ftxui {

namespace {

#if defined(_WIN32)
void WindowsEmulateVT100Terminal() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  // Enable VT processing on stdout and stdin
  auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

  DWORD out_mode = 0;
  if (!GetConsoleMode(stdout_handle, &out_mode)) {
    // The output is not a console (e.g. redirected to a file or a pipe). Keep
    // the detected color support and let the consumer of the stream interpret
    // the escape sequences.
    return;
  }

  // https://docs.microsoft.com/en-us/windows/console/setconsolemode
  const int enable_virtual_terminal_processing = 0x0004;
  const int disable_newline_auto_return = 0x0008;
  out_mode |= enable_virtual_terminal_processing;
  out_mode |= disable_newline_auto_return;

  SetConsoleMode(stdout_handle, out_mode);
}
#endif

// Position of a color sequence inside the output string. It refers to the
// output instead of copying it, so tracking it costs no allocation.
struct ColorSequence {
  size_t begin = 0;
  size_t size = 0;
};

// Append to `ss` the escape sequences switching the style from `prev` to
// `next`.
// - `color_support`: the terminal color support, read once per frame.
// - `last_colors`: the last color sequence written to `ss`.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void UpdateCellStyle(const Screen* screen,
                     std::string& ss,
                     Terminal::Color color_support,
                     ColorSequence& last_colors,
                     const Cell& prev,
                     const Cell& next) {
  // See https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
  if (FTXUI_UNLIKELY(next.hyperlink != prev.hyperlink)) {
    ss += "\x1B]8;;";
    ss += screen->Hyperlink(next.hyperlink);
    ss += "\x1B\\";
  }

  // Bold
  if (FTXUI_UNLIKELY((next.bold ^ prev.bold) | (next.dim ^ prev.dim))) {
    // BOLD_AND_DIM_RESET:
    if ((prev.bold && !next.bold) || (prev.dim && !next.dim)) {
      ss += "\x1B[22m";
    }
    if (next.bold) {
      ss += "\x1B[1m";  // BOLD_SET
    }
    if (next.dim) {
      ss += "\x1B[2m";  // DIM_SET
    }
  }

  // Underline
  if (FTXUI_UNLIKELY(next.underlined != prev.underlined ||
                     next.underlined_double != prev.underlined_double)) {
    ss += (next.underlined          ? "\x1B[4m"     // UNDERLINE
           : next.underlined_double ? "\x1B[21m"    // UNDERLINE_DOUBLE
                                    : "\x1B[24m");  // UNDERLINE_RESET
  }

  // Blink
  if (FTXUI_UNLIKELY(next.blink != prev.blink)) {
    ss += (next.blink ? "\x1B[5m"     // BLINK_SET
                      : "\x1B[25m");  // BLINK_RESET
  }

  // Inverted
  if (FTXUI_UNLIKELY(next.inverted != prev.inverted)) {
    ss += (next.inverted ? "\x1B[7m"     // INVERTED_SET
                         : "\x1B[27m");  // INVERTED_RESET
  }

  // Italics
  if (FTXUI_UNLIKELY(next.italic != prev.italic)) {
    ss += (next.italic ? "\x1B[3m"     // ITALIC_SET
                       : "\x1B[23m");  // ITALIC_RESET
  }

  // StrikeThrough
  if (FTXUI_UNLIKELY(next.strikethrough != prev.strikethrough)) {
    ss += (next.strikethrough ? "\x1B[9m"     // CROSSED_OUT
                              : "\x1B[29m");  // CROSSED_OUT_RESET
  }

  if (FTXUI_UNLIKELY(next.foreground_color != prev.foreground_color ||
                     next.background_color != prev.background_color)) {
    const size_t begin = ss.size();
    ss += "\x1B[";
    next.foreground_color.PrintTo(ss, false, color_support);
    ss += 'm';
    ss += "\x1B[";
    next.background_color.PrintTo(ss, true, color_support);
    ss += 'm';

    // Colors are degraded to the terminal color support when printed, so
    // distinct colors can print the same sequence. e.g. RGB(1,2,3) and
    // RGB(2,3,4) both print "38;5;16" on a 256-color terminal. Drop the new
    // sequence when it repeats the last one. This can't happen with
    // TrueColor, so skip the comparison there.
    if (color_support != Terminal::Color::TrueColor) {
      const size_t size = ss.size() - begin;
      if (size == last_colors.size &&
          ss.compare(begin, size, ss, last_colors.begin, size) == 0) {
        ss.resize(begin);
      } else {
        last_colors.begin = begin;
        last_colors.size = size;
      }
    }
  }
}

struct TileEncoding {
  std::uint8_t left : 2;
  std::uint8_t top : 2;
  std::uint8_t right : 2;
  std::uint8_t down : 2;
  std::uint8_t round : 1;

  // clang-format off
  bool operator<(const TileEncoding& other) const {
    if (left < other.left)   { return  true;  }
    if (left > other.left)   { return  false; }
    if (top < other.top)     { return  true;  }
    if (top > other.top)     { return  false; }
    if (right < other.right) { return  true;  }
    if (right > other.right) { return  false; }
    if (down < other.down)   { return  true;  }
    if (down > other.down)   { return  false; }
    if (round < other.round) { return  true;  }
    if (round > other.round) { return  false; }
      return false;
  }
  // clang-format on
};

// clang-format off
const std::map<std::string, TileEncoding> tile_encoding = { // NOLINT
    {"─", {1, 0, 1, 0, 0}},
    {"━", {2, 0, 2, 0, 0}},
    {"╍", {2, 0, 2, 0, 0}},

    {"│", {0, 1, 0, 1, 0}},
    {"┃", {0, 2, 0, 2, 0}},
    {"╏", {0, 2, 0, 2, 0}},

    {"┌", {0, 0, 1, 1, 0}},
    {"┍", {0, 0, 2, 1, 0}},
    {"┎", {0, 0, 1, 2, 0}},
    {"┏", {0, 0, 2, 2, 0}},

    {"┐", {1, 0, 0, 1, 0}},
    {"┑", {2, 0, 0, 1, 0}},
    {"┒", {1, 0, 0, 2, 0}},
    {"┓", {2, 0, 0, 2, 0}},

    {"└", {0, 1, 1, 0, 0}},
    {"┕", {0, 1, 2, 0, 0}},
    {"┖", {0, 2, 1, 0, 0}},
    {"┗", {0, 2, 2, 0, 0}},

    {"┘", {1, 1, 0, 0, 0}},
    {"┙", {2, 1, 0, 0, 0}},
    {"┚", {1, 2, 0, 0, 0}},
    {"┛", {2, 2, 0, 0, 0}},

    {"├", {0, 1, 1, 1, 0}},
    {"┝", {0, 1, 2, 1, 0}},
    {"┞", {0, 2, 1, 1, 0}},
    {"┟", {0, 1, 1, 2, 0}},
    {"┠", {0, 2, 1, 2, 0}},
    {"┡", {0, 2, 2, 1, 0}},
    {"┢", {0, 1, 2, 2, 0}},
    {"┣", {0, 2, 2, 2, 0}},

    {"┤", {1, 1, 0, 1, 0}},
    {"┥", {2, 1, 0, 1, 0}},
    {"┦", {1, 2, 0, 1, 0}},
    {"┧", {1, 1, 0, 2, 0}},
    {"┨", {1, 2, 0, 2, 0}},
    {"┩", {2, 2, 0, 1, 0}},
    {"┪", {2, 1, 0, 2, 0}},
    {"┫", {2, 2, 0, 2, 0}},

    {"┬", {1, 0, 1, 1, 0}},
    {"┭", {2, 0, 1, 1, 0}},
    {"┮", {1, 0, 2, 1, 0}},
    {"┯", {2, 0, 2, 1, 0}},
    {"┰", {1, 0, 1, 2, 0}},
    {"┱", {2, 0, 1, 2, 0}},
    {"┲", {1, 0, 2, 2, 0}},
    {"┳", {2, 0, 2, 2, 0}},

    {"┴", {1, 1, 1, 0, 0}},
    {"┵", {2, 1, 1, 0, 0}},
    {"┶", {1, 1, 2, 0, 0}},
    {"┷", {2, 1, 2, 0, 0}},
    {"┸", {1, 2, 1, 0, 0}},
    {"┹", {2, 2, 1, 0, 0}},
    {"┺", {1, 2, 2, 0, 0}},
    {"┻", {2, 2, 2, 0, 0}},

    {"┼", {1, 1, 1, 1, 0}},
    {"┽", {2, 1, 1, 1, 0}},
    {"┾", {1, 1, 2, 1, 0}},
    {"┿", {2, 1, 2, 1, 0}},
    {"╀", {1, 2, 1, 1, 0}},
    {"╁", {1, 1, 1, 2, 0}},
    {"╂", {1, 2, 1, 2, 0}},
    {"╃", {2, 2, 1, 1, 0}},
    {"╄", {1, 2, 2, 1, 0}},
    {"╅", {2, 1, 1, 2, 0}},
    {"╆", {1, 1, 2, 2, 0}},
    {"╇", {2, 2, 2, 1, 0}},
    {"╈", {2, 1, 2, 2, 0}},
    {"╉", {2, 2, 1, 2, 0}},
    {"╊", {1, 2, 2, 2, 0}},
    {"╋", {2, 2, 2, 2, 0}},

    {"═", {3, 0, 3, 0, 0}},
    {"║", {0, 3, 0, 3, 0}},

    {"╒", {0, 0, 3, 1, 0}},
    {"╓", {0, 0, 1, 3, 0}},
    {"╔", {0, 0, 3, 3, 0}},

    {"╕", {3, 0, 0, 1, 0}},
    {"╖", {1, 0, 0, 3, 0}},
    {"╗", {3, 0, 0, 3, 0}},

    {"╘", {0, 1, 3, 0, 0}},
    {"╙", {0, 3, 1, 0, 0}},
    {"╚", {0, 3, 3, 0, 0}},

    {"╛", {3, 1, 0, 0, 0}},
    {"╜", {1, 3, 0, 0, 0}},
    {"╝", {3, 3, 0, 0, 0}},

    {"╞", {0, 1, 3, 1, 0}},
    {"╟", {0, 3, 1, 3, 0}},
    {"╠", {0, 3, 3, 3, 0}},

    {"╡", {3, 1, 0, 1, 0}},
    {"╢", {1, 3, 0, 3, 0}},
    {"╣", {3, 3, 0, 3, 0}},

    {"╤", {3, 0, 3, 1, 0}},
    {"╥", {1, 0, 1, 3, 0}},
    {"╦", {3, 0, 3, 3, 0}},

    {"╧", {3, 1, 3, 0, 0}},
    {"╨", {1, 3, 1, 0, 0}},
    {"╩", {3, 3, 3, 0, 0}},

    {"╪", {3, 1, 3, 1, 0}},
    {"╫", {1, 3, 1, 3, 0}},
    {"╬", {3, 3, 3, 3, 0}},

    {"╭", {0, 0, 1, 1, 1}},
    {"╮", {1, 0, 0, 1, 1}},
    {"╯", {1, 1, 0, 0, 1}},
    {"╰", {0, 1, 1, 0, 1}},

    {"╴", {1, 0, 0, 0, 0}},
    {"╵", {0, 1, 0, 0, 0}},
    {"╶", {0, 0, 1, 0, 0}},
    {"╷", {0, 0, 0, 1, 0}},

    {"╸", {2, 0, 0, 0, 0}},
    {"╹", {0, 2, 0, 0, 0}},
    {"╺", {0, 0, 2, 0, 0}},
    {"╻", {0, 0, 0, 2, 0}},

    {"╼", {1, 0, 2, 0, 0}},
    {"╽", {0, 1, 0, 2, 0}},
    {"╾", {2, 0, 1, 0, 0}},
    {"╿", {0, 2, 0, 1, 0}},
};
// clang-format on

template <class A, class B>
std::map<B, A> InvertMap(const std::map<A, B>& input) {
  std::map<B, A> output;
  for (const auto& it : input) {
    output[it.second] = it.first;
  }
  return output;
}

const std::map<TileEncoding, std::string> tile_encoding_inverse =  // NOLINT
    InvertMap(tile_encoding);

void UpgradeLeftRight(std::string& left, std::string& right) {
  const auto it_left = tile_encoding.find(left);
  if (it_left == tile_encoding.end()) {
    return;
  }
  const auto it_right = tile_encoding.find(right);
  if (it_right == tile_encoding.end()) {
    return;
  }

  if (it_left->second.right == 0 && it_right->second.left != 0) {
    TileEncoding encoding_left = it_left->second;
    encoding_left.right = it_right->second.left;
    const auto it_left_upgrade = tile_encoding_inverse.find(encoding_left);
    if (it_left_upgrade != tile_encoding_inverse.end()) {
      left = it_left_upgrade->second;
    }
  }

  if (it_right->second.left == 0 && it_left->second.right != 0) {
    TileEncoding encoding_right = it_right->second;
    encoding_right.left = it_left->second.right;
    const auto it_right_upgrade = tile_encoding_inverse.find(encoding_right);
    if (it_right_upgrade != tile_encoding_inverse.end()) {
      right = it_right_upgrade->second;
    }
  }
}

void UpgradeTopDown(std::string& top, std::string& down) {
  const auto it_top = tile_encoding.find(top);
  if (it_top == tile_encoding.end()) {
    return;
  }
  const auto it_down = tile_encoding.find(down);
  if (it_down == tile_encoding.end()) {
    return;
  }

  if (it_top->second.down == 0 && it_down->second.top != 0) {
    TileEncoding encoding_top = it_top->second;
    encoding_top.down = it_down->second.top;
    const auto it_top_down = tile_encoding_inverse.find(encoding_top);
    if (it_top_down != tile_encoding_inverse.end()) {
      top = it_top_down->second;
    }
  }

  if (it_down->second.top == 0 && it_top->second.down != 0) {
    TileEncoding encoding_down = it_down->second;
    encoding_down.top = it_top->second.down;
    const auto it_down_top = tile_encoding_inverse.find(encoding_down);
    if (it_down_top != tile_encoding_inverse.end()) {
      down = it_down_top->second;
    }
  }
}

bool ShouldAttemptAutoMerge(Cell& cell) {
  return cell.automerge && cell.character.size() == 3;
}

// Whether the two cells are printed identically.
bool SameCell(const Screen& screen_a,
              const Cell& a,
              const Screen& screen_b,
              const Cell& b) {
  return a.character == b.character &&                //
         a.foreground_color == b.foreground_color &&  //
         a.background_color == b.background_color &&  //
         a.blink == b.blink &&                        //
         a.bold == b.bold &&                          //
         a.dim == b.dim &&                            //
         a.italic == b.italic &&                      //
         a.inverted == b.inverted &&                  //
         a.underlined == b.underlined &&              //
         a.underlined_double == b.underlined_double &&
         a.strikethrough == b.strikethrough &&
         screen_a.Hyperlink(a.hyperlink) == screen_b.Hyperlink(b.hyperlink);
}

// Mark the cells hidden behind the fullwidth character preceding them. They
// aren't printed. This matches Screen::ToString().
void HiddenCells(const Cell* row, int dimx, std::vector<bool>& hidden) {
  hidden.assign(static_cast<size_t>(dimx), false);
  bool previous_fullwidth = false;
  for (int x = 0; x < dimx; ++x) {
    hidden[x] = previous_fullwidth;
    const std::string& character = row[x].character;
    previous_fullwidth =
        !hidden[x] && character.size() > 1 && string_width(character) == 2;
  }
}

}  // namespace

/// A fixed dimension.
/// @see Fit
/// @see Full
Dimensions Dimension::Fixed(int v) {
  return {v, v};
}

/// Use the terminal dimensions.
/// @see Fixed
/// @see Fit
Dimensions Dimension::Full() {
  return Terminal::Size();
}

// static
/// Create a screen with the given dimension along the x-axis and y-axis.
Screen Screen::Create(Dimensions width, Dimensions height) {
  return {width.dimx, height.dimy};
}

// static
/// Create a screen with the given dimension.
Screen Screen::Create(Dimensions dimension) {
  return {dimension.dimx, dimension.dimy};
}

Screen::Screen(int dimx, int dimy) : Surface{dimx, dimy} {
#if defined(_WIN32)
  // The placement of this call is a bit weird, however we can assume that
  // anybody who instantiates a Screen object eventually wants to output
  // something to the console. If that is not the case, use an instance of
  // Surface instead. As we require UTF8 for all input/output operations we will
  // just switch to UTF8 encoding here
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  WindowsEmulateVT100Terminal();
#endif
}

/// Produce a std::string that can be used to print the Screen on the
/// terminal.
/// @note Don't forget to flush stdout. Alternatively, you can use
/// Screen::Print();
std::string Screen::ToString() const {
  // Pre-allocate: ~30 bytes per cell for character + escape codes.
  std::string ss;
  ss.reserve(static_cast<size_t>(dimx_) * static_cast<size_t>(dimy_) * 30);
  ToString(ss);
  return ss;
}

/// Produce a std::string that can be used to print the Screen on the
/// terminal.
/// @param ss The string to append to.
void Screen::ToString(std::string& ss) const {
  const Cell default_cell;
  const Cell* previous_cell_ref = &default_cell;
  const Terminal::Color color_support = Terminal::ColorSupport();
  ColorSequence last_colors;

  for (int y = 0; y < dimy_; ++y) {
    // New line in between two lines.
    if (y != 0) {
      UpdateCellStyle(this, ss, color_support, last_colors, *previous_cell_ref,
                      default_cell);
      previous_cell_ref = &default_cell;
      ss += "\r\n";
    }

    // After printing a fullwith character, we need to skip the next cell.
    bool previous_fullwidth = false;
    if (dimx_ > 0) {
      const Cell* line_start = &FastCellAt(0, y);
      const Cell* line_end = line_start + dimx_;
      for (const Cell* it = line_start; it != line_end; ++it) {
        const auto& cell = *it;
        if (!previous_fullwidth) {
          UpdateCellStyle(this, ss, color_support, last_colors,
                          *previous_cell_ref, cell);
          previous_cell_ref = &cell;
          if (cell.character.empty()) {
            ss += ' ';
          } else {
            ss += cell.character;
          }
        }
        // A hidden cell can't hide the next one.
        previous_fullwidth = !previous_fullwidth && cell.character.size() > 1 &&
                             string_width(cell.character) == 2;
      }
    }
  }

  // Reset the style to default:
  UpdateCellStyle(this, ss, color_support, last_colors, *previous_cell_ref,
                  default_cell);
}

/// Produce a std::string updating the terminal from |previous| to this Screen.
/// Only the cells that changed are printed.
/// @param ss The string to append to.
/// @param previous The Screen currently displayed. The cursor must be at its
///                 top-left corner. It ends where ToString() would leave it.
void Screen::ToString(std::string& ss, const Screen& previous) const {
  if (previous.dimx_ != dimx_ || previous.dimy_ != dimy_ || dimx_ == 0 ||
      dimy_ == 0) {
    ToString(ss);
    return;
  }

  const Cell default_cell;
  const Terminal::Color color_support = Terminal::ColorSupport();
  ColorSequence last_colors;
  std::vector<bool> hidden;
  std::vector<bool> previous_hidden;
  int cursor_y = 0;

  for (int y = 0; y < dimy_; ++y) {
    const Cell* row = &FastCellAt(0, y);
    const Cell* previous_row = &previous.FastCellAt(0, y);
    auto same = [&](int x) {
      return SameCell(*this, row[x], previous, previous_row[x]);
    };

    int first = 0;
    while (first < dimx_ && same(first)) {
      ++first;
    }
    int last = dimx_ - 1;
    if (y != dimy_ - 1) {
      if (first == dimx_) {
        continue;
      }
      while (same(last)) {
        --last;
      }
    } else if (first == dimx_) {
      // Always print the last cell, so the cursor ends like ToString().
      first = dimx_ - 1;
    }

    // Don't split fullwidth characters, from either frame. Overwriting half of
    // one erases the other half, which must be printed again.
    HiddenCells(row, dimx_, hidden);
    HiddenCells(previous_row, dimx_, previous_hidden);
    while (first > 0 && (hidden[first] || previous_hidden[first])) {
      --first;
    }
    while (last + 1 < dimx_ &&
           (hidden[last + 1] || previous_hidden[last + 1])) {
      ++last;
    }

    // Move the cursor to (first, y).
    if (y != cursor_y) {
      ss += "\x1B[" + std::to_string(y - cursor_y) + "B";  // MOVE_DOWN
      cursor_y = y;
    }
    ss += '\r';  // MOVE_LEFT
    if (first != 0) {
      ss += "\x1B[" + std::to_string(first) + "C";  // MOVE_RIGHT
    }

    const Cell* previous_cell_ref = &default_cell;
    for (int x = first; x <= last; ++x) {
      if (hidden[x]) {
        continue;
      }
      const Cell& cell = row[x];
      UpdateCellStyle(this, ss, color_support, last_colors, *previous_cell_ref,
                      cell);
      previous_cell_ref = &cell;
      if (cell.character.empty()) {
        ss += ' ';
      } else {
        ss += cell.character;
      }
    }
    UpdateCellStyle(this, ss, color_support, last_colors, *previous_cell_ref,
                    default_cell);
  }
}

// Print the Screen to the terminal.
void Screen::Print() const {
  std::cout << ToString() << '\0' << std::flush;
}

/// @brief Return a string to be printed in order to reset the cursor position
///        to the beginning of the screen.
///
/// ```cpp
/// std::string reset_position;
/// while(true) {
///   auto document = render();
///   auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
///   Render(screen, document);
///   std::cout << reset_position << screen.ToString() << std::flush;
///   reset_position = screen.ResetPosition();
///
///   using namespace std::chrono_literals;
///   std::this_thread::sleep_for(0.01s);
/// }
/// ```
///
/// @return The string to print in order to reset the cursor position to the
///         beginning.
std::string Screen::ResetPosition(bool clear) const {
  std::string ss;
  ss.reserve(static_cast<size_t>(dimy_) * 12);
  ResetPosition(ss, clear);
  return ss;
}

/// @brief Append to a string in order to reset the cursor position to the
///        beginning of the screen.
/// @param ss The string to append to.
/// @param clear Whether to clear the screen or not.
void Screen::ResetPosition(std::string& ss, bool clear) const {
  if (clear) {
    // The clear branch must move up one row at a time, because each row needs
    // its own CLEAR_LINE (\x1B[2K) erase. It cannot be collapsed into a single
    // parameterized cursor-up.
    ss += '\r';       // MOVE_LEFT;
    ss += "\x1b[2K";  // CLEAR_SCREEN;
    for (int y = 1; y < dimy_; ++y) {
      ss += "\x1B[1A";  // MOVE_UP;
      ss += "\x1B[2K";  // CLEAR_LINE;
    }
  } else {
    // The non-clear branch only needs to reposition the cursor at the top-left,
    // so the per-row walk-up is collapsed into a single parameterized
    // CSI cursor-up (\x1B[<n>A), emitting far fewer bytes per frame.
    ss += '\r';  // MOVE_LEFT;
    if (dimy_ > 1) {
      ss += "\x1B[" + std::to_string(dimy_ - 1) + "A";  // MOVE_UP;
    }
  }
}

/// @brief Clear all the cells from the screen.
void Screen::Clear() {
  Surface::Clear();

  cursor_.x = dimx_ - 1;
  cursor_.y = dimy_ - 1;

  hyperlinks_ = {
      "",
  };
}

// clang-format off
void Screen::ApplyShader() {
  // Merge box characters together.
  for (int y = 0; y < dimy_; ++y) {
    for (int x = 0; x < dimx_; ++x) {
      // Box drawing character uses exactly 3 byte.
      Cell& cur = FastCellAt(x, y);
      if (!ShouldAttemptAutoMerge(cur)) {
        continue;
      }

      if (x > 0) {
        Cell& left = FastCellAt(x - 1, y);
        if (ShouldAttemptAutoMerge(left)) {
          UpgradeLeftRight(left.character, cur.character);
        }
      }
      if (y > 0) {
        Cell& top = FastCellAt(x, y - 1);
        if (ShouldAttemptAutoMerge(top)) {
          UpgradeTopDown(top.character, cur.character);
        }
      }
    }
  }
}
// clang-format on

std::uint8_t Screen::RegisterHyperlink(std::string_view link) {
  for (std::size_t i = 0; i < hyperlinks_.size(); ++i) {
    if (hyperlinks_[i] == link) {
      return i;
    }
  }
  if (hyperlinks_.size() == std::numeric_limits<std::uint8_t>::max()) {
    return 0;
  }
  hyperlinks_.emplace_back(link);
  return hyperlinks_.size() - 1;
}

const std::string& Screen::Hyperlink(std::uint8_t id) const {
  if (id >= hyperlinks_.size()) {
    return hyperlinks_[0];
  }
  return hyperlinks_[id];
}

/// @brief Return the current selection style.
/// @see SetSelectionStyle
const Screen::SelectionStyle& Screen::GetSelectionStyle() const {
  return selection_style_;
}

/// @brief Set the current selection style.
/// @see GetSelectionStyle
void Screen::SetSelectionStyle(SelectionStyle decorator) {
  selection_style_ = std::move(decorator);
}

void Screen::Reserved1() {}
void Screen::Reserved2() {}
void Screen::Reserved3() {}
void Screen::Reserved4() {}
void Screen::Reserved5() {}
void Screen::Reserved6() {}
void Screen::Reserved7() {}
void Screen::Reserved8() {}

}  // namespace ftxui

// ---- screen\string.cpp -------------------------------------------




namespace {

struct Interval {
  uint32_t first;
  uint32_t last;
};

using WBP = ftxui::WordBreakProperty;
struct WordBreakPropertyInterval {
  uint32_t first;
  uint32_t last;
  WBP property;
};

// g_full_width_characters and g_word_break_intervals, generated from the
// Unicode Character Database by tools/gen_unicode_tables.py.

// clang-format off

constexpr std::array<Interval, 126> g_full_width_characters = {{
    {0x01100, 0x0115f}, {0x0231a, 0x0231b}, {0x02329, 0x0232a},
    {0x023e9, 0x023ec}, {0x023f0, 0x023f0}, {0x023f3, 0x023f3},
    {0x025fd, 0x025fe}, {0x02614, 0x02615}, {0x02630, 0x02637},
    {0x02648, 0x02653}, {0x0267f, 0x0267f}, {0x0268a, 0x0268f},
    {0x02693, 0x02693}, {0x026a1, 0x026a1}, {0x026aa, 0x026ab},
    {0x026bd, 0x026be}, {0x026c4, 0x026c5}, {0x026ce, 0x026ce},
    {0x026d4, 0x026d4}, {0x026ea, 0x026ea}, {0x026f2, 0x026f3},
    {0x026f5, 0x026f5}, {0x026fa, 0x026fa}, {0x026fd, 0x026fd},
    {0x02705, 0x02705}, {0x0270a, 0x0270b}, {0x02728, 0x02728},
    {0x0274c, 0x0274c}, {0x0274e, 0x0274e}, {0x02753, 0x02755},
    {0x02757, 0x02757}, {0x02795, 0x02797}, {0x027b0, 0x027b0},
    {0x027bf, 0x027bf}, {0x02b1b, 0x02b1c}, {0x02b50, 0x02b50},
    {0x02b55, 0x02b55}, {0x02e80, 0x02e99}, {0x02e9b, 0x02ef3},
    {0x02f00, 0x02fd5}, {0x02ff0, 0x0303e}, {0x03041, 0x03096},
    {0x03099, 0x030ff}, {0x03105, 0x0312f}, {0x03131, 0x0318e},
    {0x03190, 0x031e5}, {0x031ef, 0x0321e}, {0x03220, 0x03247},
    {0x03250, 0x0a48c}, {0x0a490, 0x0a4c6}, {0x0a960, 0x0a97c},
    {0x0ac00, 0x0d7a3}, {0x0f900, 0x0faff}, {0x0fe10, 0x0fe19},
    {0x0fe30, 0x0fe52}, {0x0fe54, 0x0fe66}, {0x0fe68, 0x0fe6b},
    {0x0ff01, 0x0ff60}, {0x0ffe0, 0x0ffe6}, {0x16fe0, 0x16fe4},
    {0x16ff0, 0x16ff6}, {0x17000, 0x18cda}, {0x18cff, 0x18d20},
    {0x18d80, 0x18df2}, {0x18e00, 0x19191}, {0x191a0, 0x191d2},
    {0x1aff0, 0x1aff3}, {0x1aff5, 0x1affb}, {0x1affd, 0x1affe},
    {0x1b000, 0x1b128}, {0x1b132, 0x1b132}, {0x1b150, 0x1b152},
    {0x1b155, 0x1b155}, {0x1b164, 0x1b168}, {0x1b170, 0x1b2fb},
    {0x1d300, 0x1d356}, {0x1d360, 0x1d376}, {0x1f004, 0x1f004},
    {0x1f0cf, 0x1f0cf}, {0x1f18e, 0x1f18e}, {0x1f191, 0x1f19a},
    {0x1f1ae, 0x1f1ae}, {0x1f200, 0x1f202}, {0x1f210, 0x1f23b},
    {0x1f240, 0x1f248}, {0x1f250, 0x1f251}, {0x1f260, 0x1f265},
    {0x1f300, 0x1f320}, {0x1f32d, 0x1f335}, {0x1f337, 0x1f37c},
    {0x1f37e, 0x1f393}, {0x1f3a0, 0x1f3ca}, {0x1f3cf, 0x1f3d3},
    {0x1f3e0, 0x1f3f0}, {0x1f3f4, 0x1f3f4}, {0x1f3f8, 0x1f43e},
    {0x1f440, 0x1f440}, {0x1f442, 0x1f4fc}, {0x1f4ff, 0x1f53d},
    {0x1f54b, 0x1f54e}, {0x1f550, 0x1f567}, {0x1f57a, 0x1f57a},
    {0x1f595, 0x1f596}, {0x1f5a4, 0x1f5a4}, {0x1f5fb, 0x1f64f},
    {0x1f680, 0x1f6c5}, {0x1f6cc, 0x1f6cc}, {0x1f6d0, 0x1f6d2},
    {0x1f6d5, 0x1f6d9}, {0x1f6dc, 0x1f6df}, {0x1f6eb, 0x1f6ec},
    {0x1f6f4, 0x1f6fc}, {0x1f7da, 0x1f7da}, {0x1f7e0, 0x1f7eb},
    {0x1f7f0, 0x1f7f0}, {0x1f90c, 0x1f93a}, {0x1f93c, 0x1f945},
    {0x1f947, 0x1f9ff}, {0x1fa70, 0x1fa7c}, {0x1fa80, 0x1fac6},
    {0x1fac8, 0x1fac8}, {0x1facc, 0x1fadd}, {0x1fadf, 0x1faeb},
    {0x1faef, 0x1fafa}, {0x20000, 0x2fffd}, {0x30000, 0x3fffd},
}};

constexpr std::array<WordBreakPropertyInterval, 1114>
    g_word_break_intervals = {{
        {0x0000A, 0x0000A, WBP::LF},
        {0x0000B, 0x0000C, WBP::Newline},
        {0x0000D, 0x0000D, WBP::CR},
        {0x00020, 0x00020, WBP::WSegSpace},
        {0x00022, 0x00022, WBP::Double_Quote},
        {0x00027, 0x00027, WBP::Single_Quote},
        {0x0002C, 0x0002C, WBP::MidNum},
        {0x0002E, 0x0002E, WBP::MidNumLet},
        {0x00030, 0x00039, WBP::Numeric},
        {0x0003A, 0x0003A, WBP::MidLetter},
        {0x0003B, 0x0003B, WBP::MidNum},
        {0x00041, 0x0005A, WBP::ALetter},
        {0x0005F, 0x0005F, WBP::ExtendNumLet},
        {0x00061, 0x0007A, WBP::ALetter},
        {0x00085, 0x00085, WBP::Newline},
        {0x000AA, 0x000AA, WBP::ALetter},
        {0x000AD, 0x000AD, WBP::Format},
        {0x000B5, 0x000B5, WBP::ALetter},
        {0x000B7, 0x000B7, WBP::MidLetter},
        {0x000B8, 0x000B8, WBP::ALetter},
        {0x000BA, 0x000BA, WBP::ALetter},
        {0x000C0, 0x000D6, WBP::ALetter},
        {0x000D8, 0x000F6, WBP::ALetter},
        {0x000F8, 0x002D7, WBP::ALetter},
        {0x002DE, 0x002FF, WBP::ALetter},
        {0x00300, 0x0036F, WBP::Extend},
        {0x00370, 0x00374, WBP::ALetter},
        {0x00376, 0x00377, WBP::ALetter},
        {0x0037A, 0x0037D, WBP::ALetter},
        {0x0037E, 0x0037E, WBP::MidNum},
        {0x0037F, 0x0037F, WBP::ALetter},
        {0x00386, 0x00386, WBP::ALetter},
        {0x00387, 0x00387, WBP::MidLetter},
        {0x00388, 0x0038A, WBP::ALetter},
        {0x0038C, 0x0038C, WBP::ALetter},
        {0x0038E, 0x003A1, WBP::ALetter},
        {0x003A3, 0x003F5, WBP::ALetter},
        {0x003F7, 0x00481, WBP::ALetter},
        {0x00483, 0x00489, WBP::Extend},
        {0x0048A, 0x0052F, WBP::ALetter},
        {0x00531, 0x00556, WBP::ALetter},
        {0x00558, 0x0055C, WBP::ALetter},
        {0x0055E, 0x0055E, WBP::ALetter},
        {0x0055F, 0x0055F, WBP::MidLetter},
        {0x00560, 0x00588, WBP::ALetter},
        {0x00589, 0x00589, WBP::MidNum},
        {0x0058A, 0x0058C, WBP::ALetter},
        {0x00591, 0x005BD, WBP::Extend},
        {0x005BF, 0x005BF, WBP::Extend},
        {0x005C1, 0x005C2, WBP::Extend},
        {0x005C4, 0x005C5, WBP::Extend},
        {0x005C7, 0x005C9, WBP::Extend},
        {0x005D0, 0x005EA, WBP::Hebrew_Letter},
        {0x005EF, 0x005F2, WBP::Hebrew_Letter},
        {0x005F3, 0x005F3, WBP::ALetter},
        {0x005F4, 0x005F4, WBP::MidLetter},
        {0x00600, 0x00605, WBP::Numeric},
        {0x0060C, 0x0060D, WBP::MidNum},
        {0x00610, 0x0061A, WBP::Extend},
        {0x0061C, 0x0061C, WBP::Format},
        {0x00620, 0x0064A, WBP::ALetter},
        {0x0064B, 0x0065F, WBP::Extend},
        {0x00660, 0x00669, WBP::Numeric},
        {0x0066B, 0x0066B, WBP::Numeric},
        {0x0066C, 0x0066C, WBP::MidNum},
        {0x0066E, 0x0066F, WBP::ALetter},
        {0x00670, 0x00670, WBP::Extend},
        {0x00671, 0x006D3, WBP::ALetter},
        {0x006D5, 0x006D5, WBP::ALetter},
        {0x006D6, 0x006DC, WBP::Extend},
        {0x006DD, 0x006DD, WBP::Numeric},
        {0x006DF, 0x006E4, WBP::Extend},
        {0x006E5, 0x006E6, WBP::ALetter},
        {0x006E7, 0x006E8, WBP::Extend},
        {0x006EA, 0x006ED, WBP::Extend},
        {0x006EE, 0x006EF, WBP::ALetter},
        {0x006F0, 0x006F9, WBP::Numeric},
        {0x006FA, 0x006FC, WBP::ALetter},
        {0x006FF, 0x006FF, WBP::ALetter},
        {0x0070F, 0x00710, WBP::ALetter},
        {0x00711, 0x00711, WBP::Extend},
        {0x00712, 0x0072F, WBP::ALetter},
        {0x00730, 0x0074A, WBP::Extend},
        {0x0074D, 0x007A5, WBP::ALetter},
        {0x007A6, 0x007B0, WBP::Extend},
        {0x007B1, 0x007B1, WBP::ALetter},
        {0x007C0, 0x007C9, WBP::Numeric},
        {0x007CA, 0x007EA, WBP::ALetter},
        {0x007EB, 0x007F3, WBP::Extend},
        {0x007F4, 0x007F5, WBP::ALetter},
        {0x007F8, 0x007F8, WBP::MidNum},
        {0x007FA, 0x007FA, WBP::ALetter},
        {0x007FD, 0x007FD, WBP::Extend},
        {0x00800, 0x00815, WBP::ALetter},
        {0x00816, 0x00819, WBP::Extend},
        {0x0081A, 0x0081A, WBP::ALetter},
        {0x0081B, 0x00823, WBP::Extend},
        {0x00824, 0x00824, WBP::ALetter},
        {0x00825, 0x00827, WBP::Extend},
        {0x00828, 0x00828, WBP::ALetter},
        {0x00829, 0x0082D, WBP::Extend},
        {0x00840, 0x00858, WBP::ALetter},
        {0x00859, 0x0085B, WBP::Extend},
        {0x00860, 0x0086A, WBP::ALetter},
        {0x00870, 0x00887, WBP::ALetter},
        {0x00889, 0x0088F, WBP::ALetter},
        {0x00890, 0x00891, WBP::Numeric},
        {0x00897, 0x0089F, WBP::Extend},
        {0x008A0, 0x008C9, WBP::ALetter},
        {0x008CA, 0x008E1, WBP::Extend},
        {0x008E2, 0x008E2, WBP::Numeric},
        {0x008E3, 0x00903, WBP::Extend},
        {0x00904, 0x00939, WBP::ALetter},
        {0x0093A, 0x0093C, WBP::Extend},
        {0x0093D, 0x0093D, WBP::ALetter},
        {0x0093E, 0x0094F, WBP::Extend},
        {0x00950, 0x00950, WBP::ALetter},
        {0x00951, 0x00957, WBP::Extend},
        {0x00958, 0x00961, WBP::ALetter},
        {0x00962, 0x00963, WBP::Extend},
        {0x00966, 0x0096F, WBP::Numeric},
        {0x00971, 0x00980, WBP::ALetter},
        {0x00981, 0x00983, WBP::Extend},
        {0x00985, 0x0098C, WBP::ALetter},
        {0x0098F, 0x00990, WBP::ALetter},
        {0x00993, 0x009A8, WBP::ALetter},
        {0x009AA, 0x009B0, WBP::ALetter},
        {0x009B2, 0x009B2, WBP::ALetter},
        {0x009B6, 0x009B9, WBP::ALetter},
        {0x009BC, 0x009BC, WBP::Extend},
        {0x009BD, 0x009BD, WBP::ALetter},
        {0x009BE, 0x009C4, WBP::Extend},
        {0x009C7, 0x009C8, WBP::Extend},
        {0x009CB, 0x009CD, WBP::Extend},
        {0x009CE, 0x009CE, WBP::ALetter},
        {0x009D7, 0x009D7, WBP::Extend},
        {0x009DC, 0x009DD, WBP::ALetter},
        {0x009DF, 0x009E1, WBP::ALetter},
        {0x009E2, 0x009E3, WBP::Extend},
        {0x009E6, 0x009EF, WBP::Numeric},
        {0x009F0, 0x009F1, WBP::ALetter},
        {0x009FC, 0x009FC, WBP::ALetter},
        {0x009FE, 0x009FE, WBP::Extend},
        {0x00A01, 0x00A03, WBP::Extend},
        {0x00A05, 0x00A0A, WBP::ALetter},
        {0x00A0F, 0x00A10, WBP::ALetter},
        {0x00A13, 0x00A28, WBP::ALetter},
        {0x00A2A, 0x00A30, WBP::ALetter},
        {0x00A32, 0x00A33, WBP::ALetter},
        {0x00A35, 0x00A36, WBP::ALetter},
        {0x00A38, 0x00A39, WBP::ALetter},
        {0x00A3C, 0x00A3C, WBP::Extend},
        {0x00A3E, 0x00A42, WBP::Extend},
        {0x00A47, 0x00A48, WBP::Extend},
        {0x00A4B, 0x00A4D, WBP::Extend},
        {0x00A51, 0x00A51, WBP::Extend},
        {0x00A59, 0x00A5C, WBP::ALetter},
        {0x00A5E, 0x00A5E, WBP::ALetter},
        {0x00A66, 0x00A6F, WBP::Numeric},
        {0x00A70, 0x00A71, WBP::Extend},
        {0x00A72, 0x00A74, WBP::ALetter},
        {0x00A75, 0x00A75, WBP::Extend},
        {0x00A81, 0x00A83, WBP::Extend},
        {0x00A85, 0x00A8D, WBP::ALetter},
        {0x00A8F, 0x00A91, WBP::ALetter},
        {0x00A93, 0x00AA8, WBP::ALetter},
        {0x00AAA, 0x00AB0, WBP::ALetter},
        {0x00AB2, 0x00AB3, WBP::ALetter},
        {0x00AB5, 0x00AB9, WBP::ALetter},
        {0x00ABC, 0x00ABC, WBP::Extend},
        {0x00ABD, 0x00ABD, WBP::ALetter},
        {0x00ABE, 0x00AC5, WBP::Extend},
        {0x00AC7, 0x00AC9, WBP::Extend},
        {0x00ACB, 0x00ACD, WBP::Extend},
        {0x00AD0, 0x00AD0, WBP::ALetter},
        {0x00AE0, 0x00AE1, WBP::ALetter},
        {0x00AE2, 0x00AE3, WBP::Extend},
        {0x00AE6, 0x00AEF, WBP::Numeric},
        {0x00AF9, 0x00AF9, WBP::ALetter},
        {0x00AFA, 0x00AFF, WBP::Extend},
        {0x00B01, 0x00B03, WBP::Extend},
        {0x00B05, 0x00B0C, WBP::ALetter},
        {0x00B0F, 0x00B10, WBP::ALetter},
        {0x00B13, 0x00B28, WBP::ALetter},
        {0x00B2A, 0x00B30, WBP::ALetter},
        {0x00B32, 0x00B33, WBP::ALetter},
        {0x00B35, 0x00B39, WBP::ALetter},
        {0x00B3C, 0x00B3C, WBP::Extend},
        {0x00B3D, 0x00B3D, WBP::ALetter},
        {0x00B3E, 0x00B44, WBP::Extend},
        {0x00B47, 0x00B48, WBP::Extend},
        {0x00B4B, 0x00B4D, WBP::Extend},
        {0x00B53, 0x00B57, WBP::Extend},
        {0x00B5C, 0x00B5D, WBP::ALetter},
        {0x00B5F, 0x00B61, WBP::ALetter},
        {0x00B62, 0x00B63, WBP::Extend},
        {0x00B66, 0x00B6F, WBP::Numeric},
        {0x00B71, 0x00B71, WBP::ALetter},
        {0x00B82, 0x00B82, WBP::Extend},
        {0x00B83, 0x00B83, WBP::ALetter},
        {0x00B85, 0x00B8A, WBP::ALetter},
        {0x00B8E, 0x00B90, WBP::ALetter},
        {0x00B92, 0x00B95, WBP::ALetter},
        {0x00B99, 0x00B9A, WBP::ALetter},
        {0x00B9C, 0x00B9C, WBP::ALetter},
        {0x00B9E, 0x00B9F, WBP::ALetter},
        {0x00BA3, 0x00BA4, WBP::ALetter},
        {0x00BA8, 0x00BAA, WBP::ALetter},
        {0x00BAE, 0x00BB9, WBP::ALetter},
        {0x00BBE, 0x00BC2, WBP::Extend},
        {0x00BC6, 0x00BC8, WBP::Extend},
        {0x00BCA, 0x00BCD, WBP::Extend},
        {0x00BD0, 0x00BD0, WBP::ALetter},
        {0x00BD7, 0x00BD7, WBP::Extend},
        {0x00BE6, 0x00BEF, WBP::Numeric},
        {0x00C00, 0x00C04, WBP::Extend},
        {0x00C05, 0x00C0C, WBP::ALetter},
        {0x00C0E, 0x00C10, WBP::ALetter},
        {0x00C12, 0x00C28, WBP::ALetter},
        {0x00C2A, 0x00C39, WBP::ALetter},
        {0x00C3C, 0x00C3C, WBP::Extend},
        {0x00C3D, 0x00C3D, WBP::ALetter},
        {0x00C3E, 0x00C44, WBP::Extend},
        {0x00C46, 0x00C48, WBP::Extend},
        {0x00C4A, 0x00C4D, WBP::Extend},
        {0x00C55, 0x00C56, WBP::Extend},
        {0x00C58, 0x00C5A, WBP::ALetter},
        {0x00C5C, 0x00C5D, WBP::ALetter},
        {0x00C60, 0x00C61, WBP::ALetter},
        {0x00C62, 0x00C63, WBP::Extend},
        {0x00C66, 0x00C6F, WBP::Numeric},
        {0x00C80, 0x00C80, WBP::ALetter},
        {0x00C81, 0x00C83, WBP::Extend},
        {0x00C85, 0x00C8C, WBP::ALetter},
        {0x00C8E, 0x00C90, WBP::ALetter},
        {0x00C92, 0x00CA8, WBP::ALetter},
        {0x00CAA, 0x00CB3, WBP::ALetter},
        {0x00CB5, 0x00CB9, WBP::ALetter},
        {0x00CBC, 0x00CBC, WBP::Extend},
        {0x00CBD, 0x00CBD, WBP::ALetter},
        {0x00CBE, 0x00CC4, WBP::Extend},
        {0x00CC6, 0x00CC8, WBP::Extend},
        {0x00CCA, 0x00CCD, WBP::Extend},
        {0x00CD5, 0x00CD6, WBP::Extend},
        {0x00CDC, 0x00CDE, WBP::ALetter},
        {0x00CE0, 0x00CE1, WBP::ALetter},
        {0x00CE2, 0x00CE3, WBP::Extend},
        {0x00CE6, 0x00CEF, WBP::Numeric},
        {0x00CF1, 0x00CF2, WBP::ALetter},
        {0x00CF3, 0x00CF3, WBP::Extend},
        {0x00D00, 0x00D03, WBP::Extend},
        {0x00D04, 0x00D0C, WBP::ALetter},
        {0x00D0E, 0x00D10, WBP::ALetter},
        {0x00D12, 0x00D3A, WBP::ALetter},
        {0x00D3B, 0x00D3C, WBP::Extend},
        {0x00D3D, 0x00D3D, WBP::ALetter},
        {0x00D3E, 0x00D44, WBP::Extend},
        {0x00D46, 0x00D48, WBP::Extend},
        {0x00D4A, 0x00D4D, WBP::Extend},
        {0x00D4E, 0x00D4E, WBP::ALetter},
        {0x00D54, 0x00D56, WBP::ALetter},
        {0x00D57, 0x00D57, WBP::Extend},
        {0x00D5F, 0x00D61, WBP::ALetter},
        {0x00D62, 0x00D63, WBP::Extend},
        {0x00D66, 0x00D6F, WBP::Numeric},
        {0x00D7A, 0x00D7F, WBP::ALetter},
        {0x00D81, 0x00D83, WBP::Extend},
        {0x00D85, 0x00D96, WBP::ALetter},
        {0x00D9A, 0x00DB1, WBP::ALetter},
        {0x00DB3, 0x00DBB, WBP::ALetter},
        {0x00DBD, 0x00DBD, WBP::ALetter},
        {0x00DC0, 0x00DC6, WBP::ALetter},
        {0x00DCA, 0x00DCA, WBP::Extend},
        {0x00DCF, 0x00DD4, WBP::Extend},
        {0x00DD6, 0x00DD6, WBP::Extend},
        {0x00DD8, 0x00DDF, WBP::Extend},
        {0x00DE6, 0x00DEF, WBP::Numeric},
        {0x00DF2, 0x00DF3, WBP::Extend},
        {0x00E31, 0x00E31, WBP::Extend},
        {0x00E34, 0x00E3A, WBP::Extend},
        {0x00E47, 0x00E4E, WBP::Extend},
        {0x00E50, 0x00E59, WBP::Numeric},
        {0x00EB1, 0x00EB1, WBP::Extend},
        {0x00EB4, 0x00EBC, WBP::Extend},
        {0x00EC8, 0x00ECE, WBP::Extend},
        {0x00ED0, 0x00ED9, WBP::Numeric},
        {0x00F00, 0x00F00, WBP::ALetter},
        {0x00F18, 0x00F19, WBP::Extend},
        {0x00F20, 0x00F29, WBP::Numeric},
        {0x00F35, 0x00F35, WBP::Extend},
        {0x00F37, 0x00F37, WBP::Extend},
        {0x00F39, 0x00F39, WBP::Extend},
        {0x00F3E, 0x00F3F, WBP::Extend},
        {0x00F40, 0x00F47, WBP::ALetter},
        {0x00F49, 0x00F6C, WBP::ALetter},
        {0x00F71, 0x00F84, WBP::Extend},
        {0x00F86, 0x00F87, WBP::Extend},
        {0x00F88, 0x00F8C, WBP::ALetter},
        {0x00F8D, 0x00F97, WBP::Extend},
        {0x00F99, 0x00FBC, WBP::Extend},
        {0x00FC6, 0x00FC6, WBP::Extend},
        {0x0102B, 0x0103E, WBP::Extend},
        {0x01040, 0x01049, WBP::Numeric},
        {0x01056, 0x01059, WBP::Extend},
        {0x0105E, 0x01060, WBP::Extend},
        {0x01062, 0x01064, WBP::Extend},
        {0x01067, 0x0106D, WBP::Extend},
        {0x01071, 0x01074, WBP::Extend},
        {0x01082, 0x0108D, WBP::Extend},
        {0x0108F, 0x0108F, WBP::Extend},
        {0x01090, 0x01099, WBP::Numeric},
        {0x0109A, 0x0109D, WBP::Extend},
        {0x010A0, 0x010C5, WBP::ALetter},
        {0x010C7, 0x010C7, WBP::ALetter},
        {0x010CD, 0x010CD, WBP::ALetter},
        {0x010D0, 0x010FA, WBP::ALetter},
        {0x010FC, 0x01248, WBP::ALetter},
        {0x0124A, 0x0124D, WBP::ALetter},
        {0x01250, 0x01256, WBP::ALetter},
        {0x01258, 0x01258, WBP::ALetter},
        {0x0125A, 0x0125D, WBP::ALetter},
        {0x01260, 0x01288, WBP::ALetter},
        {0x0128A, 0x0128D, WBP::ALetter},
        {0x01290, 0x012B0, WBP::ALetter},
        {0x012B2, 0x012B5, WBP::ALetter},
        {0x012B8, 0x012BE, WBP::ALetter},
        {0x012C0, 0x012C0, WBP::ALetter},
        {0x012C2, 0x012C5, WBP::ALetter},
        {0x012C8, 0x012D6, WBP::ALetter},
        {0x012D8, 0x01310, WBP::ALetter},
        {0x01312, 0x01315, WBP::ALetter},
        {0x01318, 0x0135A, WBP::ALetter},
        {0x0135D, 0x0135F, WBP::Extend},
        {0x01380, 0x0138F, WBP::ALetter},
        {0x013A0, 0x013F5, WBP::ALetter},
        {0x013F8, 0x013FD, WBP::ALetter},
        {0x01401, 0x0166C, WBP::ALetter},
        {0x0166F, 0x0167F, WBP::ALetter},
        {0x01680, 0x01680, WBP::WSegSpace},
        {0x01681, 0x0169A, WBP::ALetter},
        {0x016A0, 0x016EA, WBP::ALetter},
        {0x016EE, 0x016F8, WBP::ALetter},
        {0x01700, 0x01711, WBP::ALetter},
        {0x01712, 0x01715, WBP::Extend},
        {0x0171F, 0x01731, WBP::ALetter},
        {0x01732, 0x01734, WBP::Extend},
        {0x01740, 0x01751, WBP::ALetter},
        {0x01752, 0x01753, WBP::Extend},
        {0x01760, 0x0176C, WBP::ALetter},
        {0x0176E, 0x01770, WBP::ALetter},
        {0x01772, 0x01773, WBP::Extend},
        {0x017B4, 0x017D3, WBP::Extend},
        {0x017DD, 0x017DD, WBP::Extend},
        {0x017E0, 0x017E9, WBP::Numeric},
        {0x0180B, 0x0180D, WBP::Extend},
        {0x0180E, 0x0180E, WBP::Format},
        {0x0180F, 0x0180F, WBP::Extend},
        {0x01810, 0x01819, WBP::Numeric},
        {0x01820, 0x01878, WBP::ALetter},
        {0x01880, 0x01884, WBP::ALetter},
        {0x01885, 0x01886, WBP::Extend},
        {0x01887, 0x018A8, WBP::ALetter},
        {0x018A9, 0x018A9, WBP::Extend},
        {0x018AA, 0x018AA, WBP::ALetter},
        {0x018B0, 0x018F5, WBP::ALetter},
        {0x01900, 0x0191E, WBP::ALetter},
        {0x01920, 0x0192B, WBP::Extend},
        {0x01930, 0x0193B, WBP::Extend},
        {0x01946, 0x0194F, WBP::Numeric},
        {0x019D0, 0x019DA, WBP::Numeric},
        {0x01A00, 0x01A16, WBP::ALetter},
        {0x01A17, 0x01A1B, WBP::Extend},
        {0x01A55, 0x01A5E, WBP::Extend},
        {0x01A60, 0x01A7C, WBP::Extend},
        {0x01A7F, 0x01A7F, WBP::Extend},
        {0x01A80, 0x01A89, WBP::Numeric},
        {0x01A90, 0x01A99, WBP::Numeric},
        {0x01AB0, 0x01AF0, WBP::Extend},
        {0x01B00, 0x01B04, WBP::Extend},
        {0x01B05, 0x01B33, WBP::ALetter},
        {0x01B34, 0x01B44, WBP::Extend},
        {0x01B45, 0x01B4C, WBP::ALetter},
        {0x01B50, 0x01B59, WBP::Numeric},
        {0x01B6B, 0x01B73, WBP::Extend},
        {0x01B80, 0x01B82, WBP::Extend},
        {0x01B83, 0x01BA0, WBP::ALetter},
        {0x01BA1, 0x01BAD, WBP::Extend},
        {0x01BAE, 0x01BAF, WBP::ALetter},
        {0x01BB0, 0x01BB9, WBP::Numeric},
        {0x01BBA, 0x01BE5, WBP::ALetter},
        {0x01BE6, 0x01BF3, WBP::Extend},
        {0x01C00, 0x01C23, WBP::ALetter},
        {0x01C24, 0x01C37, WBP::Extend},
        {0x01C40, 0x01C49, WBP::Numeric},
        {0x01C4D, 0x01C4F, WBP::ALetter},
        {0x01C50, 0x01C59, WBP::Numeric},
        {0x01C5A, 0x01C7D, WBP::ALetter},
        {0x01C80, 0x01C8A, WBP::ALetter},
        {0x01C90, 0x01CBA, WBP::ALetter},
        {0x01CBD, 0x01CBF, WBP::ALetter},
        {0x01CD0, 0x01CD2, WBP::Extend},
        {0x01CD4, 0x01CE8, WBP::Extend},
        {0x01CE9, 0x01CEC, WBP::ALetter},
        {0x01CED, 0x01CED, WBP::Extend},
        {0x01CEE, 0x01CF3, WBP::ALetter},
        {0x01CF4, 0x01CF4, WBP::Extend},
        {0x01CF5, 0x01CF6, WBP::ALetter},
        {0x01CF7, 0x01CF9, WBP::Extend},
        {0x01CFA, 0x01CFA, WBP::ALetter},
        {0x01D00, 0x01DBF, WBP::ALetter},
        {0x01DC0, 0x01DFF, WBP::Extend},
        {0x01E00, 0x01F15, WBP::ALetter},
        {0x01F18, 0x01F1D, WBP::ALetter},
        {0x01F20, 0x01F45, WBP::ALetter},
        {0x01F48, 0x01F4D, WBP::ALetter},
        {0x01F50, 0x01F57, WBP::ALetter},
        {0x01F59, 0x01F59, WBP::ALetter},
        {0x01F5B, 0x01F5B, WBP::ALetter},
        {0x01F5D, 0x01F5D, WBP::ALetter},
        {0x01F5F, 0x01F7D, WBP::ALetter},
        {0x01F80, 0x01FB4, WBP::ALetter},
        {0x01FB6, 0x01FBC, WBP::ALetter},
        {0x01FBE, 0x01FBE, WBP::ALetter},
        {0x01FC2, 0x01FC4, WBP::ALetter},
        {0x01FC6, 0x01FCC, WBP::ALetter},
        {0x01FD0, 0x01FD3, WBP::ALetter},
        {0x01FD6, 0x01FDB, WBP::ALetter},
        {0x01FE0, 0x01FEC, WBP::ALetter},
        {0x01FF2, 0x01FF4, WBP::ALetter},
        {0x01FF6, 0x01FFC, WBP::ALetter},
        {0x02000, 0x02006, WBP::WSegSpace},
        {0x02008, 0x0200A, WBP::WSegSpace},
        {0x0200C, 0x0200C, WBP::Extend},
        {0x0200D, 0x0200D, WBP::ZWJ},
        {0x0200E, 0x0200F, WBP::Format},
        {0x02018, 0x02019, WBP::MidNumLet},
        {0x02024, 0x02024, WBP::MidNumLet},
        {0x02027, 0x02027, WBP::MidLetter},
        {0x02028, 0x02029, WBP::Newline},
        {0x0202A, 0x0202E, WBP::Format},
        {0x0202F, 0x0202F, WBP::ExtendNumLet},
        {0x0203F, 0x02040, WBP::ExtendNumLet},
        {0x02044, 0x02044, WBP::MidNum},
        {0x02054, 0x02054, WBP::ExtendNumLet},
        {0x0205F, 0x0205F, WBP::WSegSpace},
        {0x02060, 0x02064, WBP::Format},
        {0x02066, 0x0206F, WBP::Format},
        {0x02071, 0x02071, WBP::ALetter},
        {0x0207F, 0x0207F, WBP::ALetter},
        {0x0208F, 0x0209F, WBP::ALetter},
        {0x020D0, 0x020F0, WBP::Extend},
        {0x02102, 0x02102, WBP::ALetter},
        {0x02107, 0x02107, WBP::ALetter},
        {0x0210A, 0x02113, WBP::ALetter},
        {0x02115, 0x02115, WBP::ALetter},
        {0x02119, 0x0211D, WBP::ALetter},
        {0x02124, 0x02124, WBP::ALetter},
        {0x02126, 0x02126, WBP::ALetter},
        {0x02128, 0x02128, WBP::ALetter},
        {0x0212A, 0x0212D, WBP::ALetter},
        {0x0212F, 0x02139, WBP::ALetter},
        {0x0213C, 0x0213F, WBP::ALetter},
        {0x02145, 0x02149, WBP::ALetter},
        {0x0214E, 0x0214E, WBP::ALetter},
        {0x02160, 0x02188, WBP::ALetter},
        {0x024B6, 0x024E9, WBP::ALetter},
        {0x02C00, 0x02CE4, WBP::ALetter},
        {0x02CEB, 0x02CEE, WBP::ALetter},
        {0x02CEF, 0x02CF1, WBP::Extend},
        {0x02CF2, 0x02CF3, WBP::ALetter},
        {0x02D00, 0x02D25, WBP::ALetter},
        {0x02D27, 0x02D27, WBP::ALetter},
        {0x02D2D, 0x02D2D, WBP::ALetter},
        {0x02D30, 0x02D67, WBP::ALetter},
        {0x02D6F, 0x02D6F, WBP::ALetter},
        {0x02D7F, 0x02D7F, WBP::Extend},
        {0x02D80, 0x02D96, WBP::ALetter},
        {0x02DA0, 0x02DA6, WBP::ALetter},
        {0x02DA8, 0x02DAE, WBP::ALetter},
        {0x02DB0, 0x02DB6, WBP::ALetter},
        {0x02DB8, 0x02DBE, WBP::ALetter},
        {0x02DC0, 0x02DC6, WBP::ALetter},
        {0x02DC8, 0x02DCE, WBP::ALetter},
        {0x02DD0, 0x02DD6, WBP::ALetter},
        {0x02DD8, 0x02DDE, WBP::ALetter},
        {0x02DE0, 0x02DFF, WBP::Extend},
        {0x02E2F, 0x02E2F, WBP::ALetter},
        {0x03000, 0x03000, WBP::WSegSpace},
        {0x03005, 0x03005, WBP::ALetter},
        {0x0302A, 0x0302F, WBP::Extend},
        {0x03031, 0x03035, WBP::Katakana},
        {0x0303B, 0x0303C, WBP::ALetter},
        {0x03099, 0x0309A, WBP::Extend},
        {0x0309B, 0x0309C, WBP::Katakana},
        {0x030A0, 0x030FA, WBP::Katakana},
        {0x030FC, 0x030FF, WBP::Katakana},
        {0x03105, 0x0312F, WBP::ALetter},
        {0x03131, 0x0318E, WBP::ALetter},
        {0x031A0, 0x031BF, WBP::ALetter},
        {0x031F0, 0x031FF, WBP::Katakana},
        {0x032D0, 0x032FE, WBP::Katakana},
        {0x03300, 0x03357, WBP::Katakana},
        {0x0A000, 0x0A48C, WBP::ALetter},
        {0x0A4D0, 0x0A4FD, WBP::ALetter},
        {0x0A500, 0x0A60C, WBP::ALetter},
        {0x0A610, 0x0A61F, WBP::ALetter},
        {0x0A620, 0x0A629, WBP::Numeric},
        {0x0A62A, 0x0A62B, WBP::ALetter},
        {0x0A640, 0x0A66E, WBP::ALetter},
        {0x0A66F, 0x0A672, WBP::Extend},
        {0x0A674, 0x0A67D, WBP::Extend},
        {0x0A67F, 0x0A69D, WBP::ALetter},
        {0x0A69E, 0x0A69F, WBP::Extend},
        {0x0A6A0, 0x0A6EF, WBP::ALetter},
        {0x0A6F0, 0x0A6F1, WBP::Extend},
        {0x0A708, 0x0A7DD, WBP::ALetter},
        {0x0A7E2, 0x0A7E2, WBP::ALetter},
        {0x0A7F1, 0x0A801, WBP::ALetter},
        {0x0A802, 0x0A802, WBP::Extend},
        {0x0A803, 0x0A805, WBP::ALetter},
        {0x0A806, 0x0A806, WBP::Extend},
        {0x0A807, 0x0A80A, WBP::ALetter},
        {0x0A80B, 0x0A80B, WBP::Extend},
        {0x0A80C, 0x0A822, WBP::ALetter},
        {0x0A823, 0x0A827, WBP::Extend},
        {0x0A82C, 0x0A82C, WBP::Extend},
        {0x0A840, 0x0A873, WBP::ALetter},
        {0x0A880, 0x0A881, WBP::Extend},
        {0x0A882, 0x0A8B3, WBP::ALetter},
        {0x0A8B4, 0x0A8C5, WBP::Extend},
        {0x0A8D0, 0x0A8D9, WBP::Numeric},
        {0x0A8E0, 0x0A8F1, WBP::Extend},
        {0x0A8F2, 0x0A8F7, WBP::ALetter},
        {0x0A8FB, 0x0A8FB, WBP::ALetter},
        {0x0A8FD, 0x0A8FE, WBP::ALetter},
        {0x0A8FF, 0x0A8FF, WBP::Extend},
        {0x0A900, 0x0A909, WBP::Numeric},
        {0x0A90A, 0x0A925, WBP::ALetter},
        {0x0A926, 0x0A92D, WBP::Extend},
        {0x0A930, 0x0A946, WBP::ALetter},
        {0x0A947, 0x0A953, WBP::Extend},
        {0x0A960, 0x0A97C, WBP::ALetter},
        {0x0A980, 0x0A983, WBP::Extend},
        {0x0A984, 0x0A9B2, WBP::ALetter},
        {0x0A9B3, 0x0A9C0, WBP::Extend},
        {0x0A9CF, 0x0A9CF, WBP::ALetter},
        {0x0A9D0, 0x0A9D9, WBP::Numeric},
        {0x0A9E5, 0x0A9E5, WBP::Extend},
        {0x0A9F0, 0x0A9F9, WBP::Numeric},
        {0x0AA00, 0x0AA28, WBP::ALetter},
        {0x0AA29, 0x0AA36, WBP::Extend},
        {0x0AA40, 0x0AA42, WBP::ALetter},
        {0x0AA43, 0x0AA43, WBP::Extend},
        {0x0AA44, 0x0AA4B, WBP::ALetter},
        {0x0AA4C, 0x0AA4D, WBP::Extend},
        {0x0AA50, 0x0AA59, WBP::Numeric},
        {0x0AA7B, 0x0AA7D, WBP::Extend},
        {0x0AAB0, 0x0AAB0, WBP::Extend},
        {0x0AAB2, 0x0AAB4, WBP::Extend},
        {0x0AAB7, 0x0AAB8, WBP::Extend},
        {0x0AABE, 0x0AABF, WBP::Extend},
        {0x0AAC1, 0x0AAC1, WBP::Extend},
        {0x0AAE0, 0x0AAEA, WBP::ALetter},
        {0x0AAEB, 0x0AAEF, WBP::Extend},
        {0x0AAF2, 0x0AAF4, WBP::ALetter},
        {0x0AAF5, 0x0AAF6, WBP::Extend},
        {0x0AB01, 0x0AB06, WBP::ALetter},
        {0x0AB09, 0x0AB0E, WBP::ALetter},
        {0x0AB11, 0x0AB16, WBP::ALetter},
        {0x0AB20, 0x0AB26, WBP::ALetter},
        {0x0AB28, 0x0AB2E, WBP::ALetter},
        {0x0AB30, 0x0AB69, WBP::ALetter},
        {0x0AB6C, 0x0AB6D, WBP::ALetter},
        {0x0AB70, 0x0ABE2, WBP::ALetter},
        {0x0ABE3, 0x0ABEA, WBP::Extend},
        {0x0ABEC, 0x0ABED, WBP::Extend},
        {0x0ABF0, 0x0ABF9, WBP::Numeric},
        {0x0AC00, 0x0D7A3, WBP::ALetter},
        {0x0D7B0, 0x0D7C6, WBP::ALetter},
        {0x0D7CB, 0x0D7FB, WBP::ALetter},
        {0x0FB00, 0x0FB06, WBP::ALetter},
        {0x0FB13, 0x0FB17, WBP::ALetter},
        {0x0FB1D, 0x0FB1D, WBP::Hebrew_Letter},
        {0x0FB1E, 0x0FB1E, WBP::Extend},
        {0x0FB1F, 0x0FB28, WBP::Hebrew_Letter},
        {0x0FB2A, 0x0FB36, WBP::Hebrew_Letter},
        {0x0FB38, 0x0FB3C, WBP::Hebrew_Letter},
        {0x0FB3E, 0x0FB3E, WBP::Hebrew_Letter},
        {0x0FB40, 0x0FB41, WBP::Hebrew_Letter},
        {0x0FB43, 0x0FB44, WBP::Hebrew_Letter},
        {0x0FB46, 0x0FB4F, WBP::Hebrew_Letter},
        {0x0FB50, 0x0FBB1, WBP::ALetter},
        {0x0FBD3, 0x0FD3D, WBP::ALetter},
        {0x0FD50, 0x0FD8F, WBP::ALetter},
        {0x0FD92, 0x0FDC7, WBP::ALetter},
        {0x0FDF0, 0x0FDFB, WBP::ALetter},
        {0x0FE00, 0x0FE0F, WBP::Extend},
        {0x0FE13, 0x0FE13, WBP::MidLetter},
        {0x0FE20, 0x0FE2F, WBP::Extend},
        {0x0FE33, 0x0FE34, WBP::ExtendNumLet},
        {0x0FE4D, 0x0FE4F, WBP::ExtendNumLet},
        {0x0FE50, 0x0FE50, WBP::MidNum},
        {0x0FE52, 0x0FE52, WBP::MidNumLet},
        {0x0FE54, 0x0FE54, WBP::MidNum},
        {0x0FE55, 0x0FE55, WBP::MidLetter},
        {0x0FE70, 0x0FE74, WBP::ALetter},
        {0x0FE76, 0x0FEFC, WBP::ALetter},
        {0x0FEFF, 0x0FEFF, WBP::Format},
        {0x0FF07, 0x0FF07, WBP::MidNumLet},
        {0x0FF0C, 0x0FF0C, WBP::MidNum},
        {0x0FF0E, 0x0FF0E, WBP::MidNumLet},
        {0x0FF10, 0x0FF19, WBP::Numeric},
        {0x0FF1A, 0x0FF1A, WBP::MidLetter},
        {0x0FF1B, 0x0FF1B, WBP::MidNum},
        {0x0FF21, 0x0FF3A, WBP::ALetter},
        {0x0FF3F, 0x0FF3F, WBP::ExtendNumLet},
        {0x0FF41, 0x0FF5A, WBP::ALetter},
        {0x0FF66, 0x0FF9D, WBP::Katakana},
        {0x0FF9E, 0x0FF9F, WBP::Extend},
        {0x0FFA0, 0x0FFBE, WBP::ALetter},
        {0x0FFC2, 0x0FFC7, WBP::ALetter},
        {0x0FFCA, 0x0FFCF, WBP::ALetter},
        {0x0FFD2, 0x0FFD7, WBP::ALetter},
        {0x0FFDA, 0x0FFDC, WBP::ALetter},
        {0x0FFF9, 0x0FFFB, WBP::Format},
        {0x10000, 0x1000B, WBP::ALetter},
        {0x1000D, 0x10026, WBP::ALetter},
        {0x10028, 0x1003A, WBP::ALetter},
        {0x1003C, 0x1003D, WBP::ALetter},
        {0x1003F, 0x1004D, WBP::ALetter},
        {0x10050, 0x1005D, WBP::ALetter},
        {0x10080, 0x100FA, WBP::ALetter},
        {0x10140, 0x10174, WBP::ALetter},
        {0x101FD, 0x101FD, WBP::Extend},
        {0x10280, 0x1029C, WBP::ALetter},
        {0x102A0, 0x102D0, WBP::ALetter},
        {0x102E0, 0x102E0, WBP::Extend},
        {0x10300, 0x1031F, WBP::ALetter},
        {0x1032D, 0x1034A, WBP::ALetter},
        {0x10350, 0x10375, WBP::ALetter},
        {0x10376, 0x1037A, WBP::Extend},
        {0x10380, 0x1039D, WBP::ALetter},
        {0x103A0, 0x103C3, WBP::ALetter},
        {0x103C8, 0x103CF, WBP::ALetter},
        {0x103D1, 0x103D5, WBP::ALetter},
        {0x10400, 0x1049D, WBP::ALetter},
        {0x104A0, 0x104A9, WBP::Numeric},
        {0x104B0, 0x104D3, WBP::ALetter},
        {0x104D8, 0x104FB, WBP::ALetter},
        {0x10500, 0x10527, WBP::ALetter},
        {0x10530, 0x10563, WBP::ALetter},
        {0x10570, 0x1057A, WBP::ALetter},
        {0x1057C, 0x1058A, WBP::ALetter},
        {0x1058C, 0x10592, WBP::ALetter},
        {0x10594, 0x10595, WBP::ALetter},
        {0x10597, 0x105A1, WBP::ALetter},
        {0x105A3, 0x105B1, WBP::ALetter},
        {0x105B3, 0x105B9, WBP::ALetter},
        {0x105BB, 0x105BC, WBP::ALetter},
        {0x105C0, 0x105F3, WBP::ALetter},
        {0x10600, 0x10736, WBP::ALetter},
        {0x10740, 0x10755, WBP::ALetter},
        {0x10760, 0x10767, WBP::ALetter},
        {0x10780, 0x10785, WBP::ALetter},
        {0x10787, 0x107B0, WBP::ALetter},
        {0x107B2, 0x107BF, WBP::ALetter},
        {0x10800, 0x10805, WBP::ALetter},
        {0x10808, 0x10808, WBP::ALetter},
        {0x1080A, 0x10835, WBP::ALetter},
        {0x10837, 0x10838, WBP::ALetter},
        {0x1083C, 0x1083C, WBP::ALetter},
        {0x1083F, 0x10855, WBP::ALetter},
        {0x10860, 0x10876, WBP::ALetter},
        {0x10880, 0x1089E, WBP::ALetter},
        {0x108E0, 0x108F2, WBP::ALetter},
        {0x108F4, 0x108F5, WBP::ALetter},
        {0x10900, 0x10915, WBP::ALetter},
        {0x10920, 0x10939, WBP::ALetter},
        {0x10940, 0x10959, WBP::ALetter},
        {0x10980, 0x109B7, WBP::ALetter},
        {0x109BE, 0x109BF, WBP::ALetter},
        {0x10A00, 0x10A00, WBP::ALetter},
        {0x10A01, 0x10A03, WBP::Extend},
        {0x10A05, 0x10A06, WBP::Extend},
        {0x10A0C, 0x10A0F, WBP::Extend},
        {0x10A10, 0x10A13, WBP::ALetter},
        {0x10A15, 0x10A17, WBP::ALetter},
        {0x10A19, 0x10A35, WBP::ALetter},
        {0x10A38, 0x10A3A, WBP::Extend},
        {0x10A3F, 0x10A3F, WBP::Extend},
        {0x10A60, 0x10A7C, WBP::ALetter},
        {0x10A80, 0x10A9C, WBP::ALetter},
        {0x10AC0, 0x10AC7, WBP::ALetter},
        {0x10AC9, 0x10AE4, WBP::ALetter},
        {0x10AE5, 0x10AE6, WBP::Extend},
        {0x10B00, 0x10B35, WBP::ALetter},
        {0x10B40, 0x10B55, WBP::ALetter},
        {0x10B60, 0x10B72, WBP::ALetter},
        {0x10B80, 0x10B91, WBP::ALetter},
        {0x10C00, 0x10C48, WBP::ALetter},
        {0x10C80, 0x10CB2, WBP::ALetter},
        {0x10CC0, 0x10CF2, WBP::ALetter},
        {0x10D00, 0x10D23, WBP::ALetter},
        {0x10D24, 0x10D27, WBP::Extend},
        {0x10D30, 0x10D39, WBP::Numeric},
        {0x10D40, 0x10D49, WBP::Numeric},
        {0x10D4A, 0x10D65, WBP::ALetter},
        {0x10D69, 0x10D6D, WBP::Extend},
        {0x10D6F, 0x10D85, WBP::ALetter},
        {0x10E80, 0x10EA9, WBP::ALetter},
        {0x10EAB, 0x10EAC, WBP::Extend},
        {0x10EB0, 0x10EB1, WBP::ALetter},
        {0x10EC2, 0x10EC7, WBP::ALetter},
        {0x10ECB, 0x10ECF, WBP::Extend},
        {0x10ED9, 0x10EEE, WBP::ALetter},
        {0x10EF0, 0x10EFF, WBP::Extend},
        {0x10F00, 0x10F1C, WBP::ALetter},
        {0x10F27, 0x10F27, WBP::ALetter},
        {0x10F30, 0x10F45, WBP::ALetter},
        {0x10F46, 0x10F50, WBP::Extend},
        {0x10F70, 0x10F81, WBP::ALetter},
        {0x10F82, 0x10F85, WBP::Extend},
        {0x10FB0, 0x10FC4, WBP::ALetter},
        {0x10FE0, 0x10FF6, WBP::ALetter},
        {0x11000, 0x11002, WBP::Extend},
        {0x11003, 0x11037, WBP::ALetter},
        {0x11038, 0x11046, WBP::Extend},
        {0x11066, 0x1106F, WBP::Numeric},
        {0x11070, 0x11070, WBP::Extend},
        {0x11071, 0x11072, WBP::ALetter},
        {0x11073, 0x11074, WBP::Extend},
        {0x11075, 0x11075, WBP::ALetter},
        {0x1107F, 0x11082, WBP::Extend},
        {0x11083, 0x110AF, WBP::ALetter},
        {0x110B0, 0x110BA, WBP::Extend},
        {0x110BD, 0x110BD, WBP::Numeric},
        {0x110C2, 0x110C2, WBP::Extend},
        {0x110CD, 0x110CD, WBP::Numeric},
        {0x110D0, 0x110E8, WBP::ALetter},
        {0x110F0, 0x110F9, WBP::Numeric},
        {0x11100, 0x11102, WBP::Extend},
        {0x11103, 0x11126, WBP::ALetter},
        {0x11127, 0x11134, WBP::Extend},
        {0x11136, 0x1113F, WBP::Numeric},
        {0x11144, 0x11144, WBP::ALetter},
        {0x11145, 0x11146, WBP::Extend},
        {0x11147, 0x11147, WBP::ALetter},
        {0x11150, 0x11172, WBP::ALetter},
        {0x11173, 0x11173, WBP::Extend},
        {0x11176, 0x11176, WBP::ALetter},
        {0x11180, 0x11182, WBP::Extend},
        {0x11183, 0x111B2, WBP::ALetter},
        {0x111B3, 0x111C0, WBP::Extend},
        {0x111C1, 0x111C4, WBP::ALetter},
        {0x111C9, 0x111CC, WBP::Extend},
        {0x111CE, 0x111CF, WBP::Extend},
        {0x111D0, 0x111D9, WBP::Numeric},
        {0x111DA, 0x111DA, WBP::ALetter},
        {0x111DC, 0x111DC, WBP::ALetter},
        {0x11200, 0x11211, WBP::ALetter},
        {0x11213, 0x1122B, WBP::ALetter},
        {0x1122C, 0x11237, WBP::Extend},
        {0x1123E, 0x1123E, WBP::Extend},
        {0x1123F, 0x11240, WBP::ALetter},
        {0x11241, 0x11241, WBP::Extend},
        {0x11280, 0x11286, WBP::ALetter},
        {0x11288, 0x11288, WBP::ALetter},
        {0x1128A, 0x1128D, WBP::ALetter},
        {0x1128F, 0x1129D, WBP::ALetter},
        {0x1129F, 0x112A8, WBP::ALetter},
        {0x112B0, 0x112DE, WBP::ALetter},
        {0x112DF, 0x112EA, WBP::Extend},
        {0x112F0, 0x112F9, WBP::Numeric},
        {0x11300, 0x11303, WBP::Extend},
        {0x11305, 0x1130C, WBP::ALetter},
        {0x1130F, 0x11310, WBP::ALetter},
        {0x11313, 0x11328, WBP::ALetter},
        {0x1132A, 0x11330, WBP::ALetter},
        {0x11332, 0x11333, WBP::ALetter},
        {0x11335, 0x11339, WBP::ALetter},
        {0x1133B, 0x1133C, WBP::Extend},
        {0x1133D, 0x1133D, WBP::ALetter},
        {0x1133E, 0x11344, WBP::Extend},
        {0x11347, 0x11348, WBP::Extend},
        {0x1134B, 0x1134D, WBP::Extend},
        {0x11350, 0x11350, WBP::ALetter},
        {0x11357, 0x11357, WBP::Extend},
        {0x1135D, 0x11361, WBP::ALetter},
        {0x11362, 0x11363, WBP::Extend},
        {0x11366, 0x1136C, WBP::Extend},
        {0x11370, 0x11374, WBP::Extend},
        {0x11380, 0x11389, WBP::ALetter},
        {0x1138B, 0x1138B, WBP::ALetter},
        {0x1138E, 0x1138E, WBP::ALetter},
        {0x11390, 0x113B5, WBP::ALetter},
        {0x113B7, 0x113B7, WBP::ALetter},
        {0x113B8, 0x113C0, WBP::Extend},
        {0x113C2, 0x113C2, WBP::Extend},
        {0x113C5, 0x113C5, WBP::Extend},
        {0x113C7, 0x113CA, WBP::Extend},
        {0x113CC, 0x113D0, WBP::Extend},
        {0x113D1, 0x113D1, WBP::ALetter},
        {0x113D2, 0x113D2, WBP::Extend},
        {0x113D3, 0x113D3, WBP::ALetter},
        {0x113E1, 0x113E2, WBP::Extend},
        {0x11400, 0x11434, WBP::ALetter},
        {0x11435, 0x11446, WBP::Extend},
        {0x11447, 0x1144A, WBP::ALetter},
        {0x11450, 0x11459, WBP::Numeric},
        {0x1145E, 0x1145E, WBP::Extend},
        {0x1145F, 0x11461, WBP::ALetter},
        {0x11480, 0x114AF, WBP::ALetter},
        {0x114B0, 0x114C3, WBP::Extend},
        {0x114C4, 0x114C5, WBP::ALetter},
        {0x114C7, 0x114C7, WBP::ALetter},
        {0x114D0, 0x114D9, WBP::Numeric},
        {0x11580, 0x115AE, WBP::ALetter},
        {0x115AF, 0x115B5, WBP::Extend},
        {0x115B8, 0x115C0, WBP::Extend},
        {0x115D8, 0x115DB, WBP::ALetter},
        {0x115DC, 0x115DD, WBP::Extend},
        {0x11600, 0x1162F, WBP::ALetter},
        {0x11630, 0x11640, WBP::Extend},
        {0x11644, 0x11644, WBP::ALetter},
        {0x11650, 0x11659, WBP::Numeric},
        {0x11680, 0x116AA, WBP::ALetter},
        {0x116AB, 0x116B7, WBP::Extend},
        {0x116B8, 0x116B8, WBP::ALetter},
        {0x116C0, 0x116C9, WBP::Numeric},
        {0x116D0, 0x116E3, WBP::Numeric},
        {0x1171D, 0x1172B, WBP::Extend},
        {0x11730, 0x11739, WBP::Numeric},
        {0x11800, 0x1182B, WBP::ALetter},
        {0x1182C, 0x1183A, WBP::Extend},
        {0x118A0, 0x118DF, WBP::ALetter},
        {0x118E0, 0x118E9, WBP::Numeric},
        {0x118FF, 0x11906, WBP::ALetter},
        {0x11909, 0x11909, WBP::ALetter},
        {0x1190C, 0x11913, WBP::ALetter},
        {0x11915, 0x11916, WBP::ALetter},
        {0x11918, 0x1192F, WBP::ALetter},
        {0x11930, 0x11935, WBP::Extend},
        {0x11937, 0x11938, WBP::Extend},
        {0x1193B, 0x1193E, WBP::Extend},
        {0x1193F, 0x1193F, WBP::ALetter},
        {0x11940, 0x11940, WBP::Extend},
        {0x11941, 0x11941, WBP::ALetter},
        {0x11942, 0x11943, WBP::Extend},
        {0x11950, 0x11959, WBP::Numeric},
        {0x119A0, 0x119A7, WBP::ALetter},
        {0x119AA, 0x119D0, WBP::ALetter},
        {0x119D1, 0x119D7, WBP::Extend},
        {0x119DA, 0x119E0, WBP::Extend},
        {0x119E1, 0x119E1, WBP::ALetter},
        {0x119E3, 0x119E3, WBP::ALetter},
        {0x119E4, 0x119E4, WBP::Extend},
        {0x11A00, 0x11A00, WBP::ALetter},
        {0x11A01, 0x11A0A, WBP::Extend},
        {0x11A0B, 0x11A32, WBP::ALetter},
        {0x11A33, 0x11A39, WBP::Extend},
        {0x11A3A, 0x11A3A, WBP::ALetter},
        {0x11A3B, 0x11A3E, WBP::Extend},
        {0x11A47, 0x11A47, WBP::Extend},
        {0x11A50, 0x11A50, WBP::ALetter},
        {0x11A51, 0x11A5B, WBP::Extend},
        {0x11A5C, 0x11A89, WBP::ALetter},
        {0x11A8A, 0x11A99, WBP::Extend},
        {0x11A9D, 0x11A9D, WBP::ALetter},
        {0x11AB0, 0x11AF8, WBP::ALetter},
        {0x11B0A, 0x11B0A, WBP::ALetter},
        {0x11B60, 0x11B67, WBP::Extend},
        {0x11BC0, 0x11BE0, WBP::ALetter},
        {0x11BF0, 0x11BF9, WBP::Numeric},
        {0x11C00, 0x11C08, WBP::ALetter},
        {0x11C0A, 0x11C2E, WBP::ALetter},
        {0x11C2F, 0x11C36, WBP::Extend},
        {0x11C38, 0x11C3F, WBP::Extend},
        {0x11C40, 0x11C40, WBP::ALetter},
        {0x11C50, 0x11C59, WBP::Numeric},
        {0x11C72, 0x11C8F, WBP::ALetter},
        {0x11C92, 0x11CA7, WBP::Extend},
        {0x11CA9, 0x11CB6, WBP::Extend},
        {0x11D00, 0x11D06, WBP::ALetter},
        {0x11D08, 0x11D09, WBP::ALetter},
        {0x11D0B, 0x11D30, WBP::ALetter},
        {0x11D31, 0x11D36, WBP::Extend},
        {0x11D3A, 0x11D3A, WBP::Extend},
        {0x11D3C, 0x11D3D, WBP::Extend},
        {0x11D3F, 0x11D45, WBP::Extend},
        {0x11D46, 0x11D46, WBP::ALetter},
        {0x11D47, 0x11D47, WBP::Extend},
        {0x11D50, 0x11D59, WBP::Numeric},
        {0x11D60, 0x11D65, WBP::ALetter},
        {0x11D67, 0x11D68, WBP::ALetter},
        {0x11D6A, 0x11D89, WBP::ALetter},
        {0x11D8A, 0x11D8E, WBP::Extend},
        {0x11D90, 0x11D91, WBP::Extend},
        {0x11D93, 0x11D97, WBP::Extend},
        {0x11D98, 0x11D98, WBP::ALetter},
        {0x11DA0, 0x11DA9, WBP::Numeric},
        {0x11DB0, 0x11DDB, WBP::ALetter},
        {0x11DE0, 0x11DE9, WBP::Numeric},
        {0x11DF0, 0x11DF0, WBP::Extend},
        {0x11DF1, 0x11DF1, WBP::ALetter},
        {0x11EE0, 0x11EF2, WBP::ALetter},
        {0x11EF3, 0x11EF6, WBP::Extend},
        {0x11F00, 0x11F01, WBP::Extend},
        {0x11F02, 0x11F02, WBP::ALetter},
        {0x11F03, 0x11F03, WBP::Extend},
        {0x11F04, 0x11F10, WBP::ALetter},
        {0x11F12, 0x11F33, WBP::ALetter},
        {0x11F34, 0x11F3A, WBP::Extend},
        {0x11F3E, 0x11F42, WBP::Extend},
        {0x11F50, 0x11F59, WBP::Numeric},
        {0x11F5A, 0x11F5A, WBP::Extend},
        {0x11FB0, 0x11FB0, WBP::ALetter},
        {0x12000, 0x12399, WBP::ALetter},
        {0x12400, 0x1246F, WBP::ALetter},
        {0x12475, 0x12543, WBP::ALetter},
        {0x12550, 0x12686, WBP::ALetter},
        {0x12F90, 0x12FF0, WBP::ALetter},
        {0x13000, 0x1342F, WBP::ALetter},
        {0x13430, 0x1343F, WBP::Format},
        {0x13440, 0x13440, WBP::Extend},
        {0x13441, 0x13446, WBP::ALetter},
        {0x13447, 0x13455, WBP::Extend},
        {0x13460, 0x143FA, WBP::ALetter},
        {0x14400, 0x14646, WBP::ALetter},
        {0x16100, 0x1611D, WBP::ALetter},
        {0x1611E, 0x1612F, WBP::Extend},
        {0x16130, 0x16139, WBP::Numeric},
        {0x16800, 0x16A38, WBP::ALetter},
        {0x16A40, 0x16A5E, WBP::ALetter},
        {0x16A60, 0x16A69, WBP::Numeric},
        {0x16A70, 0x16ABE, WBP::ALetter},
        {0x16AC0, 0x16AC9, WBP::Numeric},
        {0x16AD0, 0x16AED, WBP::ALetter},
        {0x16AF0, 0x16AF4, WBP::Extend},
        {0x16B00, 0x16B2F, WBP::ALetter},
        {0x16B30, 0x16B36, WBP::Extend},
        {0x16B40, 0x16B43, WBP::ALetter},
        {0x16B50, 0x16B59, WBP::Numeric},
        {0x16B63, 0x16B77, WBP::ALetter},
        {0x16B7D, 0x16B8F, WBP::ALetter},
        {0x16D40, 0x16D6C, WBP::ALetter},
        {0x16D70, 0x16D79, WBP::Numeric},
        {0x16E40, 0x16E7F, WBP::ALetter},
        {0x16EA0, 0x16EB8, WBP::ALetter},
        {0x16EBB, 0x16ED3, WBP::ALetter},
        {0x16F00, 0x16F4A, WBP::ALetter},
        {0x16F4F, 0x16F4F, WBP::Extend},
        {0x16F50, 0x16F50, WBP::ALetter},
        {0x16F51, 0x16F87, WBP::Extend},
        {0x16F8F, 0x16F92, WBP::Extend},
        {0x16F93, 0x16F9F, WBP::ALetter},
        {0x16FE0, 0x16FE1, WBP::ALetter},
        {0x16FE3, 0x16FE3, WBP::ALetter},
        {0x16FE4, 0x16FE4, WBP::Extend},
        {0x16FF0, 0x16FF1, WBP::Extend},
        {0x1AFF0, 0x1AFF3, WBP::Katakana},
        {0x1AFF5, 0x1AFFB, WBP::Katakana},
        {0x1AFFD, 0x1AFFE, WBP::Katakana},
        {0x1B000, 0x1B000, WBP::Katakana},
        {0x1B120, 0x1B122, WBP::Katakana},
        {0x1B124, 0x1B128, WBP::Katakana},
        {0x1B155, 0x1B155, WBP::Katakana},
        {0x1B164, 0x1B168, WBP::Katakana},
        {0x1BC00, 0x1BC6A, WBP::ALetter},
        {0x1BC70, 0x1BC7C, WBP::ALetter},
        {0x1BC80, 0x1BC88, WBP::ALetter},
        {0x1BC90, 0x1BC99, WBP::ALetter},
        {0x1BC9D, 0x1BC9E, WBP::Extend},
        {0x1BCA0, 0x1BCA3, WBP::Format},
        {0x1CCF0, 0x1CCF9, WBP::Numeric},
        {0x1CF00, 0x1CF2D, WBP::Extend},
        {0x1CF30, 0x1CF46, WBP::Extend},
        {0x1D127, 0x1D128, WBP::Extend},
        {0x1D165, 0x1D169, WBP::Extend},
        {0x1D16D, 0x1D172, WBP::Extend},
        {0x1D173, 0x1D17A, WBP::Format},
        {0x1D17B, 0x1D182, WBP::Extend},
        {0x1D185, 0x1D18B, WBP::Extend},
        {0x1D1AA, 0x1D1AD, WBP::Extend},
        {0x1D242, 0x1D244, WBP::Extend},
        {0x1D250, 0x1D252, WBP::Extend},
        {0x1D25B, 0x1D25C, WBP::Extend},
        {0x1D25F, 0x1D25F, WBP::Extend},
        {0x1D280, 0x1D281, WBP::Extend},
        {0x1D400, 0x1D454, WBP::ALetter},
        {0x1D456, 0x1D49C, WBP::ALetter},
        {0x1D49E, 0x1D49F, WBP::ALetter},
        {0x1D4A2, 0x1D4A2, WBP::ALetter},
        {0x1D4A5, 0x1D4A6, WBP::ALetter},
        {0x1D4A9, 0x1D4AC, WBP::ALetter},
        {0x1D4AE, 0x1D4B9, WBP::ALetter},
        {0x1D4BB, 0x1D4BB, WBP::ALetter},
        {0x1D4BD, 0x1D4C3, WBP::ALetter},
        {0x1D4C5, 0x1D505, WBP::ALetter},
        {0x1D507, 0x1D50A, WBP::ALetter},
        {0x1D50D, 0x1D514, WBP::ALetter},
        {0x1D516, 0x1D51C, WBP::ALetter},
        {0x1D51E, 0x1D539, WBP::ALetter},
        {0x1D53B, 0x1D53E, WBP::ALetter},
        {0x1D540, 0x1D544, WBP::ALetter},
        {0x1D546, 0x1D546, WBP::ALetter},
        {0x1D54A, 0x1D550, WBP::ALetter},
        {0x1D552, 0x1D6A6, WBP::ALetter},
        {0x1D6A8, 0x1D6C0, WBP::ALetter},
        {0x1D6C2, 0x1D6DA, WBP::ALetter},
        {0x1D6DC, 0x1D6FA, WBP::ALetter},
        {0x1D6FC, 0x1D714, WBP::ALetter},
        {0x1D716, 0x1D734, WBP::ALetter},
        {0x1D736, 0x1D74E, WBP::ALetter},
        {0x1D750, 0x1D76E, WBP::ALetter},
        {0x1D770, 0x1D788, WBP::ALetter},
        {0x1D78A, 0x1D7A8, WBP::ALetter},
        {0x1D7AA, 0x1D7C2, WBP::ALetter},
        {0x1D7C4, 0x1D7CB, WBP::ALetter},
        {0x1D7CE, 0x1D7FF, WBP::Numeric},
        {0x1DA00, 0x1DA36, WBP::Extend},
        {0x1DA3B, 0x1DA6C, WBP::Extend},
        {0x1DA75, 0x1DA75, WBP::Extend},
        {0x1DA84, 0x1DA84, WBP::Extend},
        {0x1DA9B, 0x1DA9F, WBP::Extend},
        {0x1DAA1, 0x1DAAF, WBP::Extend},
        {0x1DF00, 0x1DF81, WBP::ALetter},
        {0x1DF90, 0x1DF96, WBP::ALetter},
        {0x1DFCD, 0x1DFFF, WBP::ALetter},
        {0x1E000, 0x1E006, WBP::Extend},
        {0x1E008, 0x1E018, WBP::Extend},
        {0x1E01B, 0x1E021, WBP::Extend},
        {0x1E023, 0x1E024, WBP::Extend},
        {0x1E026, 0x1E02A, WBP::Extend},
        {0x1E030, 0x1E06D, WBP::ALetter},
        {0x1E08F, 0x1E08F, WBP::Extend},
        {0x1E100, 0x1E12C, WBP::ALetter},
        {0x1E130, 0x1E136, WBP::Extend},
        {0x1E137, 0x1E13D, WBP::ALetter},
        {0x1E140, 0x1E149, WBP::Numeric},
        {0x1E14E, 0x1E14E, WBP::ALetter},
        {0x1E290, 0x1E2AD, WBP::ALetter},
        {0x1E2AE, 0x1E2AE, WBP::Extend},
        {0x1E2C0, 0x1E2EB, WBP::ALetter},
        {0x1E2EC, 0x1E2EF, WBP::Extend},
        {0x1E2F0, 0x1E2F9, WBP::Numeric},
        {0x1E4D0, 0x1E4EB, WBP::ALetter},
        {0x1E4EC, 0x1E4EF, WBP::Extend},
        {0x1E4F0, 0x1E4F9, WBP::Numeric},
        {0x1E5D0, 0x1E5ED, WBP::ALetter},
        {0x1E5EE, 0x1E5EF, WBP::Extend},
        {0x1E5F0, 0x1E5F0, WBP::ALetter},
        {0x1E5F1, 0x1E5FA, WBP::Numeric},
        {0x1E6C0, 0x1E6DE, WBP::ALetter},
        {0x1E6E0, 0x1E6E2, WBP::ALetter},
        {0x1E6E3, 0x1E6E3, WBP::Extend},
        {0x1E6E4, 0x1E6E5, WBP::ALetter},
        {0x1E6E6, 0x1E6E6, WBP::Extend},
        {0x1E6E7, 0x1E6ED, WBP::ALetter},
        {0x1E6EE, 0x1E6EF, WBP::Extend},
        {0x1E6F0, 0x1E6F4, WBP::ALetter},
        {0x1E6F5, 0x1E6F5, WBP::Extend},
        {0x1E6FE, 0x1E6FF, WBP::ALetter},
        {0x1E7E0, 0x1E7E6, WBP::ALetter},
        {0x1E7E8, 0x1E7EB, WBP::ALetter},
        {0x1E7ED, 0x1E7EE, WBP::ALetter},
        {0x1E7F0, 0x1E7FE, WBP::ALetter},
        {0x1E800, 0x1E8C4, WBP::ALetter},
        {0x1E8D0, 0x1E8D6, WBP::Extend},
        {0x1E900, 0x1E943, WBP::ALetter},
        {0x1E944, 0x1E94A, WBP::Extend},
        {0x1E94B, 0x1E94B, WBP::ALetter},
        {0x1E950, 0x1E959, WBP::Numeric},
        {0x1EE00, 0x1EE03, WBP::ALetter},
        {0x1EE05, 0x1EE1F, WBP::ALetter},
        {0x1EE21, 0x1EE22, WBP::ALetter},
        {0x1EE24, 0x1EE24, WBP::ALetter},
        {0x1EE27, 0x1EE27, WBP::ALetter},
        {0x1EE29, 0x1EE32, WBP::ALetter},
        {0x1EE34, 0x1EE37, WBP::ALetter},
        {0x1EE39, 0x1EE39, WBP::ALetter},
        {0x1EE3B, 0x1EE3B, WBP::ALetter},
        {0x1EE42, 0x1EE42, WBP::ALetter},
        {0x1EE47, 0x1EE47, WBP::ALetter},
        {0x1EE49, 0x1EE49, WBP::ALetter},
        {0x1EE4B, 0x1EE4B, WBP::ALetter},
        {0x1EE4D, 0x1EE4F, WBP::ALetter},
        {0x1EE51, 0x1EE52, WBP::ALetter},
        {0x1EE54, 0x1EE54, WBP::ALetter},
        {0x1EE57, 0x1EE57, WBP::ALetter},
        {0x1EE59, 0x1EE59, WBP::ALetter},
        {0x1EE5B, 0x1EE5B, WBP::ALetter},
        {0x1EE5D, 0x1EE5D, WBP::ALetter},
        {0x1EE5F, 0x1EE5F, WBP::ALetter},
        {0x1EE61, 0x1EE62, WBP::ALetter},
        {0x1EE64, 0x1EE64, WBP::ALetter},
        {0x1EE67, 0x1EE6A, WBP::ALetter},
        {0x1EE6C, 0x1EE72, WBP::ALetter},
        {0x1EE74, 0x1EE77, WBP::ALetter},
        {0x1EE79, 0x1EE7C, WBP::ALetter},
        {0x1EE7E, 0x1EE7E, WBP::ALetter},
        {0x1EE80, 0x1EE89, WBP::ALetter},
        {0x1EE8B, 0x1EE9B, WBP::ALetter},
        {0x1EEA1, 0x1EEA3, WBP::ALetter},
        {0x1EEA5, 0x1EEA9, WBP::ALetter},
        {0x1EEAB, 0x1EEBB, WBP::ALetter},
        {0x1F130, 0x1F149, WBP::ALetter},
        {0x1F150, 0x1F169, WBP::ALetter},
        {0x1F170, 0x1F189, WBP::ALetter},
        {0x1F1E6, 0x1F1FF, WBP::Regional_Indicator},
        {0x1F3FB, 0x1F3FF, WBP::Extend},
        {0x1FBF0, 0x1FBF9, WBP::Numeric},
        {0xE0001, 0xE0001, WBP::Format},
        {0xE0020, 0xE007F, WBP::Extend},
        {0xE0100, 0xE01EF, WBP::Extend},
}};

// clang-format on

// Construct table of just WBP::Extend character intervals
constexpr auto g_extend_characters{[]() constexpr {
  // Compute number of extend character intervals
  constexpr size_t size = []() constexpr {
    size_t count = 0;
    for (auto interval : g_word_break_intervals) {
      if (interval.property == WBP::Extend) {
        count++;
      }
    }
    return count;
  }();

  // Create array of extend character intervals
  std::array<Interval, size> result{};
  size_t index = 0;
  for (auto interval : g_word_break_intervals) {
    if (interval.property == WBP::Extend) {
      result[index++] = {interval.first, interval.last};  // NOLINT
    }
  }
  return result;
}()};

// Find a codepoint inside a sorted list of Interval.
template <size_t N>
bool Bisearch(uint32_t ucs, const std::array<Interval, N>& table) {
  if (ucs < table.front().first || ucs > table.back().last) {  // NOLINT
    return false;
  }

  int min = 0;
  int max = N - 1;
  while (max >= min) {
    const int mid = (min + max) / 2;
    if (ucs > table[mid].last) {  // NOLINT
      min = mid + 1;
    } else if (ucs < table[mid].first) {  // NOLINT
      max = mid - 1;
    } else {
      return true;
    }
  }

  return false;
}

// Find a value inside a sorted list of Interval + property.
template <class C, size_t N>
bool Bisearch(uint32_t ucs, const std::array<C, N>& table, C* out) {
  if (ucs < table.front().first || ucs > table.back().last) {  // NOLINT
    return false;
  }

  int min = 0;
  int max = N - 1;
  while (max >= min) {
    const int mid = (min + max) / 2;
    if (ucs > table[mid].last) {  // NOLINT
      min = mid + 1;
    } else if (ucs < table[mid].first) {  // NOLINT
      max = mid - 1;
    } else {
      *out = table[mid];  // NOLINT
      return true;
    }
  }

  return false;
}

// VARIATION SELECTOR-16 requests the emoji presentation of the preceding
// character. Terminals draw it two cells wide.
constexpr uint32_t kEmojiPresentationSelector = 0xFE0F;

int codepoint_width(uint32_t ucs) {
  if (ftxui::IsControl(ucs)) {
    return -1;
  }

  if (ftxui::IsCombining(ucs)) {
    return 0;
  }

  if (ftxui::IsFullWidth(ucs)) {
    return 2;
  }

  return 1;
}

}  // namespace

namespace ftxui {

// From UTF8 encoded string |input|, eat in between 1 and 4 byte representing
// one codepoint. Put the codepoint into |ucs|. Start at |start| and update
// |end| to represent the beginning of the next byte to eat for consecutive
// executions.
bool EatCodePoint(std::string_view input,
                  size_t start,
                  size_t* end,
                  uint32_t* ucs) {
  if (start >= input.size()) {
    *end = start + 1;
    return false;
  }
  const uint8_t C0 = input[start];

  // 1 byte string.
  if ((C0 & 0b1000'0000) == 0b0000'0000) {  // NOLINT
    *ucs = C0 & 0b0111'1111;                // NOLINT
    *end = start + 1;
    return true;
  }

  // 2 byte string.
  if ((C0 & 0b1110'0000) == 0b1100'0000 &&  // NOLINT
      start + 1 < input.size()) {
    const uint8_t C1 = input[start + 1];
    *ucs = 0;
    *ucs += C0 & 0b0001'1111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C1 & 0b0011'1111;  // NOLINT
    *end = start + 2;
    return true;
  }

  // 3 byte string.
  if ((C0 & 0b1111'0000) == 0b1110'0000 &&  // NOLINT
      start + 2 < input.size()) {
    const uint8_t C1 = input[start + 1];
    const uint8_t C2 = input[start + 2];
    *ucs = 0;
    *ucs += C0 & 0b0000'1111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C1 & 0b0011'1111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C2 & 0b0011'1111;  // NOLINT
    *end = start + 3;
    return true;
  }

  // 4 byte string.
  if ((C0 & 0b1111'1000) == 0b1111'0000 &&  // NOLINT
      start + 3 < input.size()) {
    const uint8_t C1 = input[start + 1];
    const uint8_t C2 = input[start + 2];
    const uint8_t C3 = input[start + 3];
    *ucs = 0;
    *ucs += C0 & 0b0000'0111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C1 & 0b0011'1111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C2 & 0b0011'1111;  // NOLINT
    *ucs <<= 6;                // NOLINT
    *ucs += C3 & 0b0011'1111;  // NOLINT
    *end = start + 4;
    return true;
  }

  *end = start + 1;
  return false;
}

// From UTF16 encoded string |input|, eat in between 1 and 4 byte representing
// one codepoint. Put the codepoint into |ucs|. Start at |start| and update
// |end| to represent the beginning of the next byte to eat for consecutive
// executions.
bool EatCodePoint(std::wstring_view input,
                  size_t start,
                  size_t* end,
                  uint32_t* ucs) {
  if (start >= input.size()) {
    *end = start + 1;
    return false;
  }

  // On linux wstring uses the UTF32 encoding:
  if constexpr (sizeof(wchar_t) == 4) {
    *ucs = input[start];  // NOLINT
    *end = start + 1;
    return true;
  }

  // On windows, wstring uses the UTF16 encoding:
  int32_t C0 = input[start];  // NOLINT

  // 1 word size:
  if (C0 < 0xd800 || C0 >= 0xdc00) {  // NOLINT
    *ucs = C0;
    *end = start + 1;
    return true;
  }

  // 2 word size:
  if (start + 1 >= input.size()) {
    *end = start + 2;
    return false;
  }

  int32_t C1 = input[start + 1];                         // NOLINT
  *ucs = ((C0 & 0x3ff) << 10) + (C1 & 0x3ff) + 0x10000;  // NOLINT
  *end = start + 2;
  return true;
}

bool IsCombining(uint32_t ucs) {
  return Bisearch(ucs, g_extend_characters);
}

bool IsFullWidth(uint32_t ucs) {
  if (ucs < 0x0300) {  // Quick path: // NOLINT
    return false;
  }

  return Bisearch(ucs, g_full_width_characters);
}

bool IsControl(uint32_t ucs) {
  if (ucs == 0) {
    return true;
  }
  if (ucs < 32) {  // NOLINT
    const uint32_t LINE_FEED = 10;
    return ucs != LINE_FEED;
  }
  if (ucs >= 0x7f && ucs < 0xa0) {  // NOLINT
    return true;
  }
  return false;
}

WordBreakProperty CodepointToWordBreakProperty(uint32_t codepoint) {
  WordBreakPropertyInterval interval = {0, 0, WBP::ALetter};
  std::ignore = Bisearch(codepoint, g_word_break_intervals, &interval);
  return interval.property;
}

int wchar_width(wchar_t ucs) {
  return codepoint_width(uint32_t(ucs));
}

int wstring_width(const std::wstring& text) {
  int width = 0;

  for (const wchar_t& it : text) {
    const int w = wchar_width(it);
    if (w < 0) {
      return -1;
    }
    width += w;
  }
  return width;
}

// Return how many cells the UTF8 encoded string |input| is taking when printed.
// Control characters are not taking any space, combining characters are
// modifying the previous character and are not taking any space, fullwidth
// characters are taking two cells and all the other characters are taking one
// cell.
int string_width(std::string_view input) {
  // 1-byte optimization: This function is often called on a single ASCII
  // character, so we can optimize this case by skipping the UTF8 decoding.
  if (input.size() == 1) {
    const char c = input[0];
    if (c >= 32 && c < 127) {  // NOLINT
      return 1;
    }
  }

  // ASCII optimization: If the string is pure ASCII, we can skip the UTF8
  // decoding and just count the number of characters, ignoring control
  // characters.
  bool is_pure_ascii = true;
  for (const char c : input) {
    if (c < 31 || c >= 127) {  // NOLINT
      is_pure_ascii = false;
      break;
    }
  }
  if (is_pure_ascii) {
    return static_cast<int>(input.size());
  }

  int width = 0;
  bool previous_narrow = false;
  size_t start = 0;
  while (start < input.size()) {
    uint32_t codepoint = 0;
    if (!EatCodePoint(input, start, &start, &codepoint)) {
      continue;
    }

    if (IsControl(codepoint)) {
      continue;
    }

    if (IsCombining(codepoint)) {
      if (codepoint == kEmojiPresentationSelector && previous_narrow) {
        previous_narrow = false;
        width += 1;
      }
      continue;
    }

    if (IsFullWidth(codepoint)) {
      previous_narrow = false;
      width += 2;
      continue;
    }

    previous_narrow = true;
    width += 1;
  }
  return width;
}

std::vector<std::string> Utf8ToGlyphs(std::string_view input) {
  std::vector<std::string> out;
  out.reserve(input.size());
  size_t last_glyph = 0;
  bool last_glyph_narrow = false;
  size_t start = 0;
  size_t end = 0;
  while (start < input.size()) {
    uint32_t codepoint = 0;
    if (!EatCodePoint(input, start, &end, &codepoint)) {
      start = end;
      continue;
    }

    const auto append = input.substr(start, end - start);
    start = end;

    // Ignore control characters.
    if (IsControl(codepoint)) {
      continue;
    }

    // Combining characters are put with the previous glyph they are modifying.
    if (IsCombining(codepoint)) {
      if (!out.empty()) {
        out[last_glyph] += append;
      }
      // The emoji presentation makes a narrow glyph take two cells.
      if (codepoint == kEmojiPresentationSelector && last_glyph_narrow) {
        last_glyph_narrow = false;
        out.emplace_back("");
      }
      continue;
    }

    last_glyph = out.size();

    // Fullwidth characters take two cells. The second is made of the empty
    // string to reserve the space the first is taking.
    if (IsFullWidth(codepoint)) {
      last_glyph_narrow = false;
      out.emplace_back(append);
      out.emplace_back("");
      continue;
    }

    // Normal characters:
    last_glyph_narrow = true;
    out.emplace_back(append);
  }
  return out;
}

size_t GlyphPrevious(std::string_view input, size_t start) {
  while (true) {
    if (start == 0) {
      return 0;
    }
    start--;

    // Skip the UTF8 continuation bytes.
    if ((input[start] & 0b1100'0000) == 0b1000'0000) {
      continue;
    }

    uint32_t codepoint = 0;
    size_t end = 0;
    const bool eaten = EatCodePoint(input, start, &end, &codepoint);

    // Ignore invalid, control characters and combining characters.
    if (!eaten || IsControl(codepoint) || IsCombining(codepoint)) {
      continue;
    }

    return start;
  }
}

size_t GlyphNext(std::string_view input, size_t start) {
  bool glyph_found = false;
  while (start < input.size()) {
    size_t end = 0;
    uint32_t codepoint = 0;
    const bool eaten = EatCodePoint(input, start, &end, &codepoint);

    // Ignore invalid, control characters and combining characters.
    if (!eaten || IsControl(codepoint) || IsCombining(codepoint)) {
      start = end;
      continue;
    }

    // We eat the beginning of the next glyph. If we are eating the one
    // requested, return its start position immediately.
    if (glyph_found) {
      return static_cast<int>(start);
    }

    // Otherwise, skip this glyph and iterate:
    glyph_found = true;
    start = end;
  }
  return static_cast<int>(input.size());
}

size_t GlyphIterate(std::string_view input, int glyph_offset, size_t start) {
  if (glyph_offset >= 0) {
    for (int i = 0; i < glyph_offset; ++i) {
      start = GlyphNext(input, start);
    }
    return start;
  } else {
    for (int i = 0; i < -glyph_offset; ++i) {
      start = GlyphPrevious(input, start);
    }
    return start;
  }
}

std::vector<int> CellToGlyphIndex(std::string_view input) {
  int x = -1;
  bool last_glyph_narrow = false;
  std::vector<int> out;
  out.reserve(input.size());
  size_t start = 0;
  size_t end = 0;
  while (start < input.size()) {
    uint32_t codepoint = 0;
    const bool eaten = EatCodePoint(input, start, &end, &codepoint);
    start = end;

    // Ignore invalid / control characters.
    if (!eaten || IsControl(codepoint)) {
      continue;
    }

    // Combining characters are put with the previous glyph they are modifying.
    if (IsCombining(codepoint)) {
      if (x == -1) {
        ++x;
        out.push_back(x);
      }
      // The emoji presentation makes a narrow glyph take two cells.
      if (codepoint == kEmojiPresentationSelector && last_glyph_narrow) {
        last_glyph_narrow = false;
        out.push_back(x);
      }
      continue;
    }

    // Fullwidth characters take two cells. The second is made of the empty
    // string to reserve the space the first is taking.
    if (IsFullWidth(codepoint)) {
      last_glyph_narrow = false;
      ++x;
      out.push_back(x);
      out.push_back(x);
      continue;
    }

    // Normal characters:
    last_glyph_narrow = true;
    ++x;
    out.push_back(x);
  }
  return out;
}

int GlyphCount(std::string_view input) {
  int size = 0;
  size_t start = 0;
  size_t end = 0;
  while (start < input.size()) {
    uint32_t codepoint = 0;
    const bool eaten = EatCodePoint(input, start, &end, &codepoint);
    start = end;

    // Ignore invalid characters:
    if (!eaten || IsControl(codepoint)) {
      continue;
    }

    // Ignore combining characters, except when they don't have a preceding to
    // combine with.
    if (IsCombining(codepoint)) {
      if (size == 0) {
        size++;
      }
      continue;
    }

    size++;
  }
  return size;
}

std::vector<WordBreakProperty> Utf8ToWordBreakProperty(std::string_view input) {
  std::vector<WordBreakProperty> out;
  out.reserve(input.size());
  size_t start = 0;
  size_t end = 0;
  while (start < input.size()) {
    uint32_t codepoint = 0;
    if (!EatCodePoint(input, start, &end, &codepoint)) {
      start = end;
      continue;
    }
    start = end;

    // Ignore control characters.
    if (IsControl(codepoint)) {
      continue;
    }

    // Ignore combining characters.
    if (IsCombining(codepoint)) {
      continue;
    }

    WordBreakPropertyInterval interval = {0, 0, WBP::ALetter};
    std::ignore = Bisearch(codepoint, g_word_break_intervals, &interval);
    out.push_back(interval.property);
  }
  return out;
}

/// Convert a std::wstring into a UTF8 std::string.
std::string to_string(std::wstring_view s) {
  std::string out;

  size_t i = 0;
  uint32_t codepoint = 0;
  while (EatCodePoint(s, i, &i, &codepoint)) {
    // Code point <-> UTF-8 conversion
    //
    // ┏━━━━━━━━┳━━━━━━━━┳━━━━━━━━┳━━━━━━━━┓
    // ┃Byte 1  ┃Byte 2  ┃Byte 3  ┃Byte 4  ┃
    // ┡━━━━━━━━╇━━━━━━━━╇━━━━━━━━╇━━━━━━━━┩
    // │0xxxxxxx│        │        │        │
    // ├────────┼────────┼────────┼────────┤
    // │110xxxxx│10xxxxxx│        │        │
    // ├────────┼────────┼────────┼────────┤
    // │1110xxxx│10xxxxxx│10xxxxxx│        │
    // ├────────┼────────┼────────┼────────┤
    // │11110xxx│10xxxxxx│10xxxxxx│10xxxxxx│
    // └────────┴────────┴────────┴────────┘

    // 1 byte UTF8
    if (codepoint <= 0b000'0000'0111'1111) {  // NOLINT
      const uint8_t p1 = codepoint;
      out.push_back(p1);  // NOLINT
      continue;
    }

    // 2 bytes UTF8
    if (codepoint <= 0b000'0111'1111'1111) {  // NOLINT
      uint8_t p2 = codepoint & 0b111111;      // NOLINT
      codepoint >>= 6;                        // NOLINT
      uint8_t p1 = codepoint;                 // NOLINT
      out.push_back(0b11000000 + p1);         // NOLINT
      out.push_back(0b10000000 + p2);         // NOLINT
      continue;
    }

    // 3 bytes UTF8
    if (codepoint <= 0b1111'1111'1111'1111) {  // NOLINT
      uint8_t p3 = codepoint & 0b111111;       // NOLINT
      codepoint >>= 6;                         // NOLINT
      uint8_t p2 = codepoint & 0b111111;       // NOLINT
      codepoint >>= 6;                         // NOLINT
      uint8_t p1 = codepoint;                  // NOLINT
      out.push_back(0b11100000 + p1);          // NOLINT
      out.push_back(0b10000000 + p2);          // NOLINT
      out.push_back(0b10000000 + p3);          // NOLINT
      continue;
    }

    // 4 bytes UTF8
    if (codepoint <= 0b1'0000'1111'1111'1111'1111) {  // NOLINT
      uint8_t p4 = codepoint & 0b111111;              // NOLINT
      codepoint >>= 6;                                // NOLINT
      uint8_t p3 = codepoint & 0b111111;              // NOLINT
      codepoint >>= 6;                                // NOLINT
      uint8_t p2 = codepoint & 0b111111;              // NOLINT
      codepoint >>= 6;                                // NOLINT
      uint8_t p1 = codepoint;                         // NOLINT
      out.push_back(0b11110000 + p1);                 // NOLINT
      out.push_back(0b10000000 + p2);                 // NOLINT
      out.push_back(0b10000000 + p3);                 // NOLINT
      out.push_back(0b10000000 + p4);                 // NOLINT
      continue;
    }

    // Something else?
  }
  return out;
}

/// Convert a UTF8 std::string into a std::wstring.
std::wstring to_wstring(std::string_view s) {
  std::wstring out;

  size_t i = 0;
  uint32_t codepoint = 0;
  while (EatCodePoint(s, i, &i, &codepoint)) {
    // On linux wstring are UTF32 encoded:
    if constexpr (sizeof(wchar_t) == 4) {
      out.push_back(codepoint);  // NOLINT
      continue;
    }

    // On Windows, wstring are UTF16 encoded:

    // Codepoint encoded using 1 word:
    // NOLINTNEXTLINE
    if (codepoint < 0xD800 || (codepoint > 0xDFFF && codepoint < 0x10000)) {
      uint16_t p0 = codepoint;  // NOLINT
      out.push_back(p0);        // NOLINT
      continue;
    }

    // Codepoint encoded using 2 words:
    codepoint -= 0x010000;                               // NOLINT
    uint16_t p0 = (((codepoint << 12) >> 22) + 0xD800);  // NOLINT
    uint16_t p1 = (((codepoint << 22) >> 22) + 0xDC00);  // NOLINT
    out.push_back(p0);                                   // NOLINT
    out.push_back(p1);                                   // NOLINT
  }
  return out;
}

}  // namespace ftxui

// ---- screen\surface.cpp ------------------------------------------


namespace ftxui {

namespace {
Cell& dev_null_cell() {
  static Cell cell;
  return cell;
}
}  // namespace

Surface::Surface(int dimx, int dimy)
    : stencil{0, std::max(0, dimx) - 1, 0, std::max(0, dimy) - 1},
      dimx_(std::max(0, dimx)),
      dimy_(std::max(0, dimy)),
      cells_(static_cast<size_t>(dimx_) * static_cast<size_t>(dimy_)) {}

/// @brief Access a character in a cell at a given position.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
std::string& Surface::at(int x, int y) {
  return CellAt(x, y).character;
}

/// @brief Access a character in a cell at a given position.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
const std::string& Surface::at(int x, int y) const {
  return CellAt(x, y).character;
}

/// @brief Access a cell (Cell) at a given position.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
Cell& Surface::CellAt(int x, int y) {
  return stencil.Contain(x, y) ? FastCellAt(x, y) : dev_null_cell();
}

/// @brief Access a cell (Cell) at a given position.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
const Cell& Surface::CellAt(int x, int y) const {
  return stencil.Contain(x, y) ? FastCellAt(x, y) : dev_null_cell();
}

/// @brief Access a cell (Cell) at a given position, without stencil check.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
Cell& Surface::FastCellAt(int x, int y) {
  return cells_[static_cast<size_t>(y) * static_cast<size_t>(dimx_) +
                static_cast<size_t>(x)];
}

/// @brief Access a cell (Cell) at a given position, without stencil check.
/// @param x The cell position along the x-axis.
/// @param y The cell position along the y-axis.
const Cell& Surface::FastCellAt(int x, int y) const {
  return cells_[static_cast<size_t>(y) * static_cast<size_t>(dimx_) +
                static_cast<size_t>(x)];
}

/// @brief Clear all the cells from the surface.
void Surface::Clear() {
  std::fill(cells_.begin(), cells_.end(), Cell());
}

void Surface::Reserved1() {}
void Surface::Reserved2() {}
void Surface::Reserved3() {}
void Surface::Reserved4() {}
void Surface::Reserved5() {}
void Surface::Reserved6() {}
void Surface::Reserved7() {}
void Surface::Reserved8() {}

}  // namespace ftxui

// ---- screen\terminal.cpp -----------------------------------------


#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif

#else
#endif
#if defined(__sun) || defined(__illumos__)
#endif

namespace ftxui {

namespace {

std::unique_ptr<Terminal::Quirks> g_quirks;
bool g_color_support_detected = false;

bool& ColorSupportDetected() {
  return g_color_support_detected;
}

Terminal::Quirks& GetQuirksInternal() {
  if (!g_quirks) {
    g_quirks = std::make_unique<Terminal::Quirks>();
#if defined(_WIN32)
    g_quirks->SetBlockCharacters(false);
    g_quirks->SetCursorHiding(false);
    g_quirks->SetComponentAscii(true);
#endif
  }
  return *g_quirks;
}

Dimensions& FallbackSize() {
#if defined(__EMSCRIPTEN__)
  // This dimension was chosen arbitrarily to be able to display:
  // https://arthursonzogni.com/FTXUI/examples
  // This will have to be improved when someone has time to implement and need
  // it.
  constexpr int fallback_width = 140;
  constexpr int fallback_height = 43;
#else
  // The terminal size in VT100 was 80x24. It is still used nowadays by
  // default in many terminal emulator. That's a good choice for a fallback
  // value.
  constexpr int fallback_width = 80;
  constexpr int fallback_height = 24;
#endif
  static Dimensions g_fallback_size{
      fallback_width,
      fallback_height,
  };
  return g_fallback_size;
}

bool Contains(std::string_view s, std::string_view key) {
  if (key.empty()) {
    return true;
  }
  const auto it = std::search(  // NOLINT
      s.begin(), s.end(), key.begin(), key.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
      });
  return it != s.end();
}

bool ContainsAny(std::string_view s,
                 std::initializer_list<std::string_view> keys) {
  for (const std::string_view key : keys) {
    if (Contains(s, key)) {
      return true;
    }
  }
  return false;
}

Terminal::Color ComputeColorSupportInternal() {
  static const std::vector<int> empty_capabilities;
  return Terminal::ComputeColorSupport(
      util::GetEnv("TERM"), util::GetEnv("COLORTERM"),
      util::GetEnv("TERM_PROGRAM"), "unknown", "unknown", empty_capabilities);
}

}  // namespace

namespace Terminal {

struct Quirks::Impl {
  bool block_characters = true;
  bool cursor_hiding = true;
  bool component_ascii = false;
  Color color_support = Palette256;
};

Quirks::Quirks() : impl_(std::make_unique<Impl>()) {}
Quirks::~Quirks() = default;
Quirks::Quirks(const Quirks& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}
Quirks& Quirks::operator=(const Quirks& other) {
  if (this != &other) {
    *impl_ = *other.impl_;
  }
  return *this;
}
Quirks::Quirks(Quirks&&) noexcept = default;
Quirks& Quirks::operator=(Quirks&&) noexcept = default;

bool Quirks::BlockCharacters() const {
  return impl_->block_characters;
}
void Quirks::SetBlockCharacters(bool v) {
  impl_->block_characters = v;
}

bool Quirks::CursorHiding() const {
  return impl_->cursor_hiding;
}
void Quirks::SetCursorHiding(bool v) {
  impl_->cursor_hiding = v;
}

bool Quirks::ComponentAscii() const {
  return impl_->component_ascii;
}
void Quirks::SetComponentAscii(bool v) {
  impl_->component_ascii = v;
}

Color Quirks::ColorSupport() const {
  return impl_->color_support;
}
void Quirks::SetColorSupport(Color v) {
  impl_->color_support = v;
}

struct TerminalInfo::Impl {
  std::string term;
  std::string colorterm;
  std::string term_program;
  std::string terminal_name;
  std::string terminal_emulator_name;
  std::vector<int> capabilities;
};

TerminalInfo::TerminalInfo() : impl_(std::make_unique<Impl>()) {}
TerminalInfo::~TerminalInfo() = default;
TerminalInfo::TerminalInfo(TerminalInfo&&) noexcept = default;
TerminalInfo& TerminalInfo::operator=(TerminalInfo&&) noexcept = default;

void TerminalInfo::SetTerm(std::string_view term) {
  impl_->term = term;
}
void TerminalInfo::SetColorterm(std::string_view colorterm) {
  impl_->colorterm = colorterm;
}
void TerminalInfo::SetTermProgram(std::string_view term_program) {
  impl_->term_program = term_program;
}
void TerminalInfo::SetTerminalName(std::string_view terminal_name) {
  impl_->terminal_name = terminal_name;
}
void TerminalInfo::SetTerminalEmulatorName(
    std::string_view terminal_emulator_name) {
  impl_->terminal_emulator_name = terminal_emulator_name;
}
void TerminalInfo::SetCapabilities(std::vector<int> capabilities) {
  impl_->capabilities = std::move(capabilities);
}

/// @brief Compute the color support based on environment variables and terminal
/// identification.
/// @param term The TERM environment variable.
/// @param colorterm The COLORTERM environment variable.
/// @param term_program The TERM_PROGRAM environment variable.
/// @param terminal_name The terminal name (from DA2).
/// @param terminal_emulator_name The terminal emulator name (from XTVERSION).
/// @param capabilities The terminal capabilities (from DA1).
Color ComputeColorSupport(std::string_view term,
                          std::string_view colorterm,
                          std::string_view term_program,
                          std::string_view terminal_name,
                          std::string_view terminal_emulator_name,
                          const std::vector<int>& capabilities) {
  TerminalInfo info;
  info.SetTerm(term);
  info.SetColorterm(colorterm);
  info.SetTermProgram(term_program);
  info.SetTerminalName(terminal_name);
  info.SetTerminalEmulatorName(terminal_emulator_name);
  info.SetCapabilities(capabilities);
  return info.ComputeColorSupport();
}

Color TerminalInfo::ComputeColorSupport() const {
  // TODO(v8.0.0): Read NO_COLOR and WT_SESSION from
  // ComputeColorSupportInternal() and pass them in as parameters, so that this
  // function remains a pure function of its inputs. This requires extending the
  // public Terminal::ComputeColorSupport() signature, i.e. an API-breaking
  // change.

  // 0. User preference. See https://no-color.org.
  if (util::GetEnv("NO_COLOR")[0] != '\0') {
    return Terminal::Color::Palette1;
  }

  // 1. Platform specific overrides.
#if defined(__EMSCRIPTEN__)
  return Terminal::Color::TrueColor;
#endif
#if defined(_WIN32)
  // Check if we are running in a console, and if that console supports VT processing.
  auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD out_mode = 0;
  if (GetConsoleMode(stdout_handle, &out_mode)) {
    const int enable_virtual_terminal_processing = 0x0004;
    const int disable_newline_auto_return = 0x0008;
    out_mode |= enable_virtual_terminal_processing;
    out_mode |= disable_newline_auto_return;
    if (!SetConsoleMode(stdout_handle, out_mode)) {
      return Terminal::Color::Palette16;
    }
  }
  return Terminal::Color::TrueColor;
#endif

  // Check WT_SESSION for Windows Terminal (e.g. when running under WSL).
  if (util::GetEnv("WT_SESSION")[0] != '\0') {
    return Terminal::Color::TrueColor;
  }

  // 2. term / colorterm environment variables explicitly advertising 24bit
  // colors.
  if (ContainsAny(impl_->colorterm, {"24bit", "truecolor"})) {
    return Terminal::Color::TrueColor;
  }
  if (ContainsAny(impl_->term,
                  {"direct", "truecolor", "kitty", "alacritty", "foot"})) {
    return Terminal::Color::TrueColor;
  }

  // 3. term_program
  if (ContainsAny(impl_->term_program, {
                                           "iterm",
                                           "vscode",
                                           "warp",
                                           "ghostty",
                                           "wezterm",
                                       })) {
    return Terminal::Color::TrueColor;
  }
  // Apple's Terminal.app (TERM_PROGRAM=Apple_Terminal) supports 256 colors,
  // but not 24bit ones.
  if (Contains(impl_->term_program, "apple_terminal")) {
    return Terminal::Color::Palette256;
  }

  // 4. terminal identification.
  // An empty name means the terminal was not identified, the same as
  // "unknown".
  if (!impl_->terminal_emulator_name.empty() &&
      impl_->terminal_emulator_name != "unknown") {
    return Terminal::Color::TrueColor;
  }
  if (impl_->terminal_name == "xterm") {
    return Terminal::Color::TrueColor;
  }

  // 5. Generic term / colorterm environment variables. They are checked after
  // the terminal identification, because they are weak evidence: most
  // terminals set TERM=xterm-256color, whatever their capabilities.
  if (ContainsAny(impl_->colorterm, {"256"}) ||
      ContainsAny(impl_->term, {"256", "xterm", "screen", "tmux"})) {
    return Terminal::Color::Palette256;
  }

  // 6. Terminal capabilities.
  for (const int x : impl_->capabilities) {
    // The value 22 is the SGR capability for 256 colors. If the terminal
    // supports it, it is a strong indication that the terminal supports 256
    // colors. This is not a perfect detection method, but it is a reasonable
    // heuristic in the absence of more specific information.
    if (x == 22) {
      return Terminal::Color::Palette256;
    }
  }

  return Terminal::Color::Palette16;
}

/// @brief Get the terminal size.
/// @return The terminal size.
/// @ingroup screen
Dimensions Size() {
#if defined(__EMSCRIPTEN__)
  // This dimension was chosen arbitrarily to be able to display:
  // https://arthursonzogni.com/FTXUI/examples
  // This will have to be improved when someone has time to implement and need
  // it.
  return FallbackSize();
#elif defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    return Dimensions{csbi.srWindow.Right - csbi.srWindow.Left + 1,
                      csbi.srWindow.Bottom - csbi.srWindow.Top + 1};
  }

  return FallbackSize();
#else
  winsize w{};
  const int status = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);  // NOLINT
  // The ioctl return value result should be checked. Some operating systems
  // don't support TIOCGWINSZ.
  if (w.ws_col == 0 || w.ws_row == 0 || status < 0) {
    return FallbackSize();
  }
  return Dimensions{w.ws_col, w.ws_row};
#endif
}

/// @brief Override terminal size in case auto-detection fails
/// @param fallbackSize Terminal dimensions to fallback to
void SetFallbackSize(const Dimensions& fallbackSize) {
  FallbackSize() = fallbackSize;
}

/// @brief Get the color support of the terminal.
/// @ingroup screen
Color ColorSupport() {
  if (!ColorSupportDetected()) {
    GetQuirksInternal().SetColorSupport(ComputeColorSupportInternal());
    ColorSupportDetected() = true;
  }
  return GetQuirksInternal().ColorSupport();
}

/// @brief Override terminal color support in case auto-detection fails
/// @ingroup dom
void SetColorSupport(Color color) {
  GetQuirksInternal().SetColorSupport(color);
  ColorSupportDetected() = true;
}

/// @brief Get the terminal quirks.
/// @ingroup screen
Quirks GetQuirks() {
  if (!ColorSupportDetected()) {
    GetQuirksInternal().SetColorSupport(ComputeColorSupportInternal());
    ColorSupportDetected() = true;
  }
  return GetQuirksInternal();
}

/// @brief Override terminal quirks.
/// @ingroup screen
void SetQuirks(const Quirks& quirks) {
  GetQuirksInternal() = quirks;
  ColorSupportDetected() = true;
}

}  // namespace Terminal
}  // namespace ftxui

// ---- util\ref.cpp ------------------------------------------------


namespace ftxui {

namespace {

// StringRef, ConstStringRef, and ConstStringListRef are exported
// (FTXUI_EXPORT(SCREEN)), but all of their member functions -- including
// ones inherited from Ref<string>/ConstRef<string> -- are defined inline in
// the header and never odr-used from within libftxui-screen itself. On
// Windows, MSVC only emits (and therefore dllexports) an inline member
// function into the DLL that first odr-uses it; otherwise consumers linking
// against the dllimport declaration get an unresolved external symbol. This
// function exists solely to force that instantiation.
[[maybe_unused]] void ForceSymbolInstantiation() {
  StringRef s = "a";
  StringRef s2(s);
  StringRef s3(std::move(s2));
  s = s3;
  s = std::move(s3);
  (void)s();
  (void)*s;
  (void)s.operator->();

  ConstStringRef c = "a";
  ConstStringRef c2(c);
  ConstStringRef c3(std::move(c2));
  c = c3;
  c = std::move(c3);
  (void)c();
  (void)*c;

  ConstStringListRef l;
  ConstStringListRef l2(l);
  ConstStringListRef l3(std::move(l2));
  l = l3;
  l = std::move(l3);
  (void)l.size();
  (void)l[0];
}

}  // namespace

}  // namespace ftxui

#endif // FTXUI_IMPLEMENTATION

#endif // FTXUI_AMALGAMATED_HPP
