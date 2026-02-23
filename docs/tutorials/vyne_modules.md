# 📦 Vyne Modules
## ⚙️ Module System Overview

---

## 📑 Table of Contents

1. Introduction  
2. Module Categories  
3. Common Modules  
4. External Modules  
5. Using Deployed Modules  
6. Empty Modules  
7. Examples  

---

## 🚀 Introduction

In Vyne, modules are used to organize code, encapsulate functionality, and enable reuse across multiple files.

Modules are divided into two main categories:

- 🧩 Common modules  
- 🌍 External modules  

---

## 📦 Module Categories

### 🔧 1. Common Modules

Common modules are usually written in **C++** and compiled together with the main Vyne compiler codebase.

They are invoked inside Vyne source code using the `module` keyword.

> ⚠️ Note:  
> The `module` keyword can also create empty modules.

---

### 🌍 2. External Modules

External modules are written directly in Vyne.  

They are usually defined as empty modules initially. During development, the following elements are added:

- 🔹 Functions  
- 🔹 Variables  
- 🔹 Data types  

Finally, they are **deployed** for use in other files.

---

## 🧪 External Module Example

**File:** `vmath.vy`

```vyne
module vmath;

fn :: vmath add(a, b) { return a + b; }

deploy vmath;
```

The `deploy` keyword publishes the module so it can be imported into other scripts.

---

## 📥 Using Deployed Modules

To use a deployed module:

```vyne
use extern "vmath.vy";

vmath.add(3.5);
```

| Keyword | Description |
|---------|-------------|
| `use` | Imports the module into the current file |
| `extern` | Specifies that the module is in the external modules directory |

> 🔎 Note:  
> If `extern` is omitted, Vyne searches for the file in the same directory as the current script.

---

## 🧱 Empty Modules

Vyne has four built-in modules:

| Module | Functionality |
|--------|---------------|
| `vcore` | System-level utilities |
| `vmem` | Memory management |
| `vglib` | Graphics & buffer management |
| `vmath` | Built-in mathematical functions |

If you define a module using one of these names, built-in functionality is automatically loaded.

### ✅ Behavior Example

```vyne
module vmath;
```
✔ Loads built-in mathematical functions automatically

```vyne
module customModule;
```
✔ Creates an empty module

> ⚠️ Note:  
> Any module name not matching a built-in module will produce a blank module.

---

## 🔍 Summary

- Modules help organize and reuse code  
- Common modules are compiled C++ extensions  
- External modules are written in Vyne and deployed  
- Built-in modules provide core functionality automatically  
- Empty modules can be created for custom functionality  