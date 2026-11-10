---
id: builtins
title: Built-ins & Collections
group: Language Reference
icon: fa-bolt
order: 12
description: Global functions, array and map methods, string methods, and the drop intrinsic.
---

# Built-ins & Collections

## Global built-ins

| Name | Behaviour |
| :--- | :--- |
| `out(value, ...)` | Print each argument followed by a newline. Concatenate with `+` for one line. |
| `string(x)` | Convert any value to `String`; no argument gives `""`. |
| `int64(x)` | Convert to `Int64`. |
| `float64(x)` | Convert to `Float64`. |
| `float32(x)` | Convert to `Float32`. |
| `type(x)` | Return the runtime type name: `"Int64"`, `"Float64"`, `"Float32"`, `"String"`, `"Array"`, `"Map"`, ... |
| `sizeof(x)` | Length of a string or array (runtime-reported size). |
| `sequence(lo, hi)` | Generate integer values from `lo` up to but excluding `hi`. |
| `map()` | Create an empty `Map`. |
| `free(x)` | No-op on the compiled backend (arena); releases the object on the interpreter. |
| `drop(x)` | Release a typed-array payload early when it is the arena tail. |
| `exit(code)` | Terminate the process with the given exit code. |

```vyne
out("value = " + string(42));
out("type = " + type(3.14));
out("size = " + string(sizeof([1, 2, 3])));
out(sequence(0, 5));          # [0, 1, 2, 3, 4]
out(map());                   # {}
```

`out` is not available on the Arduino target; use `arduino.serial_print` / `arduino.serial_println` instead.

`free` does not reclaim arena memory in compiled code. Use regions for reclamation. `drop` is the early-release primitive:

```vyne
@bounded<16_384>
region frame {
    big :: Array<Float64> := make(1024);
    use(big);
    drop(big);       # payload released now; size and cap become 0
};
```

`drop` requires a typed `Array<Float64>`, `Array<Float32>`, or `Array<Int64>` variable. It is a no-op when the value is not the most recent arena allocation, and it rejects `scratch` arrays (`VNE-084`).

## Array methods

Arrays and typed arrays share a method surface. Typed arrays keep the native, unboxed representation when the receiver type is known.

| Method | Meaning |
| :--- | :--- |
| `push(x)` | Append an element |
| `pop()` | Remove and return the last element |
| `pop_front()` | Remove and return the first element |
| `size()` / `length()` | Element count |
| `clear()` | Remove all elements |
| `reverse()` | Reverse in place |
| `sort()` | Sort in place (numeric or lexicographic by element type) |
| `find(x)` / `delete_at(i)` | Locate or remove by value/index |
| `fill(v)` | Fill every element with `v` |
| `copy_to(dst)` | Copy into another typed array |
| `place_all(src)` | Copy all elements from `src` |
| `sum()` / `mean()` / `min()` / `max()` | Numeric reductions on typed numeric arrays |
| `join(sep)` | Join string elements with a separator |
| `back()` | Last element |

```vyne
xs :: Array<Float64> := [3.0, 1.0, 2.0];
xs.push(4.0);
xs.sort();
out(xs);                 # [1, 2, 3, 4]
out(xs.sum());           # 10
out(xs.mean());          # 2.5
out(xs.back());          # 4

s :: String := ["a", "b", "c"].join("-");
out(s);                  # a-b-c
```

Membership uses `in`; there is no separate `contains` method:

```vyne
if 3 in xs {
    out("present");
}
```

## Map methods

Maps have string keys and unspecified iteration order.

| Method | Meaning |
| :--- | :--- |
| `set(key, value)` | Insert or update |
| `get(key)` | Read a key; missing keys yield `null` |
| `get_or(key, default)` | Read a key with a fallback |
| `has(key)` | Whether the key exists |
| `delete(key)` | Remove a key |
| `keys()` / `values()` | Key or value list |
| `clear()` | Remove every entry |
| `size()` / `length()` | Entry count |

```vyne
cfg :: Map := map();
cfg.set("mode", "fast");
cfg["retries"] = 3;

if cfg.has("mode") {
    out(cfg.get("mode"));
}
out(cfg.get_or("timeout", 30));
```

## String methods

| Method | Meaning |
| :--- | :--- |
| `length()` / `size()` | UTF-8 byte length |
| `substr(start, len)` | Substring |
| `byte_at(i)` | Byte value at index `i` (integer) |
| `trim()` | Strip leading and trailing whitespace |
| `uppercase()` / `lowercase()` | Case conversion |
| `replace(old, new)` | Replace occurrences |
| `split(sep)` | Split into an array of strings |

```vyne
s :: String := "  hello, vyne  ";
out(s.trim());                 # "hello, vyne"
out(s.trim().uppercase());     # "HELLO, VYNE"
out("a,b,c".split(","));       # ["a", "b", "c"]
out(s.byte_at(2));             # byte value of 'h'
```

## Conversions and formatting

```vyne
int64("42");        # 42
int64(3.9);         # 3
float64("2.5");     # 2.5
float32(1.0);       # Float32
string(true);       # "true"
```

`int64` saturates on out-of-range text rather than trapping. Arithmetic wraps on `Int64`; there is no overflow checking.

## See also

- [Values & Types](page:types)
- [Literals & Expressions](page:expr)
- [Memory & Regions](page:regions)
- [Rule Sets](page:rulesets)
