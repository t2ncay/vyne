# 🌿 Vyne

> **A C‑flavored scripting language with optional strict typing, structs via `interface`, and a standard library that reaches from VMath to VGLib 3D and VServ HTTP.**

[![Vyne CI](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml)
[![pages-build-deployment](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Version](https://img.shields.io/badge/version-0.0.4--alpha-orange.svg)](https://github.com/tuncaygafarli/vyne)

---

## 📖 Overview

Vyne is a hybrid, interpreted language with C-style syntax, strong type inference, and a rich standard library. It features:

- **Two execution modes**: Tree‑walking interpreter for fast iteration, or a C transpiler for standalone native binaries
- **Optional strict typing**: Use `::` for explicit types or `=` for inference
- **Rich standard library**: VMath (numerics), VGLib (2D/3D graphics), VAudio (DSP & audio), VServ (HTTP/WebSocket), VMem (low-level memory)
- **Interface-based structs**: `interface` blocks declare typed fields and methods together
- **Ruleset system**: Fine-grained control over type checking, memory limits, and performance
- **No GC pauses in C output**: Arena-allocated memory with deterministic cleanup

```vyne
// hello.vy — Vyne in action
msg = "vyne" + " language";
out(msg); // => "vyne language"
```

---

## 🚀 Quick Start

### Installation

```bash
git clone https://github.com/tuncaygafarli/vyne.git
cd vyne
make
```

### Run Your First Program

```bash
./vynec examples/hello.vy
```

### Transpile to C and Compile

```bash
./vynec --compile examples/logic_test.vy
# Generates logic_test.vy.c and compiles to logic_test.exe
```

---

## 🏗️ Architecture

Vyne uses a single AST with two execution paths:

```
source.vy → Lexer → Parser → AST → ┌─ Interpreter ────┐
                                    │                  │
                                    └─ C Transpiler ──┘
                                           │
                                           ▼
                                      arena‑allocated C
```

| Component       | Description                                                                                                                    |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| **Lexer**       | `tokenize()` streams a flat `vector<Token>`, handling string interpolation (`"{expr}"`) inline                                 |
| **Parser**      | Recursive‑descent `Parser` builds a typed `unique_ptr<ASTNode>` tree, tracking scopes for warnings                             |
| **Interpreter** | Each node's `evaluate(env, scopeId)` walks the tree against a `SymbolContainer` keyed by scope                                 |
| **Transpiler**  | `Codegen` + `C_Emitter` lower the same AST to a single C translation unit, then `arena_free_all()` at exit                     |
| **Value Model** | `Value` is a tagged union — primitives live inline; Array/Map/Struct/Function/Module are boxed behind `shared_ptr<VyneObject>` |
| **StringPool**  | Identifiers and field names are interned once into a global pool; symbol lookups compare `uint32_t` IDs instead of strings     |

---

## ⚙️ The Transpilation Engine: C as High-Level Assembly

Vyne's C transpiler bridges high-level expressive syntax with low-level machine performance.

### How It Works: From AST to Binary

1. **AST Flattening**: Complex, nested expressions are decomposed into a linear sequence of C statements
2. **Mangled Namespacing**: `group` and `interface` structures use deterministic mangling (e.g., `Master.Element.getName()` → `fn_Master_Element_getName`)
3. **Implicit Header Injection**: Automatically links with `vyne_runtime.h` for the core `Value` system, arena memory management, and built-in operations

### Why Transpile to C?

- **Zero Overhead Portability**: Runs anywhere with a C compiler (GCC, Clang, MSVC)
- **Aggressive Optimization**: Inherits decades of C compiler optimization research (loop unrolling, vectorization)
- **Embedded Friendly**: Resulting binaries start at ~50 KB, suitable for resource-constrained environments

### Performance Benchmark: Recursive Fibonacci (30)

Recursive functions stress any language's call stack and value system. Vyne's transpiler consistently outperforms the interpreter by nearly **3x** in recursion-heavy tasks.

| Execution Mode     | Time (ms)    | Speed Gain       |
| :----------------- | :----------- | :--------------- |
| AST Interpreter    | 54.52 ms     | 1.0x (Base)      |
| Compiled (GCC -O3) | **20.78 ms** | **~2.6x Faster** |

_Test Environment: i7-14700 · Windows 11 · Vyne Transpiler v0.9_

### Advanced Memory Management: The "Silent" Heap Frame

To prevent **Stack Overflow** during deep recursion, the transpiler uses:

1. **Heap-Based Call Frames**: Argument arrays allocated in the Vyne Arena, not the system stack
2. **Flattened Expression Trees**: Nested function calls (e.g., `f(g(x))`) are flattened into temporary variables during code generation
3. **Arena Block Allocation**: Memory managed in high-speed 8MB blocks where allocation is a simple pointer increment

---

## 📚 Language Reference

### Core Syntax

| Feature        | Syntax Example            | Description                                    |
| :------------- | :------------------------ | :--------------------------------------------- |
| **Arithmetic** | `(+, -, *, /, <, >, ==)`  | Standard mathematical and comparison operators |
| **Bitwise**    | `(&&, \|\|)`              | Low-level bit manipulation                     |
| **Functions**  | `fn calculate(x) { ... }` | Defined using the `fn` keyword                 |
| **Logic Flow** | `if cond { ... }`         | Standard conditional branching                 |
| **Loops**      | `while cond { ... }`      | Standard iteration                             |
| **Scoping**    | `group Graphics { ... }`  | Encapsulate logic into named namespaces        |
| **Modules**    | `module vcore`            | Interfaces with native C++ libraries           |

### Types

| Type      | Example                 | Notes            |
| :-------- | :---------------------- | :--------------- |
| `Int64`   | `age :: Int64 = 30;`    | 64‑bit signed    |
| `Float64` | `pi :: Float64 = 3.14;` | Double precision |
| `String`  | `"hello"`               | UTF‑8, immutable |
| `Array`   | `[1, "two", 3.0]`       | Heterogeneous    |
| `Map`     | `{ "key": 42 }`         | String keys only |
| `Bool`    | `true` / `false`        | Boolean logic    |

### Assignment Modes

| Mode         | Syntax Example               | Description                 |
| :----------- | :--------------------------- | :-------------------------- |
| **Inferred** | `score = 95`                 | Type determined at runtime  |
| **Explicit** | `age :: Int64 = 30`          | "Locked" to a specific type |
| **Constant** | `const PI :: Float64 = 3.14` | Immutable binding           |

### Control Flow

```vyne
// through with collect
doubled = through x :: [1,2,3] -> collect { x * 2 }; // [2,4,6]

// filter
evens = through n :: 0..10 -> filter { n % 2 == 0 };
```

### Structs & Interfaces

Vyne uses `interface` as both a struct definition and a constructor:

```vyne
interface Circle {
    r :: Float64;
    area() -> Float64 {
        return r * r * 3.14159;
    }
}

c = Circle(4.0); // fields fill positionally
out(c.area());   // => 50.26544
```

### Error Handling

```vyne
try {
    risky();
    throw "boom";
} catch (err) {
    out(err);
} finally {
    cleanup();
}
```

### Operators

| Operator | Name            | Example                  |
| :------- | :-------------- | :----------------------- |
| `..`     | Range           | `0..10`                  |
| `? :`    | Ternary         | `ready ? "go" : "wait"`  |
| `??`     | Null-coalesce   | `name ?? "anon"`         |
| `??=`    | Coalesce-assign | `cfg.mode ??= "default"` |
| `\|>`    | Pipeline        | `data \|> normalize`     |
| `in`     | Membership      | `x in [1, 2, 3]`         |
| `//`     | Floor divide    | `7 // 2`                 |
| `&`      | Reference param | `fn bump(v :: Int64&)`   |
| `$`      | Addresser       | `$target`                |

---

## 📦 Standard Library

### Global Functions

```vyne
out(x)         // Print to terminal
type(x)        // Returns "Float64", "String", "Array", or "Function"
sizeof(x)      // Get length of strings or count of array elements
string(x)      // Convert any data type to string
int64(x)       // Convert any data type to Int64
float64(x)     // Convert any data type to Float64
sequence(x, y) // Generates a sequence (array) in given range
free(x)        // Explicitly free memory (use with care)
exit(code)     // Exit the program
```

### Built-in Modules

| Module     | Description                                                                |
| :--------- | :------------------------------------------------------------------------- |
| **vcore**  | System-level utilities, I/O, sleep timers, process management              |
| **vglib**  | Hardware-accelerated 2D/3D graphics engine (Raylib-based)                  |
| **vaudio** | Professional-grade DSP: compressor, EQ, reverb, LUFS, BPM detection        |
| **vserv**  | HTTP/WebSocket server with Express-style routing, middleware, static files |
| **vmath**  | High-performance C++ math: trig, ML functions, logs, rounding              |
| **vmem**   | Memory introspection: usage tracking, address manipulation, peek/poke      |

### VCore

```vyne
vcore.input(prompt)   // Read line from stdin
vcore.now()           // ISO timestamp
vcore.sleep(ms)       // Block thread
vcore.memory_usage    // RSS bytes (property)
```

### VMath

```vyne
// Trig
sin, cos, tan, atan2

// ML
sigmoid, relu, clamp

// Log
sqrt, log, exp, pow

// Round
floor, ceil, round, abs
```

### VGLib — Graphics

VGLib is a full-featured 2D/3D framework built on top of **Raylib**:

```vyne
module vglib;

vglib.init(1280, 720, 60, "My Game", vglib.VSYNC);
while (vglib.running()) {
    vglib.begin();
    vglib.clear(vglib.rgba(20, 30, 40, 255));
    vglib.rect(100, 100, 200, 150, vglib.RED);
    vglib.end();
}
vglib.close();
```

**Key features:**

- 2D: lines, rectangles, circles, text, textures
- 3D: cameras, cubes, planes, models, billboards
- Shaders & post-processing (GLSL)
- Input: keyboard, mouse
- Map loading, collision detection, AI pathfinding
- Persistent groups for advanced instancing

### VAudio — DSP Engine

VAudio is a professional-grade audio processing engine with real-time DSP:

```vyne
module vaudio;

// Load and play a sound
sfx = vaudio.load_sound("explosion.wav");
vaudio.play_sound(sfx);

// Stream music with BPM detection
music = vaudio.play_stream("background.ogg");
vaudio.attach_bpm(music);
bpm = vaudio.get_bpm(music);

// Apply compressor
vaudio.attach_compressor(sfx);
vaudio.set_compressor(-12, 4, 15, 120, 2, true, true);

// 7-band EQ
vaudio.attach_eq(music);
vaudio.set_eq(2, 400, 4.5, 1.2); // Boost 400Hz

// Reverb
vaudio.attach_reverb(sfx);
vaudio.set_reverb(0.8, 0.4, 25, 0.5, true);
```

**Key features:**

- Sound loading (WAV, MP3, FLAC, OGG)
- Streaming audio with loop support
- Compressor: threshold, ratio, attack, release, makeup gain, auto-makeup
- 7-band parametric EQ: peaking, high-pass, low-pass, high-shelf, low-shelf
- Algorithmic FDN reverb: decay, mix, predelay, damping
- LUFS metering (ITU-R BS.1770)
- BPM detection (50–200 BPM)
- Spectrum analysis (64-bin FFT)
- Offline rendering: apply DSP chain to files and export as WAV
- 3D spatial audio with distance-based attenuation
- Saturation modes: Soft Tube, Hard Clip, Asymmetric, Tape, GameBoy

### VServ — Web Framework

VServ is a production-ready HTTP/WebSocket server with Express-style routing:

```vyne
module vserv;
module vcore;

// Express-style app
app = vserv.create_app();

// Logger middleware
fn logger(req, res, next) {
    vcore.out("[" + req.method + "] " + req.path);
    next();
};
vserv.app_use(app, logger);

// Routes
vserv.app_get(app, "/hello", fn(req, res) {
    vserv.send(res, "Hello, World!");
});

vserv.app_post(app, "/api/users", fn(req, res) {
    data = req.body;
    vserv.send_json(res, { "created": true, "data": data });
});

// Static files
vserv.app_get(app, "/static/*", fn(req, res) {
    res = vserv.serve_file(req.path);
    vserv.send(res);
});

vserv.app_listen(app, 3000);
```

**Key features:**

- GET, POST, PUT, DELETE routing
- Middleware chains
- Static file serving with automatic MIME type detection
- WebSocket upgrades
- Request/Response objects
- JSON response helper
- Status codes (2xx, 3xx, 4xx, 5xx)
- MIME type constants

---

## 🛡️ Rulesets

Vyne's `ruleset` system gives you fine-grained control over the engine's behavior:

```vyne
ruleset {
    // Type System
    strict_mode = true,
    type_check = "strict",
    dynamic_casting = false,
    implicit_conversion = "warn",

    // Memory
    memory_limit = 256 * 1024 * 1024, // 256MB
    memory_tracking = on,
    garbage_collection = on,

    // Performance
    optimization = 2, // 0-3
    profiling = on,
    jit = on,
    loop_unrolling = on,

    // Runtime
    debug = on,
    trace = off,
    stack_trace = on,
    recursion_limit = 1000,

    // Security
    safe_mode = off,
    sandbox = off,
    allow_vmem = on,
    allow_network = on,

    // Warnings
    warnings = "all",
    warnings_ignore = ["unused_variable", "shadow_variable"],

    // Experimental
    async = off,
    simd = on
};
```

### Available Rules

| Category         | Rule                  | Type            | Description                             |
| :--------------- | :-------------------- | :-------------- | :-------------------------------------- |
| **Type System**  | `strict_mode`         | boolean         | Enforce strict type checking            |
|                  | `type_check`          | string          | `strict` / `hybrid` / `dynamic`         |
|                  | `dynamic_casting`     | boolean         | Allow runtime type conversions          |
|                  | `implicit_conversion` | string          | `allow` / `warn` / `deny`               |
| **Memory**       | `memory_limit`        | integer (bytes) | Maximum memory usage                    |
|                  | `memory_tracking`     | boolean         | Track memory usage                      |
|                  | `garbage_collection`  | boolean         | Enable automatic GC                     |
| **Performance**  | `optimization`        | integer (0-3)   | Optimization level                      |
|                  | `profiling`           | boolean         | Collect execution metrics               |
|                  | `jit`                 | boolean         | Just-In-Time compilation                |
|                  | `loop_unrolling`      | boolean         | Loop unrolling optimization             |
| **Runtime**      | `debug`               | boolean         | Additional runtime checks               |
|                  | `trace`               | boolean         | Execution tracing                       |
|                  | `stack_trace`         | boolean         | Include stack traces in errors          |
|                  | `recursion_limit`     | integer         | Maximum recursion depth                 |
| **Security**     | `safe_mode`           | boolean         | Restrict dangerous operations           |
|                  | `sandbox`             | boolean         | Isolate execution environment           |
|                  | `allow_vmem`          | boolean         | Allow direct memory access              |
|                  | `allow_network`       | boolean         | Allow network operations                |
| **Warnings**     | `warnings`            | string          | `all` / `none` / `error_only`           |
|                  | `warnings_ignore`     | array           | List of warning types to ignore         |
| **Experimental** | `async`               | boolean         | Asynchronous execution                  |
|                  | `simd`                | boolean         | SIMD vectorization                      |
|                  | `max_iterations`      | integer         | Maximum loop iterations (0 = unlimited) |

### Preset Profiles

| Profile         | Configuration                                                               |
| :-------------- | :-------------------------------------------------------------------------- |
| **Performance** | `optimization = 3`, `jit = on`, `profiling = off`, `loop_unrolling = on`    |
| **Debug**       | `debug = on`, `trace = on`, `stack_trace = on`, `memory_tracking = on`      |
| **Secure**      | `safe_mode = on`, `sandbox = on`, `allow_vmem = off`, `allow_network = off` |

---

## 🧪 Example Projects

### Game with VGLib

```vyne
module vglib;

// Setup
vglib.init(1280, 720, 60, "Cube Spinner", vglib.VSYNC);
cam = vglib.camera(45.0);

// Main loop
while (vglib.running()) {
    vglib.begin();
    vglib.clear(vglib.rgba(20, 30, 40, 255));

    vglib.begin3d(cam);
    vglib.cube(0, 0, 0, 2.0, time * 50, vglib.CYAN);
    vglib.end3d();

    vglib.end();
    time = time + 0.01;
}
vglib.close();
```

### Web Server with VServ

```vyne
module vserv;
module vcore;

app = vserv.create_app();

vserv.app_get(app, "/", fn(req, res) {
    vserv.send(res, "<h1>Hello from Vyne!</h1>");
});

vserv.app_get(app, "/api/time", fn(req, res) {
    vserv.send_json(res, { "time": vcore.now() });
});

vserv.app_listen(app, 8080);
```

### Audio Processor with VAudio

```vyne
module vaudio;

// Load and process audio
sfx = vaudio.load_sound("input.wav");

// Apply compressor and EQ
vaudio.attach_compressor(sfx);
vaudio.set_compressor(-12, 4, 15, 120, 2, true, true);

vaudio.attach_eq(sfx);
vaudio.set_eq(2, 400, 4.5, 1.2);
vaudio.set_eq(4, 2500, 3.0, 1.5);

vaudio.attach_reverb(sfx);
vaudio.set_reverb(0.6, 0.3, 20, 0.4, true);

// Render offline
vaudio.render_offline("input.wav", "output_mastered.wav");
```

---

## 📘 Documentation

Vyne's engine architecture is fully documented using **Doxygen**.

### Generate Documentation

```bash
doxygen Doxyfile
```

Then open `vyne-docs/html/index.html` in your browser.

### Navigating the Docs

- **AST Hierarchy**: View how every language feature inherits from `ASTNode`
- **Collaboration Diagrams**: See class dependencies and interactions
- **Function Call Graphs**: Trace how `evaluate()` methods process complex scripts
- **Native Module Bindings**: Explore C++ implementations of `vglib`, `vaudio`, and `vserv`

---

## 🔧 Development

### Build from Source

```bash
git clone https://github.com/tuncaygafarli/vyne.git
cd vyne
make
```

### Run Tests

```bash
make test
```

### Project Structure

```
vyne/
├── cli/                          # Command-line interface tools
│   ├── file_handler.cpp/h        # File I/O operations
│   ├── packager.cpp/h            # Module packaging
│   └── repl.cpp/h                # Read-Eval-Print Loop
│
├── compiler/                     # Core compiler (source)
│   ├── ast/                      # Abstract Syntax Tree
│   │   ├── ast.cpp/h             # AST node definitions
│   │   ├── ast_helpers.h         # Symbol lookup & type helpers
│   │   └── value.cpp/h           # Value system (tagged union)
│   ├── codegen/                  # C transpiler
│   │   ├── codegen.cpp/h         # AST → C code generation
│   │   └── emitter.h             # C emitter with context management
│   ├── lexer/                    # Tokenizer
│   │   └── lexer.cpp/h           # Lexical analysis
│   ├── parser/                   # Recursive-descent parser
│   │   └── parser.cpp/h          # Parser with type resolution
│   └── types.h                   # VType enum definitions
│
├── docs/                         # Documentation website
│   ├── assets/                   # Images and logos
│   ├── theme/                    # Documentation theme
│   ├── tutorials/                # Tutorial markdown files
│   ├── core-concepts.html        # Core concepts guide
│   ├── index.html                # Main docs page
│   ├── index.js                  # Documentation scripts
│   └── style.css                 # Documentation styles
│
├── editors/                      # Editor integrations
│   ├── nvim/                     # Neovim plugin
│   │   ├── ftdetect/vyne.lua     # File type detection
│   │   ├── lua/vyne_lsp.lua      # LSP client
│   │   └── syntax/vyne.vim       # Syntax highlighting
│   └── vscode/                   # VS Code extension
│       └── lsp/                  # Language Server Protocol
│           ├── backend/          # LSP server (C++)
│           │   ├── include/      # Headers
│           │   └── src/          # LSP server implementation
│           ├── client/           # VS Code extension
│           │   ├── src/          # Extension source
│           │   └── syntaxes/     # TM Language grammar
│           ├── server/           # Node.js LSP server
│           └── samples/          # Example Vyne files
│
├── models/                       # Pre-trained ML models (VML)
│   ├── crypto_deep_v1/           # Cryptocurrency prediction
│   ├── crypto_market_v1/         # Market analysis
│   ├── metu_grader_v1/           # Grade prediction
│   └── xor_trained_v1/           # XOR logic gate
│
├── modules/                      # Runtime modules (source)
│   ├── common/                   # Core modules
│   │   ├── vaudio/               # Audio DSP engine
│   │   │   ├── docs/             # DSP documentation
│   │   │   │   ├── compressor.md
│   │   │   │   ├── equalizer.md
│   │   │   │   └── reverb.md
│   │   │   ├── dsp/              # DSP implementation
│   │   │   │   ├── analyzer.h
│   │   │   │   ├── compressor.h
│   │   │   │   ├── dsp_utils.h
│   │   │   │   ├── equalizer.h
│   │   │   │   ├── lufs.h
│   │   │   │   ├── reverb.h
│   │   │   │   ├── saturator.h
│   │   │   │   └── shared_state.h
│   │   │   ├── vaudio.cpp/h      # VAudio bindings
│   │   │   └── vaudio.h
│   │   ├── vcore/                # System utilities
│   │   │   └── vcore.cpp/h
│   │   ├── vcv/                  # Computer vision
│   │   │   └── vcv.cpp/h
│   │   ├── vfs/                  # Filesystem operations
│   │   │   └── vfs.cpp/h
│   │   ├── vglib/                # Graphics engine (Raylib)
│   │   │   ├── vglib.cpp/h       # Main bindings
│   │   │   ├── vglib_camera.cpp  # 3D camera
│   │   │   ├── vglib_common.h    # Shared definitions
│   │   │   ├── vglib_core.cpp    # Window & frame loop
│   │   │   ├── vglib_input.cpp   # Keyboard & mouse
│   │   │   ├── vglib_render2d.cpp # 2D rendering
│   │   │   ├── vglib_render3d.cpp # 3D rendering
│   │   │   └── vglib_shaders.cpp # Shaders & post-processing
│   │   ├── vmath/                # High-performance math
│   │   │   └── vmath.cpp/h
│   │   ├── vmem/                 # Memory introspection
│   │   │   └── vmem.cpp/h
│   │   ├── vml/                  # Machine Learning
│   │   │   ├── vml.cpp/h         # Main bindings
│   │   │   ├── vml_common.h      # Shared definitions
│   │   │   ├── vml_activation.cpp # Activation functions
│   │   │   ├── vml_layers.cpp    # Neural network layers
│   │   │   ├── vml_loss.cpp      # Loss functions
│   │   │   ├── vml_model.cpp     # Model management
│   │   │   ├── vml_ops.cpp       # Tensor operations
│   │   │   ├── vml_optimizer.cpp # Optimizers (SGD, Adam)
│   │   │   └── vml_tensor.cpp    # Tensor implementation
│   │   ├── vnet/                 # Networking (raw sockets)
│   │   │   └── vnet.cpp/h
│   │   ├── vserv/                # HTTP/WebSocket server
│   │   │   ├── vserv.h           # Main definitions
│   │   │   ├── vserv_common.h    # Shared helpers
│   │   │   ├── vserv_request.cpp # Request parsing
│   │   │   ├── vserv_server.cpp  # Server core
│   │   │   ├── vserv_setup.cpp   # Module registration
│   │   │   ├── vserv_static.cpp  # Static file serving
│   │   │   ├── vserv_web.cpp     # Express-style web framework
│   │   │   └── vserv_ws.cpp      # WebSocket support
│   │   └── vurage/               # Database engine (Urage)
│   │       └── vurage.cpp/h
│   └── external/                 # External module libraries
│       ├── vcolors.vy            # Color utilities
│       ├── vconvert.vy           # Data conversion
│       ├── vjson.vy              # JSON parsing
│       ├── vlinalg.vy            # Linear algebra
│       ├── vml.vy                # ML wrapper
│       ├── vplot.vy              # Plotting
│       └── vstring.vy            # String utilities
│
├── runtime/                      # Runtime system
│   ├── diagnostics.h             # Error & warning reporting
│   ├── vyne_runtime.h            # Core runtime API
│   └── modules/                  # Runtime module headers
│       ├── vcore.h
│       └── vmath.h
│
├── scripts/                      # Build & utility scripts
│   ├── build_path.sh
│   ├── run_all_tests.sh
│   └── test_ast.sh
│
├── tests/                        # Test suite
│   ├── assets/                   # Test assets (audio, images, models)
│   ├── compiler/                 # Compiler tests
│   ├── dsp/                      # Audio DSP tests
│   │   ├── configs/              # DSP test configurations
│   │   └── *.vy                  # DSP test scripts
│   ├── graphics/                 # Graphics tests
│   │   ├── shaders/              # Test shaders
│   │   └── *.vy                  # Graphics test scripts
│   ├── network/                  # Networking tests
│   │   ├── vdnet/                # vnet tests
│   │   └── vserv/                # VServ tests
│   ├── training/                 # ML training tests
│   │   └── vnet/                 # vnet training tests
│   └── *.vy                      # Language feature tests
│
├── utils/                        # Utilities
│   ├── file_utils.h              # File I/O helpers
│   └── sha256.h                  # SHA-256 hashing
│
├── vendor/                       # Third-party dependencies
│   ├── raylib/                   # Raylib (graphics/audio)
│   │   ├── include/              # Raylib headers
│   │   └── lib/                  # Raylib static libraries
│   ├── stb/                      # STB libraries (image, font, etc.)
│   │   ├── stb_image.h
│   │   ├── stb_image_write.h
│   │   ├── stb_truetype.h
│   │   └── ... (other STB headers)
│   └── urage/                    # Urage embedded database
│       ├── bindings/cpp/         # C++ bindings
│       └── core/                 # Urage core
│           ├── include/          # Urage headers
│           └── src/              # Urage source
│
├── build/                        # Compiled object files
│   ├── cli/
│   ├── editors/vscode/lsp/backend/src/
│   └── vyne/
│       ├── compiler/
│       │   ├── ast/
│       │   ├── codegen/
│       │   ├── lexer/
│       │   └── parser/
│       └── modules/common/
│           ├── vaudio/
│           ├── vcore/
│           ├── vcv/
│           ├── vfs/
│           ├── vglib/
│           ├── vmath/
│           ├── vmem/
│           ├── vml/
│           ├── vnet/
│           ├── vserv/
│           └── vurage/
│
├── .github/                      # GitHub Actions & templates
│   ├── ISSUE_TEMPLATE/
│   └── workflows/
│
├── .vscode/                      # VS Code project settings
├── docs/                         # User documentation (HTML)
├── main.cpp                      # Entry point
├── Makefile                      # Build system
├── Doxyfile                      # Doxygen configuration
├── LICENSE                       # MIT License
├── README.md                     # This file
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── SECURITY.md
├── build.bat                     # Windows build script
├── build.sh                      # Linux/Mac build script
└── vynec.exe                     # Vyne compiler executable
```

---

## 🤝 Contributing

Contributions are welcome! Here's how to get started:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Commit your changes**: `git commit -m 'Add amazing feature'`
4. **Push to the branch**: `git push origin feature/amazing-feature`
5. **Open a Pull Request**

### Guidelines

- Follow the existing code style
- Add tests for new features
- Update documentation accordingly
- Ensure all tests pass: `make test`

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.

---

## 🙏 Acknowledgments

- **Raylib** for graphics and audio primitives
- **OpenSSL** for WebSocket SHA1 support
- All contributors and users of Vyne

---

_Built with ❤️ by [Tuncay Qafarlı](https://github.com/tuncaygafarli)_
