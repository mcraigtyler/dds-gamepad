# Refactor: Multi-Backend Output Device Support

**Date:** 2026-02-25
**Status:** Planning

---

## 1. Problem Statement

The current output layer is tightly coupled to a single backend: the Xbox 360 virtual
controller exposed via ViGEmBus. Three layers encode this assumption:

| Layer | Hard-coded assumption |
|---|---|
| `common/MappingDefinition.h` | `ControlTarget` enum lists Xbox 360 channels (`LeftStickX`, `ButtonA`, etc.) |
| `mapper/MappingEngine.cpp` | Writes directly to `mapper::GamepadState` (Xbox 360 wire types: `int16_t`, `uint8_t`) |
| `emulator/VigemClient.h` | `IVigemClient::UpdateState(const GamepadState&)` — interface and state type are Xbox 360-only |

To add a UDP + protobuf output (or any other backend), all three layers must be generalised
without breaking the existing ViGEm path.

---

## 2. Current Data Flow

```
DDS topics
  → AppRunner (TopicHandler<MsgT> / ProcessSamples)
    → MappingEngine::Apply(field, id, raw_value, GamepadState&)
      → GamepadState  (int16 sticks, uint8 triggers, uint16 buttons bitmask)
        → IVigemClient::UpdateState(GamepadState)
          → ViGEmBus → virtual Xbox 360 HID device
```

The `ControlTarget` enum (in `common/MappingDefinition.h`) drives `MappingEngine::Apply()`,
and the enum values map 1:1 to `GamepadState` fields. Adding a new backend requires
inserting new enum values and new `GamepadState` fields — the wrong direction.

---

## 3. Target Data Flow

Both backends follow the same pipeline up to `UpdateState()`:

```
DDS topics (id + value)
  → AppRunner (TopicHandler<MsgT> / ProcessSamples)           [unchanged]
    → MappingEngine::Apply(field, id, raw_value, OutputState&) [generalised]
      → OutputState  { map<string, float> channels }           [new]
        → IOutputDevice::UpdateState(OutputState)              [new interface]
          ┌─ VigemEmulator   → XUSB_REPORT → ViGEmBus → Xbox 360 HID
          └─ UdpProtobufEmulator → protobuf message → UDP socket
```

The key principle: **`output.to` in the YAML is the output channel name, full stop.**
For the ViGEm backend it is an Xbox 360 channel identifier (`axis:right_x`).
For the UDP backend it is a protobuf field name (`steering`). There is no intermediate
translation or channel map — the `MappingEngine` writes the value under that name into
`OutputState`, and the backend reads it back out by the same name.

---

## 4. Design Goals

1. **No regression** — existing ViGEm path must continue working identically.
2. **Backend-agnostic mapping layer** — `MappingEngine` produces normalized floats keyed
   by the string from `output.to`; it knows nothing about Xbox 360 or protobuf.
3. **Config-driven backend selection** — the YAML `output:` section selects the backend
   and provides its parameters (UDP host/port); individual mappings name their target
   field via `output.to` and work the same regardless of backend.
4. **Independent backend implementations** — adding a new backend requires only a new
   `IOutputDevice` class and config parsing; no changes to AppRunner, MappingEngine,
   or ConfigLoader.
5. **Hot-path efficiency** — `UpdateState()` is called on every mapped DDS sample; avoid
   unnecessary heap allocations on that path.

---

## 5. Core Design Change: Generic Output State

Replace `mapper::GamepadState` as the cross-layer contract with a generic
**named-channel float map**:

```cpp
// src/common/OutputState.h
namespace common {

struct OutputState {
    // Normalized float values keyed by the YAML output.to string.
    // The value range is determined by channel type (see MappingDefinition::channelType):
    //   Axis:    -1.0 .. +1.0
    //   Trigger:  0.0 .. +1.0
    //   Button:   0.0 or 1.0
    std::unordered_map<std::string, float> channels;
};

}  // namespace common
```

`MappingEngine::Apply()` writes the normalized float to `OutputState::channels[target]`.
Each `IOutputDevice` reads the channels it knows about and converts to its wire format:

- `VigemEmulator` maps `"axis:left_x"` → `XUSB_REPORT.sThumbLX` (after int16 scaling),
  `"axis:left_trigger"` → `XUSB_REPORT.bLeftTrigger` (after uint8 scaling), etc.
- `UdpProtobufEmulator` sets each protobuf field by reading `channels["steering"]`,
  `channels["throttle"]`, etc. — the YAML `output.to` values **are** the protobuf field names.

---

## 6. Interface Changes

### 6.1 Rename and generalise `IVigemClient` → `IOutputDevice`

```cpp
// src/emulator/IOutputDevice.h
namespace emulator {

class IOutputDevice {
public:
    virtual ~IOutputDevice() = default;
    // Startup — throw std::runtime_error on failure.
    virtual void Connect() = 0;
    // Hot-path update — returns false on failure; inspect LastError().
    virtual bool UpdateState(const common::OutputState& state) = 0;
    virtual std::string LastError() const = 0;
    // Optional hook (default no-op).
    virtual void SetLogState(bool) {}
};

}  // namespace emulator
```

`ITxStateListener` is narrowly kept inside `VigemEmulator`; it is only used for the
console TX-state display and need not appear on the generic interface.

### 6.2 `VigemEmulator` implements `IOutputDevice`

```cpp
// src/emulator/VigemEmulator.h
class VigemEmulator final : public IOutputDevice {
public:
    void Connect() override;       // vigem_alloc + vigem_connect
    void AddX360Controller();      // vigem_target_x360_alloc + vigem_target_add
    bool UpdateState(const common::OutputState&) override;
    std::string LastError() const override;
    void SetLogState(bool) override;
    void SetTxStateListener(ITxStateListener*);
private:
    // Reads the known axis/trigger/button channel names from OutputState
    // and populates XUSB_REPORT. Unknown channel names are silently ignored.
    XUSB_REPORT BuildReport(const common::OutputState&) const;
    // ... same internal state as VigemClient today
};
```

`BuildReport()` is the only place in the codebase that knows the Xbox 360 channel name
strings (`"axis:left_x"`, `"axis:right_trigger"`, `"button:a"`, etc.) and the
`int16`/`uint8` wire-type conversions. `AxisFromNormalized` and `TriggerFromNormalized`
move here from `MappingEngine`.

### 6.3 `UdpProtobufEmulator` implements `IOutputDevice`

```cpp
// src/emulator/UdpProtobufEmulator.h
struct UdpProtobufConfig {
    std::string host;
    uint16_t port = 0;
};

class UdpProtobufEmulator final : public IOutputDevice {
public:
    explicit UdpProtobufEmulator(UdpProtobufConfig cfg);
    void Connect() override;   // resolve host, open SOCK_DGRAM socket
    bool UpdateState(const common::OutputState& state) override;
    std::string LastError() const override;
};
```

`UpdateState()` iterates `state.channels`, sets the matching protobuf message field by
name, serializes the message, and sends it via UDP. The YAML `output.to` values are the
protobuf field names — there is no additional translation layer.

**Protobuf field name matching**: Use protobuf reflection (`GetDescriptor()` /
`FindFieldByName()`) to set fields by the string key from `OutputState`. This avoids
hand-coding the field list in C++ and means adding a new mapped channel only requires
updating the `.proto` file and the YAML — no C++ changes.

### 6.4 `MappingEngine` writes `OutputState` instead of `GamepadState`

```cpp
// Changed signature:
bool Apply(const std::string& field, int message_id, float value, common::OutputState& state);
```

The body:
- Normalizes `value` to a float in the appropriate range using the new `channelType`
  field on `MappingDefinition` (see §6.5).
- Writes `state.channels[mapping.target] = normalized_value`.
- No longer calls `AxisFromNormalized` / `TriggerFromNormalized` — those produce
  Xbox 360 wire types and belong in `VigemEmulator`.

### 6.5 `ControlTarget` becomes `std::string` + `ChannelType`

Replacing the `ControlTarget` enum with a plain string removes the normalization hint
the engine used to derive from the enum variant (trigger: clamp 0..1; stick: clamp
-1..1). A small explicit `ChannelType` field preserves that behaviour without encoding
any backend-specific knowledge:

```cpp
// src/common/MappingDefinition.h

enum class ChannelType { Axis, Trigger, Button };

struct MappingDefinition {
    std::string name;
    int id = 0;
    std::string field;
    std::string target;           // YAML output.to — channel key and backend field name
    ChannelType channelType = ChannelType::Axis;  // drives normalization range
    float scale = 1.0f;
    float deadzone = 0.0f;
    bool invert = false;
    bool has_input_range = false;
    float input_min = 0.0f;
    float input_max = 1.0f;
    bool additive = false;
};
```

`ConfigLoader` sets `channelType` from the YAML `output.type` field (`axis` / `trigger` /
`button`). For backward compatibility with existing ViGEm configs that use the
`axis:*` / `button:*` channel naming convention, `ConfigLoader` can infer `channelType`
from the `output.to` prefix when `output.type` is absent:

| `output.to` prefix | Inferred `channelType` |
|---|---|
| `axis:left_trigger`, `axis:right_trigger` | `Trigger` |
| `axis:*` | `Axis` |
| `button:*`, `dpad:*` | `Button` |
| anything else (custom field name) | must specify `output.type` explicitly |

`MappingEngine` uses `channelType` instead of the old enum to decide normalization:

```cpp
switch (mapping.channelType) {
    case ChannelType::Trigger:
        mapped = std::clamp(mapped, 0.0f, 1.0f);  break;
    case ChannelType::Axis:
        mapped = std::clamp(mapped, -1.0f, 1.0f); break;
    case ChannelType::Button:
        mapped = (mapped > 0.5f) ? 1.0f : 0.0f;  break;
}
state.channels[mapping.target] = mapped;
```

---

## 7. Config Schema Changes

Add a top-level `output:` section to the role YAML. Rename the per-mapping `gamepad:`
key to `output:`. The `output.to` value names the output channel — it is simultaneously
the key in `OutputState::channels` and the backend's native field name (ViGEm channel
identifier or protobuf field name).

### ViGEm config (new schema, backward-compatible):

```yaml
role:
  name: "Driver"

output:
  type: vigem_x360     # explicit; omitting defaults to vigem_x360

mappings:
  - name: steering
    dds:
      topic: "Gamepad_Stick_TwoAxis"
      type: "Gamepad::Stick_TwoAxis"
      idl_file: "idl/Gamepad.idl"
      id: 30
      field: x
      input_min: -110.0
      input_max: 110.0
    output:               # was: gamepad:
      to: axis:right_x    # ViGEm channel name (unchanged)
      scale: 1.0
      deadzone: 0.02
      invert: false
      # type: axis        # inferred from "axis:*" prefix; explicit for custom channels
```

### UDP + protobuf config:

```yaml
role:
  name: "Driver"

output:
  type: udp_protobuf
  host: 192.168.1.100
  port: 5000

mappings:
  - name: steering
    dds:
      topic: "Gamepad_Stick_TwoAxis"
      type: "Gamepad::Stick_TwoAxis"
      idl_file: "idl/Gamepad.idl"
      id: 30
      field: x
      input_min: -110.0
      input_max: 110.0
    output:
      to: steering      # protobuf field name — used directly, no translation
      type: axis        # required for custom field names (can't be inferred from prefix)
      scale: 1.0
      deadzone: 0.02
      invert: false

  - name: throttle
    dds:
      topic: "Gamepad_Analog"
      type: "Gamepad::Gamepad_Analog"
      idl_file: "idl/Gamepad.idl"
      id: 31
      field: value
    output:
      to: throttle      # protobuf field name
      type: trigger
      scale: 1.0
      deadzone: 0.0
      invert: false
```

No `channel_map` is needed. The `output.to` string in each mapping is used verbatim as
the key in `OutputState::channels` and as the protobuf field name when the UDP backend
calls `FindFieldByName()`.

---

## 8. AppRunner Changes

`AppRunner::Run()` currently constructs `VigemClient` internally and calls `Connect()` +
`AddX360Controller()`. After the refactor:

1. Read `config.output.type` from `RoleConfig`.
2. Construct the appropriate `IOutputDevice` (e.g. `VigemEmulator` or `UdpProtobufEmulator`).
3. Call `device->Connect()`. For ViGEm, `AddX360Controller()` is called inside `Connect()`
   or as a second virtual (`Prepare()`); the goal is one virtual call from AppRunner.
4. Pass `IOutputDevice&` to the read loop, replacing `IVigemClient&`.

The existing overload `AppRunner::Run(options, IVigemClient&, stopToken)` becomes
`AppRunner::Run(options, IOutputDevice&, stopToken)` — the same injection point,
generalised. `main.cpp` and `ServiceMain.cpp` can continue to construct the concrete type
directly, or delegate construction to `AppRunner` based on config.

---

## 9. Phased Implementation Plan

Each phase compiles and passes smoke tests independently.

### Phase 1 — Rename interface (no behaviour change)

1. Create `src/emulator/IOutputDevice.h` with the new interface.
2. `VigemClient` implements `IOutputDevice` (rename class or keep and just rename interface).
3. Update `AppRunner.h/.cpp` to use `IOutputDevice&` instead of `IVigemClient&`.
4. Update `main.cpp` and `ServiceMain.cpp`.

*Diff size: small. No logic changes.*

### Phase 2 — Generalise output state

5. Add `src/common/OutputState.h`.
6. Replace `ControlTarget` enum with `std::string target` + `ChannelType` in
   `MappingDefinition`. Delete the enum.
7. Update `ConfigLoader` to parse `output.to` as a plain string, infer `channelType`
   from prefix for existing ViGEm channels, require explicit `output.type` for custom names.
8. Change `MappingEngine::Apply()` to write `OutputState` using `channelType`-driven
   normalization. Move `AxisFromNormalized` / `TriggerFromNormalized` to `VigemEmulator`.
9. Update `VigemEmulator::UpdateState()` to read `OutputState` channels and build
   `XUSB_REPORT`. `GamepadState` becomes a private concern of `VigemEmulator` or is deleted.

*Diff size: medium. Highest risk phase — thorough testing required on ViGEm path.*

### Phase 3 — Add config schema for output backend

10. Add `output:` section parsing to `ConfigLoader`; add `OutputConfig` struct to
    `RoleConfig`. Parse `type`, `host`, `port`.
11. Update `AppRunner` to construct the right `IOutputDevice` from `config.output.type`.
    Default to `vigem_x360` if `output:` is absent (backward compat).

*Diff size: small-medium. Pure addition.*

### Phase 4 — Implement `UdpProtobufEmulator`

12. Add protobuf dependency (see §10).
13. Define the `.proto` schema for the output message.
14. Implement `src/emulator/UdpProtobufEmulator.h/.cpp`:
    - `Connect()` — resolve host, open UDP socket (`SOCK_DGRAM`, `WSAStartup` on Windows).
    - `UpdateState(OutputState)` — iterate `state.channels`; for each key call
      `descriptor->FindFieldByName(key)` and set the field via reflection; serialize
      and `sendto()`.
    - `LastError()` — surface last socket or serialization error.
15. Register in `CMakeLists.txt`; wire into `AppRunner` backend factory.

*Diff size: medium. Self-contained new file.*

---

## 10. Protobuf Dependency

### Option A — Google protobuf via vcpkg (recommended)

```
vcpkg install protobuf:x64-windows
```

```cmake
find_package(protobuf CONFIG REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/emulator/vehicle_state.proto)
```

Pro: vcpkg already in use; mature library; reflection API available for field-by-name
lookup. Con: libprotobuf is large; adds a DLL to the install bundle.

### Option B — nanopb (lightweight)

Generates minimal C structs from `.proto` files; no runtime DLL.
Con: No reflection API — field-by-name lookup must be hand-coded; requires a Python
build step.

### Option C — Hand-written binary format

Skip `.proto` entirely; define a fixed struct or length-prefixed key-value binary layout.
Simplest to implement, least interoperable.

**Recommendation:** Option A (vcpkg protobuf) — the reflection API makes the
`UdpProtobufEmulator` implementation substantially simpler and avoids hard-coding the
field list in C++.

---

## 11. Open Questions

| # | Question | Impact |
|---|---|---|
| 1 | **What is the `.proto` schema?** Field names, types (float/double/int?), message name. | Determines `vehicle_state.proto` and the protobuf dependency choice |
| 2 | **Are both outputs active simultaneously?** E.g., one config that drives ViGEm *and* UDP at the same time. | If yes, `AppRunner` holds a `vector<IOutputDevice*>` and fans out `UpdateState` |
| 3 | **Is the UDP target fixed per config file, or configurable per-run (CLI arg)?** | Whether `host`/`port` live in YAML or can be overridden at startup |
| 4 | **Send-on-every-update or send-on-change?** Should `UdpProtobufEmulator::UpdateState()` send a packet every call, or only when at least one channel value changed? | Network load; hot-path complexity |
| 5 | **Protobuf library preference** (see §10)? | Build system and install bundle |
| 6 | **Backward compat for existing YAML**: rename `gamepad:` → `output:`, or support both keys? | `ConfigLoader` complexity; whether existing configs need edits |

---

## 12. Files Affected Summary

| File | Change |
|---|---|
| `src/common/MappingDefinition.h` | `ControlTarget` enum deleted; `std::string target` + `ChannelType` added |
| `src/common/OutputState.h` | **New** — `OutputState { unordered_map<string,float> channels }` |
| `src/mapper/MappingEngine.h/.cpp` | `Apply()` writes `OutputState`; `channelType`-driven normalization |
| `src/mapper/GamepadState.h` | Retained as ViGEm-internal type, or deleted |
| `src/emulator/IOutputDevice.h` | **New** — generalised interface (replaces `IVigemClient`) |
| `src/emulator/VigemClient.h/.cpp` | Implements `IOutputDevice`; `OutputState → XUSB_REPORT` internally |
| `src/emulator/UdpProtobufEmulator.h/.cpp` | **New** — UDP + protobuf backend |
| `src/emulator/vehicle_state.proto` | **New** — protobuf message schema |
| `src/config/ConfigLoader.h/.cpp` | Parse `output:` section; `target` as string; infer/parse `channelType` |
| `src/app/AppRunner.h/.cpp` | Use `IOutputDevice`; backend factory from config |
| `src/main.cpp` | Construct `VigemEmulator` or delegate to `AppRunner` factory |
| `src/service/ServiceMain.cpp` | Same as `main.cpp` |
| `CMakeLists.txt` | Add protobuf; add `UdpProtobufEmulator` sources |
| `config/*.yaml` | Add `output:` section; rename `gamepad:` → `output:` per mapping |
