---
id: keywords
title: Keywords
group: Reference
icon: fa-key
order: 2
description: Reserved words, built-in names, and contextual identifiers.
---

# Keywords

Reserved words are recognized by the lexer and cannot be used as ordinary identifiers. Built-in names (`out`, `string`, `drop`, ...) are reserved too.

## Values and declarations

`true`, `false`, `null`, `const`, `fn`, `interface`, `group`, `module`, `enum`, `ruleset`, `use`, `native`, `external`, `lib`, `as`, `deploy`, `dismiss`.

## Control flow

`if`, `else`, `while`, `through`, `loop`, `collect`, `filter`, `every`, `unique`, `break`, `continue`, `return`, `in`.

## Memory and lifetime

`region`, `scratch`.

## Errors

`try`, `catch`, `finally`, `throw`.

## Built-ins

`out`, `sizeof`, `type`, `string`, `int64`, `float64`, `float32`, `sequence`, `exit`, `free`, `drop`, `map`.

## Contextual names

These are parsed contextually rather than reserved globally:

- `requires`, `ensures`, `result` — contracts;
- `contracts`, `type_check`, `implicit_casting`, `warnings_ignore`, `strict`, `pedantic`, `verbose`, `profiling`, `debug`, `trace`, `max_iterations` — ruleset keys;
- `warnings`, `dynamic_casting`, `memory_limit` — both ruleset keys and lexer keywords;
- region policy names (`bounded`, `zeroed`, `deterministic`, `realtime`, `pool`, `slab`, `ring`, `streamed`, `persistent`, `speculative`, `device`, `layout`, `backward`, `recompute`, `mixed`) — valid only after `@`;
- `bump`, `soa`, `aos`, `struct_of_arrays` — policy arguments;
- `backward`, `grad` — AD intrinsics.

`self` is an ordinary identifier bound inside interface methods. `native` and `external` are reserved because they select import kinds.

## Not keywords

`for`, `foreach`, `switch`, `case`, `class`, `struct`, `let`, `var`, and `lambda` are not Vyne syntax. Use `through` for iteration, `interface` for structured values, and `:=` for declarations.

## See also

- [Operators](page:operators)
- [Grammar-style tour](page:tour)
- [Rulesets](page:rulesets)
