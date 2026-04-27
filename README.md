# 🌿 Vyne

[![Vyne CI](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml)
[![pages-build-deployment](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment)

> **DISCLAIMER:** This repository is currently under active maintenance. This README serves as a live technical manifest and personal scratchpad for engine development.

## 🚀 Feature Set & Roadmap

Vyne is currently in its early stages but already supports a robust set of core programming constructs, specialized for terminal-based logic and ASCII manipulation.

---

## ⚙️ The Transpilation Engine: C as High-Level Assembly

Vyne features a powerful **C-Transpiler** that bridges the gap between high-level expressive syntax and low-level machine performance. Instead of compiling to a heavy bytecode or relying solely on an interpreter, Vyne generates human-readable, optimized C99 code.

### 🏗️ How it Works: From AST to Binary

The transformation process follows a strict pipeline to ensure that the semantic meaning of Vyne logic is preserved while maximizing execution speed:

1. **AST Flattening**: Complex, nested expressions are decomposed into a linear sequence of C statements. This prevents stack-depth issues and allows the C compiler to better optimize register usage.
2. **Mangled Namespacing**: To support Vyne's `group` and `interface` structures in a flat C namespace, the engine uses a deterministic mangling scheme (e.g., `Master.Element.getName()` becomes `fn_Master_Element_getName`).
3. **Implicit Header Injection**: The transpiler automatically links the source with `vyne_runtime.h`, a lightweight header providing the core `Value` system, Arena memory management, and built-in math/graphics operations.

### 💎 Why Transpile to C?

- **Zero Overhead Portability**: Any system with a C compiler (GCC, Clang, MSVC) can run Vyne code.
- **Aggressive Optimization**: By transpiling to C, Vyne inherits decades of optimization research embedded in modern C compilers (like loop unrolling and vectorization).
- **Embedded Friendly**: The resulting binaries are extremely small (starting at 50 KB), making Vyne suitable for resource-constrained environments or as an embedded logic engine for larger C++ projects.

### 🚀 Usage

To generate and compile the C source in one command:

```bash

# This generates script.vy.c and compiles it to script.exe
vynec --compile ./tests/logic_test.vy
```

#### 📊 Performance Benchmark: Recursive Fibonacci (30)

Recursive functions are a stress test for any language's call stack and value system. Vyne’s transpiler consistently outperforms its interpreter by nearly 3x in recursion-heavy tasks.

| Execution Mode         | Time (ms)    | Speed Gain       |
| :--------------------- | :----------- | :--------------- |
| **AST Interpreter**    | 54.52 ms     | 1.0x (Base)      |
| **Compiled (GCC -O3)** | **20.78 ms** | **~2.6x Faster** |

##### Test Environment: i7-14700 | Windows 11 | Vyne Transpiler v0.9

#### 🧠 Advanced Memory Management: The "Silent" Heap Frame

To prevent **Stack Overflow** during deep recursion (like Fibonacci 30+), Vyne's transpiler employs a specialized memory strategy:

1. **Heap-Based Call Frames**: Unlike standard C which uses the limited system stack for function arguments, Vyne's transpiler allocates argument arrays inside the **Vyne Arena**.
2. **Flattened Expression Trees**: Nested function calls (e.g., `f(g(x))`) are automatically flattened into temporary variables during code generation. This ensures that the C stack only handles function return addresses, while all heavy data resides in the heap.
3. **Arena Block Allocation**: Memory is managed in high-speed 8MB blocks, where allocation is a simple pointer increment, ensuring zero overhead during recursive calls.

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
- 🎨 **[vglib](https://github.com/tuncaygafarli/vyne/tree/master/vyne/modules/vglib)** Vyne’s high-performance Graphics & Audio Engine. Features hardware-accelerated 3D rendering (Z-buffer), spatial audio pipelines, and native hardware input mapping.
- 🧠 **[vmem](https://github.com/tuncaygafarli/vyne/tree/master/vyne/modules/vmem)** Memory management and introspection — track heap usage, inspect raw memory addresses, and monitor variable footprints.
- 🧪 **[vmath](https://github.com/t2ncay/vyne/tree/master/vyne/modules/vmath)** A comprehensive wrapper for the C++ standard math library, featuring trigonometric functions, hyperbolic operations, and mathematical constants like $\pi$ and $\phi$.

---

### 🏛 Structs & Object-Oriented Interfaces

Vyne uses `interface` definitions to create structured data types. Unlike traditional interfaces, Vyne interfaces act as **Constructors** and can contain **Methods** with access to the instance via the `self` keyword.

#### 1. Interface Anatomy

| Component       | Syntax Example               | Description                                                                 |
| :-------------- | :--------------------------- | :-------------------------------------------------------------------------- |
| **Fields**      | `row :: Int64`               | Explicitly typed data members.                                              |
| **Methods**     | `magnitude() { ... }`        | Functions defined inside the interface scope.                               |
| **Self-Ref**    | `self.x`                     | Accesses the current instance's fields or other methods.                    |
| **Namespacing** | `group Types :: mod { ... }` | Interfaces can be nested inside groups for strict organizational hierarchy. |

#### 2. Usage & Implementation

Interfaces are instantiated using the type name as a constructor. Methods are invoked using dot notation.

```vyne
use extern "vlinalg.vy";
```

##### 1. Instantiation (Constructor Mode)

Parameters are mapped positionally to the interface fields

```vyne
pos :: vlinalg.Types.Vector = vlinalg.Types.Vector(10, 20);
```

##### 2. Method Invocation

Methods have internal access to fields via 'self'

```vyne
m = pos.magnitude();
```

##### 3. Type-Safe Method Arguments

Methods can accept other instances as typed parameters

```vyne
other_pos = vlinalg.Types.Vector(5, 5);
cp = pos.cross_product(other_pos);
```

#### 3. Field & Method Introspection

Every struct instance in Vyne supports built-in reflection to assist with debugging and dynamic logic:

- **`obj.fields()`**: Returns an `Array` of strings containing all defined field names.
- **`out(obj)`**: Native string representation showing the internal state: `Vector { x: 10, y: 20 }`.
- **Member Assignment**: Supports direct updates to fields, e.g., `pos.x = 50`.

---

### 🏗 Engine Architecture (Technical Memo)

- **Constructor Hook**: When the `FunctionCallNode` identifies a target name residing in the `InterfaceTable` rather than the `FunctionTable`, it triggers an automatic allocation of a `VyneInstance`.
- **Symbol Mapping**: Arguments passed to the constructor are mapped positionally to the fields defined in the `InterfaceNode`.
- **`self` Binding**: During a method call, the interpreter injects a `self` symbol into the local `SymbolContainer` scope, pointing back to the caller's memory address.
- **Recursive Resolution**: The parser and interpreter support deep pathing for types (e.g., `Module.Group.Interface`) to ensure clear encapsulation.

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

make
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
