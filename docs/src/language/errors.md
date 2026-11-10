---
id: errors
title: Error Handling
group: Language Reference
icon: fa-triangle-exclamation
order: 11
description: Try, catch, finally, and throw across the interpreter and compiled backends.
---

# Error Handling

Vyne uses exceptions for recoverable failure. The compiled backend implements them with a bounded `longjmp` frame stack; the AST interpreter unwinds the evaluation stack.

```vyne
try {
    parse(input);
} catch (err) {
    out("failed: " + string(err));
} finally {
    cleanup();
};
```

A `try` must be followed by `catch`, `finally`, or both. The catch variable is optional:

```vyne
try {
    risky();
} catch {
    out("failed");
};
```

## Throwing

`throw` accepts any value. There is no dedicated exception type; strings are the common case, but numbers, maps, and structs work too.

```vyne
fn checked_div(a :: Float64, b :: Float64) -> Float64 {
    if b == 0.0 {
        throw "division by zero";
    }
    return a / b;
}

try {
    out(checked_div(1.0, 0.0));
} catch (err) {
    out("caught: " + string(err));
};
```

An uncaught throw terminates the compiled program with a message. The runtime keeps `VYNE_MAX_EXC_FRAMES` = 64 nested exception frames; exceeding that is a fatal error.

## Finally

`finally` runs on both the normal path and the throwing path, after the matching `catch` body:

```vyne
try {
    out("work");
} catch (err) {
    out("recovered");
} finally {
    out("cleanup");
};
```

Use `finally` for state that must be restored regardless of outcome. It also runs when a `return` leaves the `try` block.

## Interaction with regions

A region unwind is emitted for every open region when an exception crosses a region boundary. Values allocated inside the region are reclaimed before control reaches the handler. If you need a value to survive the exception, commit or move it before throwing.

```vyne
try {
    region work {
        result :: Array<Float64> := compute();
        region.commit(result);
        maybe_throw(result);
    };
} catch (err) {
    out("work failed");
};
```

## Error values

`catch` binds the thrown value without a type annotation. Convert it for display with `string(err)`, or test it if you throw structured values:

```vyne
try {
    load_config();
} catch (err) {
    if type(err) == "String" {
        out("config error: " + err);
    } else {
        out("unexpected error");
    };
};
```

## Compile-time diagnostics

Exceptions are a runtime mechanism. Compile errors use the `VNE-*` diagnostic system with source excerpts and suggestions. See [Error Codes](page:errcodes) for the code index and [Rulesets](page:rulesets) for `contracts` mode.

## See also

- [Control Flow](page:flow)
- [Memory & Regions](page:regions)
- [Error Codes](page:errcodes)
