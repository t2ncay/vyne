---
id: hello
title: Hello, World
group: Getting Started
icon: fa-terminal
order: 3
description: Your first Vyne program, the execution modes, and a first look at regions.
---

# Hello, World

Create `hello.vy`:

```vyne title="hello.vy"
out("hello, vyne");
```

Run it:

```shell
./vynec hello.vy
```

A bare invocation compiles the source and runs the resulting program. `out` appends a newline; use `vcore` or `arduino.serial_print` when you need byte-level control.

## The same file in three modes

```shell
./vynec --ast hello.vy        # AST interpreter, no C emitted
./vynec --compile hello.vy    # emit C, compile a binary, stop
./vynec --run hello.vy        # compile and run
./vynec --emit-c hello.vy     # write hello.vy.c and stop
```

The interpreter is the quickest feedback loop. `--compile` is the path to a standalone binary. Both consume the same parsed tree.

## Values and interpolation

```vyne title="values.vy"
name :: String := "Vyne";
version :: Float64 := 0.6;
count :: Int64 := 3;

out("hello, " + name);
out("version " + string(version));
out("count = " + string(count));
```

Vyne has explicit types when you want them:

```vyne title="types.vy"
x :: Int64 := 42;
pi :: Float64 := 3.141592653589793;
flag :: Bool := true;
items :: Array<Float64> := [1.0, 2.0, 3.0];
```

`:=` introduces a new binding; `=` reassigns an existing one. The distinction is enforced by the parser and is the single most important syntax change from older Vyne code.

## A first region

```vyne title="region.vy"
through epoch :: 1..5 -> loop {
    region scratch {
        xs :: Array<Float64> := [1.0, 2.0, 3.0];
        out("epoch " + string(epoch) + " sum " + string(xs.sum()));
    };
};
```

Everything allocated inside `region scratch` is reclaimed at the closing brace. The loop can run five times or five million times; the arena returns to its entry offset on every iteration. That is the core memory model, and the rest of the language is designed to make it visible rather than implicit.

## Comments

```vyne
# line comment
out("still code");   # trailing comment

/* block comments are also accepted */
```

## Next

- [Language Tour](page:tour) — every construct in one pass.
- [Memory & Regions](page:regions) — the full policy matrix.
- [Projects & Tooling](page:projects) — scaffold, build, run, and benchmark a real project.
