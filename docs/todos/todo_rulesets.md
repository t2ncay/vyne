# TODO — Per-File Safety Rulesets

**Last updated:** 2026-11-10

Draft. Companion to `todo.md`, `todo_2.md`, `todo_optimization.md`, `todo_libraries.md`,
and `todo_expansion.md`.

> **Status snapshot (2026-11-10).** **Not implemented.** The parser has no
> `safety { }` block, there is no `RulesetFlags` on `ProgramNode` / `CompileUnit`,
> and there is no per-file `scratch_bounds` / `region_escape` override. The
> ruleset system does now support executable contracts
> (`ruleset { contracts = "check" | "evidence" | "off" }`), plus the existing
> `type_check`, `memory_limit`, warning, and profiling categories. Those are the
> precedent this document should follow for syntax and reporting.
>
> The original "wait for Paper 1" gate no longer reflects the tree: the
> compiler has continued to evolve well past the region paper. Land this when it
> is prioritized; the design below still matches the current emitter/CLI
> structure, with the caveat that `setScratchBoundsEnabled` and the
> `--no-scratch-bounds` path should be re-checked before starting.

The feature this file specifies: **per-file safety rulesets.** A `.vy`
file can waive its own bounds checks or escape checks, and the compiler
respects the waiver for code emitted from that file only. The CLI keeps
final say. The build reports what was waived.

---

## The design, in one paragraph

Today, `--no-scratch-bounds` is a CLI flag that disables bounds checks
globally for the whole program. That's wrong-shaped for libraries: a
library author who has proven their own scratch accesses are in-bounds
should be able to say so _in their file_, without forcing every caller to
pass a flag. Per-file rulesets give them that. A file's `ruleset { safety
{ scratch_bounds = off } }` is a claim about the file's own code, like
Rust's `#![allow(...)]` or `unsafe`. It applies only to code emitted from
that file. It composes with the CLI via a strict precedence rule:
**CLI > file ruleset > language default.** The compiler prints a summary
of what was waived so a caller can see what they're importing.

**Build rules stay out.** `opt_level`, `march`, `blas`, `simd` are
properties of the build, not the file. They remain CLI flags. The
ruleset mechanism is for _safety assertions about the file's own code_,
nothing else.

---

## Non-goals

Explicitly out of scope. Each of these is a different feature that would
be a mistake to bundle in.

- **Build configuration in the source.** `opt_level = 3`,
  `march = "native"`, `blas = on` stay as CLI flags. Two sources of
  truth for the same knob is a bug factory.
- **Dynamic rulesets.** `scratch_bounds = some_variable` is not a
  thing. Rulesets are read by the parser before codegen. Static only.
- **Cross-file effect.** A file's waiver never disables checks in the
  code of another file, even if the other file imports it. Per-file
  scope is the whole point.
- **Capability system.** No "refuse to link unchecked libraries"
  (`--deny-unsafe-deps`) in v1. That's a Phase 5 conversation if it
  ever becomes one. For now, visibility is enough.
- **Boolean expression rulesets.** No `scratch_bounds = on && !pedantic`.
  Two states, bare flag or `= on/off`.

---

## Syntax

Two forms, matching the existing `ruleset { }` style:

```vyne
# Flag form — bare name means "disable this check"
ruleset {
    safety { scratch_bounds };
};

# Value form — explicit
ruleset {
    safety {
        scratch_bounds = off;
        region_escape  = off;
    };
};
```

Both are equivalent. The bare-flag form is sugar for `= off`, matching
the existing convention where `ruleset { dynamic_casting }` enables the
non-default behavior. Here the non-default is _disabling a safety check_,
so the bare flag disables.

The `safety` block is the only nested form in v1. Future blocks
(`codegen`, `target`) would follow the same shape if they ever exist.

### Precedence

Three levels, highest priority first:

| Priority    | Source           | Scope                            |
| ----------- | ---------------- | -------------------------------- |
| 1 (highest) | CLI flag         | Whole program, unconditional     |
| 2           | File ruleset     | Code emitted from that file only |
| 3 (lowest)  | Language default | `on` for every check             |

Concretely:

| CLI                   | File says | Effective for that file |
| --------------------- | --------- | ----------------------- |
| (none)                | (none)    | **on**                  |
| (none)                | `off`     | **off**                 |
| (none)                | `on`      | **on**                  |
| `--no-scratch-bounds` | (none)    | **off**                 |
| `--no-scratch-bounds` | `on`      | **off** (CLI wins)      |
| `--scratch-bounds`    | `off`     | **on** (CLI wins)       |

The invariant: **the CLI never loses.** A file can request a change;
the build can veto it.

---

## Phase 1 — Parser + AST (~1 day)

Get the syntax recognized and the two flags carried on the AST. At the
end of this phase, the parser accepts the new syntax and stores the
flags, but nothing downstream reads them yet.

### T1.1 — Add a `RulesetFlags` struct

**File:** `vyne/compiler/ast/ast.h` (or `vyne/compiler/types.h`)

Add a small struct that will be carried on `ProgramNode`:

```cpp
struct RulesetFlags {
    bool scratchBounds = true;    // default on
    bool regionEscape  = true;    // default on
};
```

Two booleans, not a general config object. If a third check lands
(`precision`, `bounds`, whatever), add a third field. Keep it flat.

### T1.2 — Attach it to `ProgramNode`

**File:** `vyne/compiler/ast/ast.h`

`ProgramNode` gets:

```cpp
class ProgramNode : public ASTNode {
    RulesetFlags rulesets;   // NEW
public:
    // ...
    const RulesetFlags& getRulesets() const { return rulesets; }
    void setRulesets(const RulesetFlags& r) { rulesets = r; }
};
```

No other AST node carries rulesets. They're program-scoped, one per
translation unit.

### T1.3 — Parse the nested `safety { }` block

**Files:** `parser/parser.h`, `parser/parser.cpp`

In `parseRulesetBlock`, add a case for a nested block when the ruleset
name is `safety`:

```cpp
// Inside parseRulesetBlock's loop, after reading `ruleName`:

if (ruleName.name == "safety" && peekToken().type == VTokenType::Left_CB) {
    parseSafetyRuleset(line, rulesetFlags);   // new method
    continue;
}
```

Add a new private method:

```cpp
void parseSafetyRuleset(int line, RulesetFlags& out);
```

Implementation: consume `{`, loop over `key` or `key = on/off` pairs,
match against the two known keys, ignore-and-warn on unknown keys,
consume `}` and optional `;`.

Recognized keys for v1:

- `scratch_bounds`
- `region_escape`

Unknown key → `emitWarning` with code `VNE-033` and a suggestion list
naming the two supported keys. Do not throw; a future compiler version
might understand a key this one doesn't, and a warning is friendlier
than a hard error for forward-compat.

### T1.4 — Thread `RulesetFlags` through `parseProgram`

**Files:** `parser/parser.h`, `parser/parser.cpp`

`parseProgram` currently returns a fresh `ProgramNode`. Change it to
thread a `RulesetFlags` reference down to `parseRulesetBlock` so the
nested block can write into it, then set the flags on the returned
`ProgramNode` before returning.

The `parseRuleset` and `parseRulesetBlock` signatures both need a new
parameter:

```cpp
std::unique_ptr<ASTNode> parseRuleset(RulesetFlags& flags);
std::unique_ptr<ASTNode> parseRulesetBlock(int line, RulesetFlags& flags);
```

Only the top-level ruleset in a file can set safety flags. A ruleset
inside a function body is not processed by this path, and that's
correct — safety waivers are file-scoped, not block-scoped.

### T1.5 — Verify

After Phase 1, the parser should accept and reject correctly:

**Accept:**

```vyne
ruleset { safety { scratch_bounds = off }; }
ruleset { safety { scratch_bounds }; }
ruleset { safety { region_escape = off; scratch_bounds = off; }; }
```

**Reject (or warn):**

```vyne
ruleset { safety { unknown_key = off }; }   # warning, ignored
ruleset { safety { scratch_bounds = maybe }; }   # parse error
```

**Not part of the safety block:**

```vyne
ruleset { dynamic_casting; }   # existing behavior unchanged
```

Add a parser test in `examples/transpiler/ruleset_test.vy` that exercises
each form.

**Effort:** 1 day.

---

## Phase 2 — Linker + driver (~half a day)

Now the flags need to flow from each parsed file through to the emitter.
The linker already produces one `CompileUnit` per file; each needs to
carry its own flags.

### T2.1 — Extend `CompileUnit`

**File:** `vyne/compiler/codegen/linker.h`

Add a `RulesetFlags` to the struct:

```cpp
struct CompileUnit {
    std::string canonicalPath;
    std::shared_ptr<ProgramNode> ast;
    std::string alias;
    bool isExtern = false;
    RulesetFlags rulesets;   // NEW — copied from the parsed ProgramNode
};
```

### T2.2 — Populate the flags in `visit`

**File:** `vyne/compiler/codegen/linker.cpp`

In `VyneLinker::visit`, after parsing the AST, copy the flags:

```cpp
CompileUnit unit;
unit.canonicalPath = canonicalPath;
unit.ast           = ast;
unit.alias         = alias;
unit.isExtern      = isExtern;
unit.rulesets      = ast->getRulesets();   // NEW
order.push_back(std::move(unit));
```

One line.

### T2.3 — Set per-unit flags in the driver

**File:** `cli/file_handler.cpp`

The emitter already has `setScratchBoundsEnabled(bool)`. Today it's
called once, before the unit loop. Move it _inside_ the loop, so each
unit reconfigures the emitter before its AST is compiled:

```cpp
C_Emitter emitter;
emitter.reset();
emitter.setScratchBoundsEnabled(scratchBounds);   // CLI default, may be overridden

for (auto& unit : units) {
    emitter.setSourceDir(...);

    // NEW: file ruleset wins over the default, but CLI wins over the file
    if (!scratchBoundsFromCliExplicit) {
        // CLI did not pass --scratch-bounds or --no-scratch-bounds
        emitter.setScratchBoundsEnabled(unit.rulesets.scratchBounds);
    }
    // else: CLI said something, leave the CLI value in place

    if (unit.alias.empty()) {
        unit.ast->compile(emitter);
    } else {
        unit.ast->compileAliased(emitter, unit.alias);
    }
}
```

Requires a new boolean `scratchBoundsFromCliExplicit` to distinguish
"CLI didn't mention it" from "CLI said on (the default)". Do the same
for `region_escape` when the region-escape check gets a runtime toggle
(it's currently always-on; a matching `setRegionEscapeEnabled` would be
symmetric).

### T2.4 — Same treatment for `region_escape`

`RegionNode::compile` currently always emits the escape check via
`checkRegionEscape`. Add a `bool regionEscapeEnabled = true;` to the
emitter, wire it through the same way, and gate the three check call
sites (`AssignmentNode`, `MemberAssignmentNode`, `IndexAssignmentNode`)
on it.

**Effort:** half a day.

---

## Phase 3 — CLI override (~half a day)

The CLI needs a way to say "I know better than the file." Extend the
flag parser in `main.cpp`.

### T3.1 — Explicit CLI values

Today:

```cpp
bool scratchBounds = false;
// ...
} else if (arg == "--no-scratch-bounds") {
    scratchBounds = false;
}
```

Replace with a tri-state:

```cpp
enum class CliBool { Unset, On, Off };
CliBool scratchBoundsCli = CliBool::Unset;
CliBool regionEscapeCli  = CliBool::Unset;

// ...

} else if (arg == "--no-scratch-bounds") {
    scratchBoundsCli = CliBool::Off;
} else if (arg == "--scratch-bounds") {
    scratchBoundsCli = CliBool::On;
} else if (arg == "--no-region-escape") {
    regionEscapeCli = CliBool::Off;
} else if (arg == "--region-escape") {
    regionEscapeCli = CliBool::On;
}
```

`--no-scratch-bounds` is preserved as-is; `--scratch-bounds` is new and
allows an explicit "on" that overrides a file's `= off`.

### T3.2 — Pass tri-states through `runFile`

**Files:** `cli/file_handler.h`, `cli/file_handler.cpp`

The signature changes:

```cpp
int runFile(const std::string& filename, SymbolContainer& env,
            const std::string& mode,
            bool enforceIntegrity = false,
            bool nativeIsa = false,
            CliBool scratchBoundsCli = CliBool::Unset,
            CliBool regionEscapeCli  = CliBool::Unset);
```

Do this once. It replaces the current `bool scratchBounds` parameter.

Inside the unit loop, the effective value is:

```cpp
bool effectiveScratchBounds =
    (scratchBoundsCli == CliBool::On)  ? true  :
    (scratchBoundsCli == CliBool::Off) ? false :
    unit.rulesets.scratchBounds;   // unset → file value → default true
```

Same for region escape.

### T3.3 — Keep the old flag working

`--no-scratch-bounds` maps to `CliBool::Off`. Scripts that pass it keep
working. No deprecation needed.

**Effort:** half a day.

---

## Phase 4 — Visibility (~half a day)

A library that waives a check should not be silent about it. When the
build succeeds, print the effective safety configuration.

### T4.1 — Collect waivers during the unit loop

**File:** `cli/file_handler.cpp`

Before the compile loop, initialize an empty list:

```cpp
struct SafetyReport {
    std::string file;
    std::string rule;
    bool effective;
};
std::vector<SafetyReport> safetyReport;
```

After computing the effective values for each unit, if either is `false`
and the source is _not_ the CLI:

```cpp
if (!effectiveScratchBounds && scratchBoundsCli == CliBool::Unset) {
    safetyReport.push_back({unit.canonicalPath, "scratch_bounds", false});
}
```

Same for region escape.

### T4.2 — Print the report

After the compile finishes, before the summary block:

```
[safety] region_escape:  on (default)
[safety] scratch_bounds: off (from lib/matmul.vy, imported by main.vy)
```

One line per rule. "on (default)" when no file and no CLI waived it.
"off (from <file>)" when a file waived it. "off (from CLI)" when the CLI
waived it globally. Never print "off" without a source.

For imported files, walk the import graph to find who pulled the file
in. That's a small tree walk over `units` — the linker already produced
them in topological order, so the direct importer is the next unit that
references this one. If the walk is annoying, print the file's own path
and skip the "imported by" clause for v1.

### T4.3 — Suppress when quiet

Skip the report when `Vyne::isQuietMode()` is true, matching the
diagnostic system's behavior. `ruleset { warnings }` in the entry file
turns it on.

**Effort:** half a day.

---

## Phase 5 — Tests, docs, and the paper paragraph (~half a day)

### T5.1 — Test cases

Create `examples/rulesets/` with:

- `cli_default.vy` — no ruleset, checks on, verifies VNE-072 fires on
  a bad index.
- `file_waives.vy` — `ruleset { safety { scratch_bounds = off }; }`,
  bad index does _not_ abort.
- `file_waives_region.vy` — `ruleset { safety { region_escape = off }; }`,
  direct escape compiles.
- `cli_overrides_file.vy` — file says `off`, compiled with
  `--scratch-bounds`, checks fire.
- `imported_waives.vy` + `imported_lib.vy` — a library that waives
  checks, imported by a file that doesn't. Verify the caller's code is
  still checked; the library's is not.
- `unknown_key.vy` — `ruleset { safety { no_such_rule = off }; }`,
  verify warning `VNE-033` fires.

Wire them into `examples/rulesets/run_rulesets.ps1`, mirroring
`run_safety.ps1`'s shape.

### T5.2 — Docs

**File:** `docs/tutorials/reference/rulesets.md`

The reference page for rulesets exists today. Add a `safety` subsection:

- What each rule does.
- Default values.
- Precedence table.
- Per-file scope.
- The `[safety]` build report.
- When _not_ to use it (build rules go in the CLI).

Short. One screen.

### T5.3 — Paper paragraph

**File:** `regions.md` §6.7 or a new §6.11

One paragraph in the limitations section:

> Scratch bounds checking is on by default and the compiler does not
> currently offer a per-file opt-out. The CLI's `--no-scratch-bounds`
> disables checks globally, which is the wrong shape for libraries: a
> library whose accesses are provably in-bounds should be able to say so
> without forcing every caller to pass a flag. A per-file
> `ruleset { safety { scratch_bounds = off }; }` construct, with strict
> CLI-over-file precedence and a build-time report of what was waived,
> is the natural fix. It is deferred from the current paper because it
> changes the parser and the linker and would require re-measuring
> §5.6–§5.9.

That's the honest framing. The feature is real, it's small, it's
deferred, and the paper says why.

**Effort:** half a day.

---

## Implementation order

```
Phase 1  Parser + AST                      1 day
Phase 2  Linker + driver                   0.5 day
Phase 3  CLI override                      0.5 day
Phase 4  Visibility report                 0.5 day
Phase 5  Tests, docs, paper paragraph      0.5 day
                                          ────────
                                          3 days total
```

Do them in order. Each phase is independently testable; if any phase
turns out to need more than expected, the earlier phases are still
shippable.

---

## Files touched, exhaustively

| File                                    | Change                                            |
| --------------------------------------- | ------------------------------------------------- |
| `vyne/compiler/types.h` or `ast.h`      | New `RulesetFlags` struct                         |
| `vyne/compiler/ast/ast.h`               | `ProgramNode` carries `RulesetFlags`              |
| `parser/parser.h`                       | New methods, signature changes                    |
| `parser/parser.cpp`                     | Nested `safety { }` block parsing, flag threading |
| `vyne/compiler/codegen/linker.h`        | `CompileUnit` carries flags                       |
| `vyne/compiler/codegen/linker.cpp`      | One-line copy from `ProgramNode`                  |
| `vyne/compiler/codegen/emitter.h`       | `regionEscapeEnabled` field + setter              |
| `vyne/compiler/codegen/assignments.cpp` | Gate `checkRegionEscape` call                     |
| `vyne/compiler/codegen/collections.cpp` | Gate `checkRegionEscape` call                     |
| `vyne/compiler/codegen/members.cpp`     | Gate `checkRegionEscape` call                     |
| `cli/file_handler.h`                    | Signature change (tri-state params)               |
| `cli/file_handler.cpp`                  | Per-unit flag setting, safety report              |
| `main.cpp`                              | Tri-state CLI parsing                             |
| `examples/rulesets/`                    | New test directory                                |
| `docs/tutorials/reference/rulesets.md`  | New subsection                                    |

~14 files. Most of them one or two lines.

---

## What NOT to do

- **Don't add `codegen` or `target` or `output` blocks to the ruleset.**
  Those are build configuration. The ruleset mechanism is for safety
  assertions about the file's own code. The moment you add `opt_level`,
  you have two sources of truth and a maintenance problem.
- **Don't let a ruleset apply to anything but the file that contains
  it.** Cross-file effect is a capability system. If you want one,
  design one. Don't accidentally build a half of one.
- **Don't error on unknown keys.** Warn and continue. A future version
  of the compiler might understand a key this one doesn't. Forward
  compatibility matters more than strictness here.
- **Don't print the safety report unconditionally.** Respect
  `isQuietMode()`, matching the diagnostic system.
- **Don't land this without re-measuring the safety suite and the benchmark.**
  Every phase touches code that either emits or gates the checks the paper and
  `examples/safety/` exercise. The Paper 1 freeze is historical; the current
  requirement is a before/after safety-suite run and a benchmark comparison.

---

## What this buys

Once landed:

- A library that knows its scratch accesses are in-bounds can say so
  once, in its own source, instead of forcing every caller to pass
  `--no-scratch-bounds`.
- The caller sees exactly which imported files waived what, in the
  build output. No hidden unsafe code.
- The CLI remains the final authority. A build can always override a
  file's claim.
- The mechanism generalizes: any future safety rule gets the same
  treatment with a one-line addition to `RulesetFlags` and a new key
  in `parseSafetyRuleset`.

The feature is small, well-scoped, and it fixes the specific wrong-shape
problem in the current `--no-scratch-bounds` design. Three days of work,
mostly in files that haven't been touched since Phase 1.

---
