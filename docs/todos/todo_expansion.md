# TODO — Domain Expansion & Technical Milestones

**Last updated:** 2026-11-09
**Status:** Draft strategy. This file records decisions and milestones only; it does
not authorise code changes.
**Baseline:** v0.6.0 (regions + policies, `Float32`/CUDA `@device`, scientific
libraries, Arduino AVR backend, `vynec init`).

Companion documents:

- `todo.md` — Paper 1 pipeline (regions, scratch, peak bounds).
- `todo_2.md` — post-Paper-1 feature axes (shape/range/unique/effect types,
  device regions, region policies).
- `todo_libraries.md` — standard-library roadmap and the per-PR workflow.
- `todo_optimization.md` — codegen peepholes and structural performance work.
- `todo_rulesets.md` — per-file safety rulesets.
- `todo_drug_discovery.md` — the `vchem` pipeline.
- `milestone.md` (repo root) — overall scientific/numerical roadmap.

---

## 0. Executive summary

Vyne should not become a general-purpose language. It should become the best
language for **where math, memory, and hardware meet**: deterministic numerical
code that runs unchanged on a laptop GPU, a browser, and a microcontroller.

The v0.6.0 moat is four things and only four things:

1. **Deterministic memory** — regions, compile-time peak bounds (`@bounded`),
   no GC, no hidden allocation.
2. **Multi-target from one AST** — interpreter, C transpiler, CUDA `@device`,
   AVR backend, and soon WASM.
3. **Numerical precision as a first-class concern** — `Float32`/`Float64`,
   typed arrays, native math, FFT, complex, linear algebra.
4. **Provable determinism** — `@deterministic`, `@zeroed`, `@bounded`, and the
   diagnostics/evidence they produce.

A domain is in scope **only if at least three of those four are load-bearing**.
If a domain could be served just as well by Python, Go, or JavaScript, it is
out of scope. If it needs a GUI toolkit, a web framework, or a text-processing
stack, it is out of scope.

**The recommended order is: sensing → control/robotics → audio → WASM →
autodiff → SciML → (deferred) Tier 3.** The first three phases reuse almost
everything that already exists; autodiff is the one genuinely new compiler
feature, and it should land only after the library ecosystem and demos make it
worth the cost.

---

## 1. The selection gate

Every candidate domain is scored on six criteria. Weights reflect the strategy:
leverage the moat first, chase market size last.

| Criterion | Weight | Test |
| --- | ---: | --- |
| Moat leverage | 25% | Do 3–4 of deterministic memory / multi-target / numerical precision / no-GC apply? |
| Existing-asset reuse | 20% | Can we start from a shipped library, template, or demo? |
| Compiler delta (inverse) | 20% | How much new compiler/backend work does the first useful demo need? |
| Time-to-first-demo | 15% | Can a credible demo ship in ≤ 4 weeks? |
| Market pull | 10% | Is there a real buyer/user, not just technical neatness? |
| Defensibility / certification | 10% | Does the domain reward provable bounds and determinism? |

Score each criterion 0–5, multiply by weight. **≥ 4.0 = Now. 3.0–3.9 = Next.
2.0–2.9 = Later. < 2.0 = Watch/Reject.**

### Portfolio scorecard (initial estimate)

| Domain | Moat | Reuse | Delta | Demo | Market | Cert | Weighted | Verdict |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Industrial IoT / embedded sensing | 5 | 5 | 4 | 5 | 4 | 5 | **4.75** | **Now** |
| Robotics & autonomous systems | 5 | 4 | 3 | 3 | 5 | 5 | **4.20** | **Now** |
| Real-time audio / DDSP | 4 | 4 | 3 | 3 | 4 | 4 | **3.65** | **Next** |
| WebAssembly target | 4 | 5 | 5 | 4 | 4 | 3 | **4.30** | **Now** |
| Differentiable simulation | 5 | 2 | 1 | 2 | 5 | 4 | **2.95** | **Later (AD-gated)** |
| Scientific ML (PINNs) | 5 | 3 | 2 | 3 | 4 | 4 | **3.55** | **Next (AD-gated)** |
| HFT / quant finance | 5 | 2 | 1 | 1 | 5 | 3 | **2.55** | **Later** |
| Smart contracts | 4 | 2 | 1 | 1 | 3 | 5 | **2.35** | **Later / Watch** |
| Game engines / lockstep | 3 | 2 | 1 | 1 | 3 | 2 | **1.95** | **Watch / Reject** |

The two "Now" domains that matter most are **embedded sensing** (a weekend
demo on existing infrastructure) and **WASM** (a cheap, high-visibility
multiplier on the existing C emitter). Robotics is "Now" because it is the
direct next step from sensing, not because the first demo is cheap.

---

## 2. The shared investment: language capabilities

These are **multipliers**. Build each once, unlock several domains. Ordered by
return on engineering weeks (ew).

Legend: `[ ]` not started, `[~]` in progress, `[x]` shipped.

### C1 — MCU peripheral-bus bindings (I2C / SPI / Wire) `[~]`

**Status (2026-11-09):** implemented on branch `feat/arduino-i2c-spi`.
Bundled-library build integration, `mcu_i2c.h` / `mcu_spi.h`, 14
`ARDUINO_MAP` entries, opt-in `VYNE_USE_I2C` / `VYNE_USE_SPI` guards, and
typed `vi2c` / `vspi` wrappers with validation ship. Firmware compiles
and links; hardware confirmation on a real IMU and SPI loopback is the
remaining exit-criteria item.

**Why.** Every real sensor, IMU, motor driver, display, and audio codec talks
I2C or SPI. The AVR runtime deliberately binds neither today: those Arduino
libraries ship `.cpp` sources that the Vyne AVR integration cannot compile
because it only adds `cores/arduino/` to the include path.

**Current state.** `arduino.*` covers serial, timing, math, pins, memory, bits,
EEPROM, and edge interrupts. No I2C/SPI/Wire.

**Work.**

- Teach the AVR build integration to scan `vendor/arduino/libraries/*/library.properties`,
  add each library's `src/` to the include path, and link its `.c`/`.cpp`.
- Add `i2c_begin`, `i2c_write`, `i2c_read`, `i2c_write_reg`, `i2c_read_reg`,
  `spi_begin`, `spi_transfer`, `spi_transfer16` to `ARDUINO_MAP`.
- Add a typed Vyne surface (`vi2c`, `vspi`) with device-address validation and
  error returns rather than silent halts.
- Hardware test: read a known IMU (e.g. MPU-6050) and print WHO_AM_I over serial.

**Exit criteria.** `examples/arduino/i2c_imu.vy` builds and prints the IMU
identity on a real Uno; one SPI loopback test passes; the host CI still builds
without avr-gcc.

**Effort.** 2–3 ew. **Risk.** Medium — build-integration changes touch every
AVR build.

---

### C2 — `@realtime<deadline_ms>` policy `[ ]`

**Why.** Robotics and audio need a guarantee, not a measurement: the control
loop or audio callback must finish before its deadline or the system degrades
predictably. This is a natural extension of the region-policy machinery.

**Current state.** `@bounded`, `@zeroed`, `@deterministic`, `@ring`,
`@streamed`, `@persistent`, `@slab`, `@device` exist. There is no timing policy.

**Design sketch.**

```vyne
@realtime<2> region control_tick {
    # body must complete within 2 ms
};
```

- On entry: record `micros()`/`now_ns()`.
- On every exit path (including early returns): compare elapsed against the
  deadline and call `vyne_panic_deadline` on overrun (or invoke a registered
  overrun handler).
- Emit per-region worst-observed and total-overrun counters, exposed through
  `vcore` / `vmem` telemetry.
- Static mode (future): reject obviously unbounded loops and blocking calls
  (`delay`, `serial_read_line`, `vfs.*`) inside `@realtime`.

**Exit criteria.** Deadline overrun panics on AVR and desktop; telemetry
reports worst-case and overrun count; safety suite includes
`region_deadline_overrun.vy` and `region_deadline_met.vy`.

**Effort.** 3–4 ew for the runtime policy; 8–12 ew for real static WCET
analysis (separate milestone, certification track).

**Risk.** A runtime check is not a proof. Be explicit in docs: `@realtime` is
a watchdog today, a WCET obligation later.

---

### C3 — `@invariant` / contracts `[ ]`

**Why.** Certification and safety domains need executable assertions that can
be stripped in release builds but retained in evidence builds. This is also the
substrate for "this Kalman filter satisfies state bounds" claims.

**Design sketch.** Function-level `requires` / `ensures` / loop `@invariant`,
lowered to `vyne_assert` calls gated by a ruleset flag (`contracts = "evidence"`),
with a stable diagnostic ID and source location. Later: emit a traceability
record per contract for the certification package.

**Exit criteria.** Contracts fire on violation on desktop and AVR; a ruleset can
disable them for release; evidence mode writes a machine-readable contract log.

**Effort.** 2–3 ew. **Risk.** Low.

---

### C4 — `vsignal` DSP library `[ ]`

**Why.** Filters are the shared primitive of robotics, audio, and industrial
sensing: moving average, exponential smoothing, FIR, biquad IIR (low/high/band
pass/notch), median, resampling, windowing, and spectral helpers. This is the
next library in `todo_libraries.md` and the single highest-reuse library for
the new domains.

**Current state.** `vfft` and `vcomplex` exist; `vstat` has descriptive stats
and `vinterp` has 1-D/2-D interpolation. No filter library.

**Work.** `vsignal/Types.vy` (Filter state, coefficients), `Kernels.vy`
(native-ABI leaf loops), `Iir.vy`, `Fir.vy`, `Windows.vy`, `Resample.vy`,
`Spectral.vy`, facade, README, test, benchmark. Confirm `vfft` precision and
supported lengths first (still unconfirmed since v0.4.0).

**Exit criteria.** `vsignal_test.vy` checks biquad responses against hand
values; `vsignal_bench.vy` times 1 M-sample filtering; moving-average and
biquad demos run on both desktop and MCU.

**Effort.** 3–4 ew. **Risk.** Low–medium (numeric edge cases at Nyquist).

---

### C5 — `vcontrol` control-systems library `[ ]`

**Why.** PID, complementary filters, and Kalman filters are the canonical
robotics primitives. Pure-Vyne, no compiler work.

**Work.** `Pid` (anti-windup, derivative filtering, bumpless transfer),
`ComplementaryFilter`, `Kalman1D`, `Kalman` (small-matrix), `StateSpace`
helpers. Depends on `vsignal` for filters and `vlin`/`vdecomp` for matrix
operations.

**Exit criteria.** Closed-loop step response on a simulated plant matches a
reference; the same controller compiles for AVR and desktop; PID runs on a real
motor or LED-dimmer rig.

**Effort.** 2–3 ew. **Risk.** Low.

---

### C6 — `vaudio` audio I/O and graph `[ ]`

**Why.** Audio is the natural second act after images: the same decode → process
→ encode shape, with `vfft`, `vcomplex`, and `@ring` already in place.

**Current state.** No audio module (the old `vaudio` was removed in v0.5.0).

**Phased work.**

1. **Offline first.** WAV read/write (PCM 16/24/32, float32), and an offline
   render graph. No real-time backend. This is a 2–3 ew slice and immediately
   useful for DSP experiments.
2. **Streaming.** `@ring`-backed circular buffers, block processors, and a
   deterministic pull/push graph. Reuses `@ring` and `@realtime`.
3. **Real-time backends.** ALSA/WASAPI/CoreAudio/ASIO. This is the expensive,
   platform-heavy part; defer until phases 1–2 have users.
4. **Embedded.** Cortex-M audio (I2S + DMA) after C13 (ARM backend) lands.

**Exit criteria (phase 1+2).** `vaudio_test.vy` round-trips WAV; a real-time
biquad/FFT demo runs on desktop with `@ring` and `@realtime`; CPU budget stays
deterministic.

**Effort.** 4–6 ew for phases 1–2; 4–6 ew more for real-time backends.
**Risk.** High at phase 3 (drivers), low at phase 1.

---

### C7 — WASM target `[ ]`

**Why.** The C emitter already exists; clang's `wasm32` backend is mature. This
is the cheapest way to put Vyne in front of a huge audience (browser demos,
in-browser inference, reproducible notebooks) without a new code generator.

**Work.**

- `--target wasm` on the CLI; emit C, compile with `clang --target=wasm32`,
  link with `wasm-ld`, produce `.wasm` plus a `.js`/`.d.ts` glue file.
- Runtime shims: `vfs` → Emscripten-style virtual FS or in-memory FS;
  `vcore.now`/`now_ns` → `performance.now()`; `vmem` → instrumentation only.
- Determinism mode: fixed random seed, no wall-clock in `@deterministic`.
- Size budget: compile with `-Os` and report `.wasm` size per example; keep the
  ESPCN demo under a stated ceiling.
- Browser harness and CI job.

**Exit criteria.** `examples/sr` ESPCN runs in a headless browser harness and
produces the same output as native; `lorenz` renders in the browser; size and
startup budgets are tracked in CI.

**Effort.** 3–5 ew. **Risk.** Medium (JS interop, FS semantics).

---

### C8 — Deterministic fixed-point `Fixed64` `[ ]`

**Why.** AVR `double` is 32-bit, GPUs differ in rounding, and lockstep/HFT/
contracts need bit-identical results across machines. `Fixed64` (Q32.32) gives
deterministic arithmetic independent of FPU and compiler flags.

**Work.** A `Fixed64` scalar type with wrapping/saturating ops, defined rounding
for multiply/divide, conversions from/to `Int64`/`Float64`, and a codegen path
that avoids floating-point intermediates. Test on desktop, AVR, and WASM.

**Exit criteria.** Golden-vector test yields identical bits on all three
targets; `Fixed64` compiles in a lockstep demo.

**Effort.** 2–3 ew. **Risk.** Low–medium (rounding semantics must be documented
precisely).

---

### C9 — Concurrency: thread-local arenas, sync primitives, `vnet` `[ ]`

**Why.** HFT needs predictable multi-core latency; some robotics/audio pipelines
want a producer/consumer split. This is a large, deliberately deferred
investment.

**Work.** Thread-local region arenas, a small atomics/spin/park surface,
bounded lock-free queues (SPSC/MPSC), `vnet` (TCP/UDP, low-latency paths),
hugepage allocation for region backings.

**Exit criteria.** A benchmark shows bounded tail latency under load; a producer
thread and consumer thread exchange region-owned buffers without copies.

**Effort.** 6–10 ew. **Risk.** High (concurrency bugs, platform differences).
**Gate:** only start after `@realtime` and `Fixed64` exist and a Tier 3 domain
has a concrete user.

---

### C10 — Autodiff: `@adjoint`, `@differentiable`, `@differentiable_implicit` `[ ]`

**Why.** This is the one genuinely new compiler feature and the gate to Tier 2
(differentiable simulation, SciML). It is also the single largest strategic bet
in this document.

**Design constraints (must be decided before implementation).**

- **Reverse mode only for v1.** Forward mode can be a library later.
- **Tape lives in a region.** The adjoint tape is an arena-backed buffer with a
  `@bounded` obligation; this is what makes AD memory deterministic and unique
  against PyTorch/JAX.
- **Checkpointing.** `@adjoint` functions can recompute instead of storing; the
  recompute policy is part of the annotation.
- **Implicit differentiation.** `@differentiable_implicit` wraps a solver
  (root-finding, ODE step) and differentiates through the implicit function
  theorem, not the unrolled loop.
- **Composition.** Differentiable regions, device-resident adjoints, and
  `Float32`/`Float64` both supported; AVR is out of scope for v1.

**Milestones.**

1. Scalar reverse-mode tape for a fixed op set (`+ - * / exp log sin cos pow
   matmul`), testable against finite differences.
2. `@differentiable` functions and a `grad` entry point.
3. Region-backed tape with a compile-time peak bound and checkpointing.
4. `@differentiable_implicit` for one solver (Newton root-find; then RK4).
5. `voptim` integration (Adam/SGD) and a training loop that fits in a region.

**Exit criteria.** A 2-layer network trains to the same loss as the `vml` path
within tolerance; gradient checks pass; tape peak memory is bounded and
reported; an ODE-adjoint example matches finite differences for a small Lorenz
integration.

**Effort.** 10–16 ew. **Risk.** High. **Do not start before C4/C5 and a stable
`voptim` exist.**

---

### C11 — Certification and evidence pipeline `[ ]`

**Why.** This is the moat's commercial expression. `@bounded`, `@deterministic`,
`@zeroed`, and `@invariant` are only valuable to regulated users if we can
export evidence.

**Work.** A `vynec certify` (or ruleset-driven) mode that emits:

- Region bound declarations and their runtime counters.
- A contract/traceability log (requirement → function → `@invariant` → test).
- Determinism manifest (no wall-clock, no entropy, seed pinned).
- Build reproducibility record (toolchain, flags, hashes).
- Coverage/MC-DC report hooks (external tool integration).

Start with a **Kalman-filter case study** and a written mapping to DO-178C /
ISO 26262 / IEC 62304 evidence objectives. This does not require claiming
certification — it requires producing the artifacts an assessor asks for.

**Exit criteria.** A `docs/certification/kalman.md` case study with a generated
evidence bundle and a gap analysis against one standard.

**Effort.** 8–12 ew ongoing. **Risk.** Medium (process, not code).

---

### C12 — ARM Cortex-M backend `[ ]`

**Why.** The Uno is a teaching target. Serious robotics, motor control, and
audio need Cortex-M4/M7 (STM32, RP2040, nRF52). The pitch in this document
("deploy to the STM32 controlling the drone") requires this backend.

**Work.** A second MCU runtime modelled on `vyne/runtime/arduino/` but for
Cortex-M: newlib/arm-none-eabi, hardware FPU (`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`),
optional I2S/ADC/DMA hooks, and a board descriptor (`boards/stm32f4.toml`).
Keep the emitter contract identical: `vyne_setup`/`vyne_loop` and the same
`VyneValue` layout.

**Exit criteria.** Blinky + serial + `@bounded` on an STM32F4 Discovery or
RP2040; typed arrays and `vcontrol.Pid` run; the same `.vy` source builds for
Uno and STM32 with only a `--board` change.

**Effort.** 8–12 ew. **Risk.** High (toolchain + board bring-up).
**Gate:** start after C1 and a real robotics/audio user exists.

---

### C13 — Tooling: telemetry, profiler, trace `[ ]`

**Why.** Determinism is only credible if observable. We need a consistent,
low-overhead telemetry surface across targets.

**Work.** Unify arena/deadline/stream/device counters under one `vcore.telemetry`
API; add a chrome-trace or CSV emitter for region timing; add a `vynec prof`
that reads the trace; keep AVR telemetry serial-only and optional.

**Exit criteria.** A region-timing trace from desktop and AVR; documented
overhead budget.

**Effort.** 4–8 ew. **Risk.** Low.

---

### Capability dependency graph

```
C4 vsignal ──┬──> C5 vcontrol ──> Robotics demos
             ├──> Audio DSP
             └──> Industrial sensing

C1 I2C/SPI ──┬──> Robotics sensors/motors
             ├──> Industrial sensing
             └──> Embedded audio codecs

C2 @realtime ─┬──> Robotics control loops
              └──> Audio callbacks

C3 @invariant ───> C11 certification evidence

C7 WASM ─────────> Browser demos / inference

C8 Fixed64 ──────> Determinism across targets
               └──> Tier 3 (HFT, contracts, lockstep)

C10 Autodiff ────> Differentiable simulation
               └──> SciML / PINNs ──> C4/C5/voptim/vsparse/vdecomp/vnum

C12 ARM backend ─> Serious robotics + embedded audio
C9 concurrency ──> HFT (deferred)
C13 tooling ─────> all evidence claims
```

---

## 3. Tier 1 — The natural expansions

### D1 — Industrial IoT & embedded sensing — **NOW**

**Positioning.** "Write the sensor pipeline once in Vyne; it runs on the Uno
today, the STM32 tomorrow, and the desktop simulator the day before."

**Why it fits.** It exercises all four moat properties at once: `@bounded` for
SRAM, `@deterministic` for repeatable readings, `vstat`/`vsignal` for filtering,
and one source across targets.

**Beachhead.** The shipped `embedded` template (blinky + `@bounded<512>` +
serial) is the starting point. The weekend demo is the first milestone below.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| I0 | `examples/arduino/sensor_stream.vy` | Reads A0, applies moving average, prints over serial at 115200; arena 0 B after rewind | 0.5–1 ew | — |
| I1 | `vsignal` moving-average/filter slice | `vsignal_test` passes on desktop + Uno | 1–2 ew | C4 |
| I2 | `embedded` template grows a sensor variant | `vynec init --kind embedded --sensor` generates a working project | 1 ew | I0 |
| I3 | I2C sensor path | MPU-6050/WHO_AM_I over I2C on Uno | 1–2 ew | C1 |
| I4 | Serial telemetry format + host plot | `vtermplot`/CSV output from logged serial data | 1–2 ew | I0, C13 |
| I5 | Industrial case study | A documented temperature/vibration monitor with bounds evidence | 2–3 ew | I1–I4, C11 |

**Kill criterion.** If I0 cannot be made deterministic (run-to-run identical
filtered output for identical input) without heroics, the sensing story is
weaker than claimed and D1 drops to "Next".

---

### D2 — Robotics & autonomous systems — **NOW (after sensing)**

**Positioning.** "One Kalman filter. Simulated on the desktop, bounded on the
STM32, never a GC pause."

**Why it fits.** Real-time control cannot tolerate GC or hidden allocation;
`@bounded` and `@realtime` are the differentiators. Sensors and motors need
C1. Serious deployments need C12 (ARM), not the Uno.

**First credible demo.** A 1-D or 2-D balancing rig (cart-pole simulation first,
then a real motor with an encoder) running a PID and a complementary/Kalman
filter, with a `@bounded` step and a `@realtime` deadline.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| R0 | `vcontrol.Pid` + plant simulation | Step response matches reference; compiles for desktop + AVR | 1–2 ew | C5 |
| R1 | `vcontrol.ComplementaryFilter`, `Kalman1D` | Filtered angle tracks ground truth in sim | 2–3 ew | C5, C4 |
| R2 | `@realtime` MVP | Deadline overrun halts predictably; telemetry reports worst case | 3–4 ew | C2 |
| R3 | Sensor + motor demo on Uno | Closed-loop control at a fixed rate; drift == 0 after rewind | 2–4 ew | C1, R0–R2 |
| R4 | ARM Cortex-M port of the demo | Same `.vy`, `--board stm32f4`, hardware FPU | 4–6 ew | C12 |
| R5 | Small-matrix Kalman | `vdecomp`/`vlin`-backed; numeric check vs reference | 3–4 ew | C5, `vdecomp` |
| R6 | Differentiable robotics (gated) | Policy trained by backprop through a bounded sim | 6–10 ew | C10, R1 |

**Kill criterion.** If `@realtime` telemetry cannot distinguish a well-behaved
loop from a badly-behaved one, the certification pitch is not ready; keep R0–R3
as demos and defer R6.

---

### D3 — Real-time audio & DDSP — **NEXT**

**Positioning.** "Train a DDSP synthesizer on a GPU; deploy the same inference
code to an embedded audio board with zero GC pauses."

**Why it fits.** `vfft`, `vcomplex`, and `@ring` are already present. Audio is
the most demanding determinism showcase (dropouts are audible and measurable).

**Non-goals for v1.** DAW integration, plugin formats (VST/AU), MIDI editing,
low-latency ASIO at launch. Start offline, then streaming, then real-time.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| A0 | `vaudio` WAV read/write + offline graph | Round-trip PCM16/float32; deterministic render | 2–3 ew | C6 |
| A1 | `vsignal` biquad/FIR/FFT blocks | Filter response matches reference; 1 M-sample bench | 2–3 ew | C4 |
| A2 | `@ring`-backed streaming graph | Block processing with bounded memory; no allocation in the audio path | 2 ew | `@ring`, C2 |
| A3 | Real-time backend (WASAPI/ALSA first) | Sustained playback without dropouts under a stated buffer size | 4–6 ew | C6, C2 |
| A4 | DDSP inference demo | Same model runs offline and in the streaming graph | 4–6 ew | A0–A3, `vml`/AD for training |
| A5 | Embedded audio (Cortex-M + I2S) | A filter runs on hardware at a target sample rate | 6–10 ew | C12, C1 |

**Kill criterion.** If A2 cannot hold a bounded memory profile under sustained
load, the "no GC" pitch for audio is not yet demonstrable; stop before A4.

---

### D4 — WebAssembly target — **NOW**

**Positioning.** "Vyne in the browser: deterministic numeric code and on-device
inference without Pyodide's weight or JavaScript's memory model."

**Why it fits.** It is a packaging exercise on the existing C emitter, not a new
compiler. It multiplies the reach of every demo (ESPCN, Lorenz, plots) and gives
the project a shareable artifact.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| W0 | `--target wasm` hello-world | `.wasm` builds and prints via a JS shim | 2–3 ew | C7 |
| W1 | Runtime shims (`vfs`, time, memory) | File read/write and timing work in the harness | 2 ew | W0 |
| W2 | ESPCN or Lorenz in the browser | Output matches native within tolerance | 2–3 ew | W1 |
| W3 | Size/startup budgets in CI | Regressions fail CI; budgets documented | 1–2 ew | W2 |
| W4 | Browser demo page | A static page that loads and runs a model | 1–2 ew | W2 |

**Kill criterion.** If W0 requires more than one new runtime abstraction beyond
shims, re-scope: WASM is only cheap; if it is not cheap, defer.

---

## 4. Tier 2 — The strategic bets (autodiff-gated)

### D5 — Differentiable simulation & physics — **LATER**

**Positioning.** "Backpropagate through a physically accurate, region-bounded
simulation — with a compile-time memory ceiling."

**Why it fits.** This is the flagship use of C10: neural ODEs, differentiable
rigid-body/contact simulation, and RL environments that are deterministic by
construction.

**Prerequisites.** C10 (reverse mode + checkpointing + implicit diff), C5
(control), `vdecomp`/`vnum` (solvers), and a bounded tape.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| S0 | Differentiable ODE step (RK4) | Gradient matches finite differences | 4–6 ew | C10, `vnum` |
| S1 | Neural ODE on Lorenz | Trains; tape peak reported | 4–6 ew | S0 |
| S2 | Differentiable rigid-body toy | Gradient through a 2-body step | 6–10 ew | S0, `vdecomp` |
| S3 | Differentiable robotics demo | Policy trained through the sim from R1 | 6–10 ew | S2, R1 |

### D6 — Scientific ML (PINNs) — **NEXT (AD-gated)**

**Positioning.** "PyTorch for text; Vyne for PDEs."

**Why it fits.** Float64 precision, complex numbers, FFT, deterministic
reduction order, and a bounded training loop are exactly what PINNs and
spectral solvers want.

**Prerequisites.** `voptim`, `vsparse`, `vdecomp`, `vnum`, and C10.

#### Milestones

| ID | Deliverable | Exit criteria | Effort | Deps |
| --- | --- | --- | ---: | --- |
| P0 | PINN on 1-D Poisson/Burgers | Matches a reference solution | 4–6 ew | C10, `voptim`, `vnum` |
| P1 | Spectral solver + neural closure | Stable rollout vs reference | 6–10 ew | P0, `vsparse`, `vfft` |
| P2 | 2-D Navier-Stokes toy | Documented accuracy/latency envelope | 8–12 ew | P1 |

---

## 5. Tier 3 — Stretch goals (defer until evidence exists)

These are explicitly **not** on the near-term roadmap. They are recorded so we
do not accidentally build for them.

| Domain | What it needs from the language | Why it is deferred | Revisit when |
| --- | --- | --- | --- |
| HFT / quant | `vnet`, thread-local arenas, lock-free queues, hugepages, `Fixed64`, tail-latency harness | C++/Rust incumbents; needs a latency proof we do not have | After C8, C9, and a measured p99 parity benchmark |
| Smart contracts | `Fixed64`, gas metering, deterministic runtime, EVM/eWASM backend, sandbox | Solidity/Rust incumbents; backend is a large new target | After C8 and a concrete eWASM/EVM partnership |
| Game engines / lockstep | `Fixed64`, graphics bindings, `vnet`, asset pipeline | C++/C# incumbents; weak fit with the numerical moat | After C8 and a lockstep prototype |

---

## 6. Release sequencing (proposed, not committed)

| Release | Theme | Capabilities | Domains | Exit gate |
| --- | --- | --- | --- | --- |
| **v0.7.0** | Sensing & Control | C1, C2 (MVP), C3, C4, C5, C13 (telemetry) | D1 (I0–I4), D2 (R0–R3) | Sensor demo + PID on Uno; `vsignal`/`vcontrol` shipped |
| **v0.8.0** | Audio & Browser | C6 (phase 1–2), C7 | D3 (A0–A2), D4 (W0–W3) | Streaming audio demo; ESPCN in browser |
| **v0.9.0** | Autodiff | C10 (steps 1–3), `voptim` | AD tests, differentiable ODE | Gradient checks pass; bounded tape |
| **v1.0.0** | SciML & Evidence | C10 (steps 4–5), C11, `vsparse`/`vdecomp`/`vnum` | D5 (S0–S3), D6 (P0–P2) | Neural ODE + PINN + certification bundle |
| **post-1.0** | Tier 3 evaluation | C8, C9, C12 (as needed) | HFT/contracts/lockstep | Only with a concrete partner/user |

The one dependency that cannot be reordered: **C10 (autodiff) must not start
before `vsignal`, `vcontrol`, `voptim`, and at least one solver library exist.**
AD without a numerical ecosystem is a feature nobody can use.

---

## 7. The next move (this week) — milestone I0

This is the cheapest possible proof of the entire thesis. Do it before any of
the larger work.

1. `vynec init --kind embedded` (or the interactive questionnaire).
2. Flash the blinky to a real Arduino Uno; confirm serial output.
3. Change the pin to `A0`, read an analog sensor.
4. Apply a moving average (start with a hand-written loop; move it into
   `vsignal` once it exists).
5. Wrap the loop in `@bounded<512>` and `@deterministic`.
6. Print `raw`, `filtered`, and `arena_used()` over serial at 115200.
7. Verify: identical input sequence → identical filtered output; `drift == 0 B`
   after the region.

**Acceptance criteria.**

- `examples/arduino/sensor_stream.vy` builds with `vynec --arduino uno --flash`.
- Serial output is stable across boots for the same input.
- Arena usage stays under the bound and rewinds to 0.
- A short README section and a `vtermplot`/CSV host script plot the stream.

This single file becomes the seed for the embedded sensor template (I2) and the
industrial case study (I5). It is also the first honest answer to "what can I
build with Vyne on a microcontroller today?"

---

## 8. Anti-goals

Do **not** build or market these. They dilute the moat and lose to incumbents.

- Web servers, HTTP frameworks, REST/GraphQL tooling.
- Desktop or mobile GUIs, windowing, widget toolkits.
- Text processing, parsing DSLs, templating languages (beyond `vynec init`).
- General-purpose scripting or "Python replacement" positioning.
- Package-manager/ecosystem plays before the numerical libraries are complete.
- Game engines, graphics pipelines, shader languages.
- Smart-contract VMs before `Fixed64` and a concrete chain partnership.

A domain may be revisited once it clears the Section 1 gate with a score ≥ 3.0
and has a named user or partner.

---

## 9. Metrics and evidence

Every milestone should produce at least one of:

- **Correctness:** numeric agreement with a reference (tol/threshold stated).
- **Determinism:** run-to-run bit-identical output under `@deterministic`.
- **Memory:** compile-time bound + runtime peak + post-rewind drift = 0.
- **Latency:** worst-case and p99 for `@realtime` regions.
- **Portability:** same source builds and runs on ≥ 2 targets.
- **Size:** `.wasm`/firmware size budget per demo.

Track these in a `docs/evidence/` table per release. If a milestone cannot state
its evidence, it is not done.

---

## 10. Open decisions (need a call before implementation)

1. **First MCU after AVR:** STM32F4 (broad industry, FPU, I2S/ADC) vs RP2040
   (cheap, dual-core, PIO) vs nRF52 (BLE). Recommend STM32F4 for robotics/audio.
2. **`@realtime` semantics:** watchdog-only first (recommended), or block on
   static WCET from day one?
3. **Autodiff scope for v1:** reverse mode only? Which ops are mandatory?
   Is a region-backed tape a hard requirement or an optimization?
4. **Audio v1:** offline WAV + streaming graph only (recommended), or commit to
   a real-time backend immediately?
5. **WASM runtime:** minimal custom shims vs Emscripten. Recommend minimal shims
   to keep the output small and the semantics explicit.
6. **Certification target:** DO-178C (aerospace) vs ISO 26262 (automotive) vs
   IEC 62304 (medical). Pick one for the first case study; the artifacts differ.
7. **`Fixed64` rounding:** Q32.32 multiply/divide rounding mode must be
   specified and frozen before any lockstep/HFT work.
8. **Thread model:** OS threads with thread-local arenas vs an actor/queue model.
   Affects `vnet` and HFT.

---

## 11. Appendix — domain × capability matrix

`●` = required, `○` = useful, blank = not needed.

| | C1 I2C/SPI | C2 realtime | C3 invariants | C4 vsignal | C5 vcontrol | C6 vaudio | C7 WASM | C8 Fixed64 | C9 threads | C10 AD | C11 cert | C12 ARM |
| --- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| D1 Industrial sensing | ● | ○ | ● | ● | ○ | | ○ | ○ | | | ● | ○ |
| D2 Robotics | ● | ● | ● | ● | ● | | | ○ | ○ | ○ | ● | ● |
| D3 Audio / DDSP | ○ | ● | ○ | ● | | ● | ○ | | | ○ | ○ | ● |
| D4 WASM | | ○ | ○ | ○ | | | ● | ○ | | | ○ | |
| D5 Diff. simulation | | ○ | ● | ● | ● | | | | ○ | ● | ● | |
| D6 SciML | | | ● | ● | ○ | | ○ | | ○ | ● | ● | |
| HFT | | ● | ● | ○ | | | | ● | ● | | ● | |
| Smart contracts | | | ● | | | | ○ | ● | | | ● | |
| Game lockstep | | ● | ○ | ○ | | ○ | ○ | ● | ● | | ○ | |
