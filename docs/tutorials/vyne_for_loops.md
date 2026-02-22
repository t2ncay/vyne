# 🔁 Vyne Programming Language

## ⚙️ Control Structures – For Loop System

---

## 📑 Table of Contents

1. Introduction
2. Iterable Sources
3. Loop Modes
4. Default Iterator Behavior
5. Examples
6. Design Philosophy
7. Summary

---

## 🚀 Introduction

In the Vyne Programming Language, iteration is implemented using the `through` keyword.

Unlike traditional programming languages that use classical `for` syntax, Vyne introduces a functional-style iteration system that combines:

- Declarative iteration
- Built-in transformation modes
- Simplified syntax
- Default iterator handling

### 🧩 General Structure

```vyne
through iterable -> MODE {
    statements;
}
```

---

## 📦 Iterable Sources

Vyne allows iteration over two types of sources:

---

### 🔢 1. Sequence Generator

Numeric ranges can be generated in two ways:

```vyne
through 1..30
through sequence(1,30)
```

#### 📘 Sequence Operator

| Expression       | Description                                |
| ---------------- | ------------------------------------------ |
| `1..30`          | Generates numbers from 1 to 30 (inclusive) |
| `sequence(1,30)` | Functional form of numeric sequence        |

The `..` operator internally creates a temporary iterable numeric sequence.

---

### 📚 2. Array Iteration

Existing arrays can also be iterated.

```vyne
x :: Array = [1,2,3];
through x
```

| Component   | Meaning                              |
| ----------- | ------------------------------------ |
| `x`         | Array variable                       |
| `through x` | Iterates through all elements of `x` |

---

## 🔄 Loop Modes

After defining the iterable, Vyne requires a loop mode using:

```vyne
-> MODE
```

Currently supported loop modes:

| Mode      | Purpose                           | Functional Equivalent |
| --------- | --------------------------------- | --------------------- |
| `LOOP`    | Standard iteration                | Classic for-loop      |
| `COLLECT` | Transform elements into new array | map()                 |
| `FILTER`  | Filter elements into new array    | filter()              |
| `UNIQUE`  | Remove duplicates and sort        | set + sort            |

---

## 🎯 Default Iterator Behavior

By default, Vyne uses `_` as the iterator variable.

| Symbol | Description                    |
| ------ | ------------------------------ |
| `_`    | Represents the current element |

Example:

```vyne
through x -> loop {
    out(_)
}
```

---

### 🔤 Renaming the Iterator

The iterator can be renamed using `::` syntax:

```vyne
through num::x -> loop {
    out(num)
}
```

| Syntax   | Meaning                                           |
| -------- | ------------------------------------------------- |
| `num::x` | Iterates over `x` assigning each element to `num` |

---

## 🧪 Examples

---

### 🔁 LOOP Mode Example

```vyne
x :: Array = [1,2,3];

through x -> loop {
    out(_)
}
```

#### 📤 Output

```
1
2
3
```

Execution Flow:

| Step | Value of `_` |
| ---- | ------------ |
| 1    | 1            |
| 2    | 2            |
| 3    | 3            |

---

### 🔢 Range Iteration Example

```vyne
through 1..5 -> loop {
    out(_)
}
```

#### 📤 Output

```
1
2
3
4
5
```

---

### 🧮 COLLECT Mode Example

```vyne
x :: Array = [1,2,3,4,5,6];

y :: Array = through num::x -> collect {
    num * 2;
};
```

#### 📤 Result

```
[2,4,6,8,10,12]
```

Execution Behavior:

| Stage          | Action                         |
| -------------- | ------------------------------ |
| Iteration      | Each element assigned to `num` |
| Transformation | `num * 2` applied              |
| Return         | New transformed array          |

---

### 🔍 FILTER Mode Example

```vyne
x :: Array = [1,2,3,4,5,6];

y :: Array = through num::x -> filter {
    num % 2 == 0;
};
```

#### 📤 Result

```
[2,4,6]
```

Execution Behavior:

| Stage     | Action                         |
| --------- | ------------------------------ |
| Iteration | Each element assigned to `num` |
| Condition | `num % 2 == 0` evaluated       |
| Selection | Only even numbers returned     |

---

### 🧹 UNIQUE Mode Example

```vyne
x :: Array = [1,2,2,3,3,4];

y :: Array = through x -> unique {};
```

#### 📤 Result

```
[1,2,3,4]
```

| Feature            | Supported |
| ------------------ | --------- |
| Removes duplicates | Yes       |
| Sorts result       | Yes       |
| Returns new array  | Yes       |

---

## 🧠 Design Philosophy

The Vyne loop system is designed to:

- Reduce syntactic complexity
- Combine iteration and transformation
- Encourage functional-style programming
- Improve readability
- Minimize boilerplate code

By integrating transformation modes directly into the loop structure, Vyne eliminates the need for separate mapping and filtering functions.

---

## ✅ Summary

The `through` loop mechanism in Vyne provides:

- Flexible iterable sources
- Built-in transformation modes
- Default iterator simplification
- Functional programming integration
- Clean and expressive syntax

Vyne’s iteration model merges traditional control flow with modern declarative programming concepts, offering both power and simplicity.
