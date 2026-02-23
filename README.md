# 🌿 Vyne

[![Vyne CI](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml)
[![pages-build-deployment](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment)

> **DISCLAIMER:** This repository is currently under active maintenance. This README serves as a live technical manifest and personal scratchpad for engine development.

## 🚀 Feature Set & Roadmap

Vyne is currently in its early stages but already supports a robust set of core programming constructs, specialized for terminal-based logic and ASCII manipulation.

---

### 🔢 Core Language Syntax

| Feature        | Syntax Example            | Description                                                |
| :------------- | :------------------------ | :--------------------------------------------------------- |
| **Arithmetic** | `(+, -, *, /, <, >, ==)`  | Standard mathematical and comparison operators.            |
| **Bitwise**    | `(&&, \|\|)`              | Low-level bit manipulation for flags and binary data.      |
| **Functions**  | `fn calculate(x) { ... }` | Defined using the `fn` keyword with scoped arguments.      |
| **Logic Flow** | `if cond { ... }`         | Standard conditional branching.                            |
| **Loops**      | `while cond { ... }`      | Standard iteration for repeated execution logic.           |
| **Scoping**    | `group Graphics { ... }`  | Encapsulate logic and variables into named namespaces.     |
| **Modules**    | `module vcore`            | Interfaces with native C++ libraries and system resources. |

### 🛡️ Typing & Assignment Rules

Vyne employs a hybrid type system that supports both **Explicit Declaration** and **Inferred Typing**. This allows for flexible scripting while maintaining the safety required for complex logic.

---

#### 1. Assignment Modes

| Mode         | Syntax Example               | Description                                                                 |
| :----------- | :--------------------------- | :-------------------------------------------------------------------------- |
| **Inferred** | `score = 95`                 | Type is determined at runtime based on the assigned value.                  |
| **Explicit** | `age :: Int64 = 30`          | The variable is "locked" to a specific type; future assignments must match. |
| **Constant** | `const PI :: Float64 = 3.14` | Immutable binding. Reassignment attempts will trigger a Runtime Error.      |

#### 2. Built-in Primitive Types

Vyne recognizes the following core types during explicit declaration:

- **`Int64`**: Signed 64-bit integer. Used for indexing, pointers, and discrete counts.
- **`Float64`**: 64-bit double-precision floating point. Used for ML, DSP, and physics.
- **`String`**: UTF-8 encoded character sequences.
- **`Boolean`**: Logical `true` or `false`.
- **`Array`**: Dynamic list of `Value` objects.

#### 3. Safety Constraints

To ensure engine stability, the following rules are enforced:

> [ Note: Type Mismatch ]
> If a variable is declared as `val :: Int64`, assigning a `String` to it later will result in a `Type Error`.

> [ Note: Constant Protection ]
> Constants must be initialized at the moment of declaration. Once set, they are read-only for the duration of the program execution.

---

### 📦 Built-in Modules

Vyne leverages native C++ modules to handle high-performance tasks that the interpreter shouldn't do alone.

- 📡 **[vcore](https://github.com/tuncaygafarli/vyne/tree/master/vyne/modules/vcore)** System-level utilities, sleep timers, and process management.
- 🎨 **[vglib](https://github.com/tuncaygafarli/vyne/tree/master/vyne/modules/vglib)** The "Vyne Graphics Library" — home to the 3D ASCII donut and buffer management (in the future).
- 🧠 **[vmem](https://github.com/tuncaygafarli/vyne/tree/master/vyne/modules/vmem)** Memory management and introspection — track heap usage, inspect raw memory addresses, and monitor variable footprints.
- 🧪 **[vmath](https://github.com/t2ncay/vyne/tree/master/vyne/modules/vmath)** A comprehensive wrapper for the C++ standard math library, featuring trigonometric functions, hyperbolic operations, and mathematical constants like $\pi$ and $\phi$.

---

### 📚 Standard Library & Arrays

#### Global Functions

```bash
out(x)         # Print to terminal
type(x)        # Returns "Float64", "String", "Array", or "Function"
sizeof(x)      # Get length of strings or count of array elements
string(x)      # Convert any data type to string
int64(x)       # Convert any data type to Int64
float64(x)     # Convert any data type to Float64
sequence(x, y) # Generates a sequence ( array ) in given range of numbers
```

#### Array Methods

Arrays in Vyne are dynamic and come with built-in methods for data manipulation:

- `arr.push(val)` / `arr.pop()` — Stack operations.
- `arr.delete(val)` — Remove specific elements.
- `arr.sort()` — In-place numeric sorting.
- `arr.reverse()` — Flip array order.
- `arr.place_all(val, count)` — Bulk initialize an array.
- `arr.clear()` — Wipe all data from the instance.

---

### 🛠 Installation & Setup

To build the interpreter from source, clone the repository and compile using your preferred C++ compiler:

```bash
git clone https://github.com/tuncaygafarli/vyne.git
cd vyne

./build.bat # For Windows
./build.sh  # For Linux
```

## 📘 Documentation Walkthrough

Vyne's engine architecture is fully documented using **Doxygen**. This allows you to explore the interpreter's internals through a searchable web interface, complete with class diagrams and function call graphs.

### 🛠 Generating the Documentation

To build the documentation locally, ensure you have [Doxygen](https://www.doxygen.nl/) and [Graphviz](https://graphviz.org/) installed, then run:

```bash
doxygen Doxyfile
```

Once the process finishes, open the following file in your browser:
`vyne-docs/html/index.html`

### 🔍 Navigating the Engine

The documentation provides several powerful ways to understand how Vyne works:

- **Abstract Syntax Tree (AST) Hierarchy**:
  Navigate to `Classes -> Class Hierarchy`. This visualizes how every language feature (like `WhileNode`, `BinOpNode`, or `FunctionNode`) inherits from the base `ASTNode`.

- **Collaboration Diagrams**:
  Each class page features a diagram showing which other objects it depends on. For example, you can see how an `AssignmentNode` interacts with the `SymbolContainer`.

- **Function Call Graphs**:
  Every `evaluate()` method includes a flowchart showing which sub-functions are called during execution. This is extremely helpful for tracing how the interpreter processes complex Vyne scripts.

- **Native Module Bindings**:
  Explore the `modules` namespace to see the C++ implementation of `vglib` (the donut renderer) and `vcore`. You can view the raw C++ math directly alongside the documentation.

### 🏗 Project Structure

- **`vyne/compiler`**: The Lexer and Parser that turn source code into an AST.
- **`vyne/core`**: The main execution engine and the `Value` system.
- **`vyne/modules`**: Native C++ extensions that provide high-performance features to the language.
