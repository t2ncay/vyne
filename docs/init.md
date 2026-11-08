# `vynec init`

`vynec init` scaffolds a new Vyne project. On a terminal it opens an
ftxui questionnaire; in scripts and CI it is fully non-interactive.

```bash
vynec init                 # interactive questionnaire
vynec init --name demo --kind ml --no-git
```

## Questionnaire

The TUI asks up to five questions and then shows a confirmation screen:

1. **Project kind** — ML training, Embedded, Scientific, Image / Video,
   or Mixed.
2. **Targets** — Desktop, Arduino Uno R3, Coral Edge TPU, Jetson Nano.
3. **Libraries** — `vlin`, `vml`, `vstat`, `vtermplot`, `vfft`,
   `vcomplex`, `vcsv`, `vimage`, `vconv`.
4. **CUDA** — only when Desktop is selected and the project is not
   Embedded. Detects `nvcc` and warns without blocking.
5. **License** — AGPL-3.0-only, MIT, or Proprietary.

Every question can be skipped by passing the matching flag. The
terminal must be at least 80x24; otherwise `vynec init` exits cleanly
with code 2. In a non-TTY environment the questionnaire is not launched.

## Templates

| Kind | What it demonstrates |
| --- | --- |
| `blank` | `@bounded` and `@deterministic` regions |
| `ml` | Iris MLP, manual backprop, Adam, `vtermplot` loss curve |
| `embedded` | Arduino Uno blinky and serial over `@bounded` |
| `scientific` | Lorenz RK4 integration, Lyapunov estimate, phase portrait |
| `sr` | ESPCN-style 2x upscaling with `vimage` + `vconv` |

`mixed` uses the `ml` template with every library selected.

## Flags

| Flag | Meaning |
| --- | --- |
| `--name <name>` | project name; also suppresses the TUI |
| `--kind <kind>` | blank, ml, embedded, scientific, sr, mixed |
| `--targets <list>` | comma-separated target names |
| `--libraries <list>` | comma-separated library names |
| `--cuda yes\|no` | add `[device.cuda]` |
| `--license <id>` | AGPL-3.0-only, MIT, proprietary |
| `--yes` | accept defaults without prompting |
| `--no-tui` | force non-interactive |
| `--no-git` | skip `git init` and the initial commit |

Exit codes: 0 success, 1 cancel, 2 non-interactive without a name,
3 `project.toml` already exists, 4 generation/configuration error.

## Tests

```bash
xmake test-init
```

`xmake test-init` runs `scripts/test_init.ps1`, which generates every
template, compiles and runs the host templates, and compiles the
embedded template with `avr-gcc` when it is installed.
