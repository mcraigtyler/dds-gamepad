# Task 282 – Yoke Declination: Dual-Analog Axis Conflict

**Decision:** Option 3 (Additive/Sum mode) — implemented.

## Context

`gunner.yaml` maps the turret's pitch (up/down movement) using two separate
`Gamepad_Analog` DDS sources — one per direction — both targeting `axis:left_y`:

```yaml
- name: turret_pitch_up
  dds:
    topic: "Gamepad_Analog"
    id: 2
    field: value
  gamepad:
    to: axis:left_y
    invert: false

- name: turret_pitch_down
  dds:
    topic: "Gamepad_Analog"
    id: 1
    field: value
  gamepad:
    to: axis:left_y
    invert: true
```

This pattern is necessary because the physical hardware (yoke/joystick) exposes up
and down deflection as two independent analog sensors rather than a single signed axis.

---

## Bug

Both DDS streams emit values continuously. When the user pushes the yoke downward:

- `id=1` (DOWN) stream sends, e.g., `0.7`
- `id=2` (UP) stream simultaneously sends `0.0` (no deflection)

Since both messages share the `Gamepad_Analog` topic, they are consumed by the same
`AnalogHandler` in a single `reader.take()` call
([AppRunner.cpp:244–285](../../src/app/AppRunner.cpp)). The handler processes each
sample sequentially and calls `MappingEngine::Apply()` for each one. `Apply()` writes
directly to `GamepadState` on every match — the last write wins.

The result is non-deterministic: whichever of `id=1` or `id=2` happens to be last in
the DDS sample queue overwrites the other. When the UP stream's `0.0` arrives after the
DOWN stream's `0.7`, the axis is silently reset to zero mid-deflection.

### Root Cause in Code

`MappingEngine::Apply()` ([MappingEngine.cpp:48–191](../../src/mapper/MappingEngine.cpp))
always overwrites `GamepadState`:

```cpp
// Line 121-122
case ControlTarget::LeftStickY:
    state.left_stick_y = AxisFromNormalized(mapped_value);  // unconditional overwrite
```

There is no mechanism to accumulate contributions from multiple mappings that share
the same output target.

---

## Options Considered

### Option 1 — Skip write when in deadzone

Modify the processing path so that when a value falls within the deadzone, the write to
`GamepadState` is skipped entirely (rather than zeroing it out, as happens today in
[MappingEngine.cpp:19–27](../../src/mapper/MappingEngine.cpp)).

**How it would work:** A sentinel return from `ApplyDeadzone` (or an early `continue`
in the caller) prevents writing when the value is sub-threshold.

**Problems:**
- When the user releases the yoke and both streams return to `0.0`, neither write fires.
  The axis is permanently frozen at its last non-zero value.
- Works only while at least one stream is reliably non-zero; breaks at rest.

**Verdict: rejected** — the zero-return case is fundamentally broken.

---

### Option 2 — Compete group / max-abs selection

Add a `compete_group` string field to `MappingDefinition`. Mappings sharing a group
name and a target axis participate in a "tournament": at the end of each read batch, the
mapping with the highest absolute value wins and is written to state.

**How it would work:**

1. `MappingDefinition` gains `std::string compete_group`.
2. `ConfigLoader` parses `compete_group` from the YAML `gamepad` section.
3. `MappingEngine::Apply()` buffers computed values for compete-group mappings instead of
   writing immediately.
4. A new `MappingEngine::FlushCompeteGroups(GamepadState&)` picks the max-abs winner per
   group/target pair and writes it.
5. `ProcessAnalogSamples()` in AppRunner calls `FlushCompeteGroups` after the sample loop.

**Pros:**
- Semantically explicit in config — the competition relationship is visible.
- Handles every edge case correctly: both-zero → 0; one active → that value; both
  non-zero → dominant value wins.
- Does not affect non-grouped mappings.

**Cons:**
- Introduces per-batch mutable state into `MappingEngine` (breaks its current
  stateless/`const` `Apply()` design).
- Requires changes in four files plus YAML.
- The "dominant wins" semantics are slightly surprising if both sensors are simultaneously
  non-zero (e.g., during a mechanical transition).

**Verdict: viable, but more complex than necessary for this hardware.**

---

### Option 3 — Additive / sum mode (recommended)

Add an `additive: true` flag to the YAML `gamepad` section for mappings that should
contribute to a shared axis via summation rather than replacement.

**Physical intuition:** The hardware has two unidirectional sensors:

```
axis_value = sensor_up(id=2) + ( −sensor_down(id=1) )
           = 0.0             + ( −0.7              )   →  −0.7  (pitching down)
           = 0.5             + (  0.0              )   →  +0.5  (pitching up)
           = 0.0             + (  0.0              )   →   0.0  (centered)
```

The `invert: true` on the DOWN mapping already negates its contribution; summing the two
normalized values produces the correct signed axis value in all cases.

**How it would work:**

1. **`MappingDefinition`** — add `bool additive = false`.
2. **`ConfigLoader`** — parse `additive` from the YAML `gamepad` node
   ([ConfigLoader.cpp:208–210](../../src/config/ConfigLoader.cpp) is where `scale`,
   `deadzone`, `invert` are parsed; `additive` slots in beside them).
3. **`MappingEngine`** — two additions:
   - `ResetAdditiveTargets(GamepadState& state)` — zeros out only those
     `GamepadState` fields that have at least one additive mapping targeting them.
     Called once before a read batch begins.
   - In `Apply()`, the second `switch` ([MappingEngine.cpp:111–186](../../src/mapper/MappingEngine.cpp))
     uses `+=` (accumulated) instead of `=` (replacement) for additive mappings:
     ```cpp
     case ControlTarget::LeftStickY:
         if (mapping.additive)
             state.left_stick_y += AxisFromNormalized(mapped_value);  // accumulate
         else
             state.left_stick_y  = AxisFromNormalized(mapped_value);  // replace (current)
         break;
     ```
     *(Clamping after accumulation must happen in `ResetAdditiveTargets` or in a new
     `ClampAdditiveTargets` pass after `Apply()` finishes.)*
4. **`AppRunner::ProcessAnalogSamples`** — call `engine.ResetAdditiveTargets(state)`
   before line 245 (the sample loop). The rest of the function is unchanged.
5. **`config/gunner.yaml`** — add `additive: true` to both turret pitch mappings.

**Pros:**
- Correct for split-sensor hardware by construction.
- Zero case works naturally: both sensors at `0.0` → accumulated sum is `0.0`.
- Minimal code surface — no new mutable engine state, no per-batch flush step.
- Non-additive mappings are completely unaffected.

**Cons:**
- If the physical sensors are simultaneously non-zero (e.g., a noisy transition),
  contributions partially cancel. This is the physically correct outcome for a mechanical
  joystick but may produce a brief "dead" moment at direction reversal.
- The `+=` on `int16_t` can theoretically overflow if both sensors are near maximum
  simultaneously; clamping after accumulation is required.

**Verdict: recommended.** Matches the physics, handles zero correctly, least code change.

---

### Option 4 — Compound / multi-source mapping

Replace the two separate mapping entries with a single YAML entry that lists multiple DDS
sources and a combine rule:

```yaml
- name: turret_pitch
  dds:
    sources:
      - topic: "Gamepad_Analog"
        id: 2
        field: value
        invert: false
      - topic: "Gamepad_Analog"
        id: 1
        field: value
        invert: true
    combine: sum
  gamepad:
    to: axis:left_y
    scale: 1.0
    deadzone: 0.06
```

**Pros:** Most expressive; zero-case correct; no interaction between otherwise-independent
mappings; clean config schema.

**Cons:** Significant schema and code change — `ConfigLoader`, `AppConfig`, handler
construction, and `MappingEngine` all need rework. Premature abstraction for a single
known use case.

**Verdict: best long-term design, but too much churn for this bug fix.**

---

## Recommended Implementation: Option 3 (Additive Sum Mode)

### Files to change

| File | Change |
|------|--------|
| [src/mapper/MappingEngine.h](../../src/mapper/MappingEngine.h) | Add `bool additive = false` to `MappingDefinition`; declare `ResetAdditiveTargets` |
| [src/mapper/MappingEngine.cpp](../../src/mapper/MappingEngine.cpp) | Implement `ResetAdditiveTargets`; add `+=` path in `Apply()` write switch; clamp additive axes |
| [src/config/ConfigLoader.cpp](../../src/config/ConfigLoader.cpp) | Parse `additive` from YAML gamepad node alongside `scale`/`deadzone`/`invert` |
| [src/app/AppRunner.cpp](../../src/app/AppRunner.cpp) | Call `engine.ResetAdditiveTargets(state)` before the sample loop in `ProcessAnalogSamples` (line 244) |
| [config/gunner.yaml](../../config/gunner.yaml) | Add `additive: true` to `turret_pitch_up` and `turret_pitch_down` |

### Implementation notes

- `ResetAdditiveTargets` should iterate `mappings_` to discover which targets have any
  `additive` mapping, then zero only those fields in `GamepadState`. This avoids
  accidentally clearing axes that are written by other (non-additive) handlers.
- The `+=` accumulation operates on the normalized float (`mapped_value`) before
  conversion to `int16_t`, to avoid integer overflow. Alternatively, accumulate on a
  temporary `float` per target and convert once at the end.
- `ProcessStickSamples` does not need changes for this bug; the turret yaw (stick X) is
  a single mapping on a different topic/handler.

### Verification

1. Build Debug: `cmake --build .\build --config Debug --target install`
2. Run: `.\install\dds-gamepad\bin\dds-gamepad.exe config\gunner.yaml 0 <yoke_id> --table`
3. Deflect yoke downward → `axis:left_y` moves negative, holds steadily while held.
4. Release yoke → `axis:left_y` returns to `0`.
5. Deflect upward → `axis:left_y` moves positive.
6. Verify no interference between UP/DOWN streams when idle (table should show 0).
