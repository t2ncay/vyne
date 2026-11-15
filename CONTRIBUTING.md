# Contributing to Vyne 🌿

Thank you for your interest in contributing to the Vyne Interpreter! We aim for a high-performance, modular, and readable codebase.

---

## 🏗 Architecture Principles

To keep the "Vyne" from getting tangled, please follow these guidelines:

- **Recursive Descent:** Every grammar rule should have its own "worker" function (e.g., `parseWhileLoop`). Avoid massive `if-else` chains.
- **Move Semantics:** Use `std::move` when passing large objects like `std::vector<Token>` or `std::unique_ptr<ASTNode>` to prevent expensive copies.
- **Memory Safety:** Strictly use smart pointers (`std::unique_ptr`, `std::shared_ptr`). No raw `new` or `delete`.

---

## 🚀 Getting Started

### 1. Prerequisites

Use a C++23-capable toolchain and xmake for the compiler. Registered regression
tests require Python 3.11 or newer; target-specific suites may need additional tools.

### 2. Build Instructions

```bash
git clone https://github.com/t2ncay/vyne.git
cd vyne
xmake
xmake test
```

---

## 🧪 Development Workflow

### Coding Style

- **Classes:** `PascalCase` (e.g., `FunctionNode`)
- **Methods/Variables:** `camelCase` (e.g., `parseStatement`)
- **Files:** Follow the existing directory structure (`/lexer`, `/parser`, `/ast`).

### Commit Messages

We follow the Conventional Commits specification. This helps us generate clean changelogs automatically.

- `feat:` A new feature for the user.
- `fix:` A bug fix.
- `refactor:` A code change that neither fixes a bug nor adds a feature (e.g., chunking functions).
- `perf:` A code change that improves performance (e.g., adding `std::move`).

### Pull Requests

1. Fork the repo and create your branch from `master`.
2. Ensure your code compiles without warnings (we use `-Wall -Wextra -Wpedantics`).
3. Add a meaningful fixture under `examples/transpiler/`, `examples/test/`, or
   `examples/optimization/` and register its compile/runtime/output contract in
   `scripts/tests/suites.toml`. Run `xmake test` (core) or `xmake test-all` (all
   registered suites). See `scripts/tests/README.md`; empty or unregistered
   discovery is an error, not a passing test run.
4. Open a PR with a clear description of your changes.

---

## ⚖️ License

By contributing to Vyne, you agree that your contributions will be licensed under the project's MIT License.
