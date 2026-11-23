# M06 — Compact game runtime, interactions, and audio

**Depends on:** M01; M02 for environment actions. **Status:** planned.

**Outcome:** a small game-making layer for atmospheric worlds: registered
behaviors, input actions, usable objects, triggers, audio, and World transitions.

## TODO

### Runtime behavior and input

- [ ] **M06.01** Define a minimal typed behavior API with initialize/update/event/stop
  phases, parameter schema, runtime owners, and deferred entity mutation.
- [ ] **M06.02** Choose explicit compiled behavior registration for the first version;
  document how game code attaches behavior without requiring a new scripting VM.
- [ ] **M06.03** Create bounded behavior/event/timer storage before Play and validate
  behavior IDs and parameters during staged scene loading.
- [ ] **M06.04** Add named input actions for movement, look, crouch, sprint, jump,
  interact, pause, and cursor release, with configurable bindings.
- [ ] **M06.05** Preserve clamped/substepped character motion and authored camera
  semantics; expose gameplay-friendly movement parameters with units.
- [ ] **M06.06** Route UI/captured gameplay input once per frame and define pause
  behavior for timers, animation, physics, and audio.

### Interaction and events

- [ ] **M06.07** Add ray-based interaction targeting with range, mask, and visible
  prompt; use the World query interface.
- [ ] **M06.08** Implement a configurable door/lever behavior with transform motion,
  interaction events, and matching collision proxy updates.
- [ ] **M06.09** Add bounded trigger volumes with enter/stay/exit events, stable IDs,
  and edge-triggered dispatch during repeated overlap frames.
- [ ] **M06.10** Add simple actions: play sound, change light, request environment
  override, show message, enable entity, or request World transition.
- [ ] **M06.11** Add an editable transform/light/effect-parameter timeline sufficient
  for door motion, flicker, and scripted surreal events.
- [ ] **M06.12** Define trigger/interaction behavior after teleport, respawn, entity
  deletion, undo in authoring, and repeated Play/Stop.

### Audio and transitions

- [ ] **M06.13** Add checked project-owned sound/stream resources with portable
  references, volume, looping, and supported-format diagnostics.
- [ ] **M06.14** Implement a simple positional-audio layer: listener transform,
  distance attenuation, panning, bounded voice allocation, and priorities.
- [ ] **M06.15** Keep audio optional for CPU/headless fixtures and provide a testable
  event path when an audio device is unavailable.
- [ ] **M06.16** Stage destination World/assets before a transition; report failures
  and retain the active scene when loading fails.
- [ ] **M06.17** Define spawn markers, fade timing, retained gameplay state, and event
  cancellation when World switching completes.
- [ ] **M06.18** Add explicit save-game/checkpoint serialization for a small selected
  gameplay state set; keep it separate from authored scene persistence.

### Authoring and launch

- [ ] **M06.19** Add behavior/trigger/audio authoring controls with validated fields,
  defaults, undo, project references, and preview actions.
- [ ] **M06.20** Create a standalone authored-scene runtime launcher that starts the
  configured game World and shares the Studio Play path; distinguish it from legacy DAT play.
- [ ] **M06.21** Resolve project assets relative to the packaged project root and
  make starting-scene/input/quality settings explicit.
- [ ] **M06.22** Close behaviors, voices, streams, World targets, and assets safely
  during Stop, transition failure, window close, and runtime exceptions.

## Acceptance

- [ ] A player approaches and opens a door, triggers a sound/light event, and moves
  between two rooms without editor UI or authoring mutations.
- [ ] Trigger enter/exit events occur once at the correct boundaries.
- [ ] Door motion and collision remain aligned and cannot trap stale references.
- [ ] Missing audio/World assets produce actionable errors and retain usable runtime state.
- [ ] Pausing, respawn, transitions, and repeated Play/Stop leave no live-resource drift.
- [ ] The standalone launcher and Studio Play produce equivalent gameplay behavior.

## Implementation touchpoints

`src/scene/play.vy`, character/world motion modules, `src/engine/input.vy`,
scene catalog/schema/IO, managed audio wrappers, and new bounded behavior/trigger
modules. Keep sample game behavior separate from reusable runtime orchestration.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |
