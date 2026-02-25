# Architecture Review: dds-gamepad2

**Date:** 2026-02-25
**Reviewer:** Claude Code
**Codebase size:** ~2,500 lines of C++ source

---

## 1. Executive Summary

The project has a sound overall structure. The data flow is clean and linear, the module boundaries are generally well-drawn, and several good patterns are already in place (variant/visitor dispatch, RAII lifecycle management, abstract `IVigemClient` interface). The codebase is production-capable today.

The main friction points are:

1. `AppRunner` accumulates too many responsibilities. Local classes and large free functions defined inside the translation unit make it harder to read, test, or extend.
2. Three nearly-identical handler structs and three nearly-identical `Process*Samples` functions are a maintenance burden waiting to compound.
3. The `MappingEngine::Apply()` method mutates state through a `mutable` member while being declared `const`, which is a design smell that can mislead readers.
4. Cross-module coupling (`config` → `mapper`) and concrete type use inside `AppRunner` (instead of the existing `IVigemClient` interface) limit testability despite interfaces already being present.
5. `RxTable.h` includes `<windows.h>` unconditionally, polluting every translation unit that includes it.

None of these are bugs or blockers. They are incremental improvements that reduce maintenance cost and make the system easier to extend.

---

## 2. Current Module Structure

```
src/
├── main.cpp                  # Entry point; parses CLI args, calls AppRunner::Run()
├── dds_includes.h            # Macro guards for Windows/DDS header conflicts
├── app/
│   ├── AppRunner.h/.cpp      # Core orchestrator: DDS setup, main loop, console output
│   └── StopToken.h           # Graceful-shutdown atomic flag
├── config/
│   ├── ConfigLoader.h/.cpp   # YAML → RoleConfig; validates and groups mappings
│   └── [structs]             # DdsConfig, AppConfig, RoleConfig
├── mapper/
│   ├── MappingEngine.h/.cpp  # Transform raw values → GamepadState
│   ├── GamepadState.h        # POD struct (triggers, sticks, buttons bitmask)
│   └── MappingDefinition     # (inside MappingEngine.h)
├── emulator/
│   ├── VigemClient.h/.cpp    # ViGEmBus C API wrapper; RAII lifecycle
│   └── [interfaces]          # IVigemClient, ITxStateListener
├── console/
│   └── RxTable.h/.cpp        # Non-scrolling live console dashboard
└── service/
    ├── ServiceMain.cpp       # Windows SCM lifecycle + CLI parsing
    └── EventLog.h/.cpp       # Windows Event Log adapter
```

Data flow:

```
DDS topics → AppRunner (handlers) → MappingEngine → GamepadState → VigemClient → virtual Xbox 360 pad
```

---

## 3. Strengths to Preserve

| Strength | Notes |
|---|---|
| Clear pipeline | DDS in → transform → gamepad out is easy to follow |
| Variant/visitor dispatch | `std::variant<AnalogHandler, StickHandler, ButtonHandler>` avoids virtual dispatch cleanly |
| `IVigemClient` interface | Already defined; supports mock injection when wired up |
| YAML config schema | Human-readable; well-validated by ConfigLoader |
| Additive mappings | Elegant per-source state accumulation for opposing axes |
| `StopToken` shutdown | Memory-ordered atomic; correct and simple |
| RAII everywhere | VigemClient destructor, RxTable destructor are clean |
| Dual execution modes | Console and Windows service share the same AppRunner |

---

## 4. Issues and Recommendations

### 4.1 AppRunner Does Too Much  *(High Priority)*

**File:** [src/app/AppRunner.cpp](src/app/AppRunner.cpp)

`AppRunner::Run()` (~260 lines) sets up DDS infrastructure, initialises ViGEm, builds handlers, manages the status-polling state machine, drives the console table, and runs the main read loop — all in a single function. Two local classes (`TableTxStateListener`, `StatusSource`) and four free functions are defined in the same translation unit.

This is not yet unmaintainable, but adding a new DDS message type or a new display mode currently requires editing this one function across multiple dispersed sites.

**Recommended split:**

| New class / file | Responsibility extracted from AppRunner |
|---|---|
| `app/DdsSession.h/.cpp` | Create DDS `DomainParticipant` + `Subscriber`; own handler lifetimes |
| `app/StatusPoller.h/.cpp` | 500 ms status-polling loop; `FormatReaderStatus()`; `StatusSource` struct |
| `app/TableTxListener.h/.cpp` | `TableTxStateListener` inner class promoted to file scope |

`AppRunner::Run()` would then delegate to these helpers, shrinking to ~80 lines of orchestration.

---

### 4.2 Handler Struct and Process Function Duplication  *(High Priority)*

**File:** [src/app/AppRunner.cpp:177–415](src/app/AppRunner.cpp#L177)

`AnalogHandler`, `StickHandler`, and `ButtonHandler` share an identical data layout (name, topic, reader, mappingEngine, totalValidSamples, seenIds) and differ only in the DDS message type parameter. Their constructors are copy-paste identical.

`ProcessAnalogSamples`, `ProcessStickSamples`, and `ProcessButtonSamples` are structurally identical. The differences are:
- Which fields are read from the sample (`data.value()` vs `data.x()`/`data.y()` vs `data.btnState()`)
- How those values are formatted for the log/table
- Which `Apply()` field keys are used (`"value"`, `"x"`/`"y"`, `"btnState"`)

**Recommended refactoring:**

Templatize the handler struct and extract the per-type extraction logic into a small traits struct or a free function. The `Process*Samples` bodies can then collapse to a single template:

```cpp
// Handler struct becomes:
template <typename MsgT>
struct TopicHandler {
    std::string name;
    dds::topic::Topic<MsgT> topic;
    dds::sub::DataReader<MsgT> reader;
    mapper::MappingEngine mappingEngine;
    uint64_t totalValidSamples = 0;
    std::unordered_set<std::string> seenIds;
};

// Per-type extraction via traits:
template <typename MsgT> struct MessageTraits;

template <> struct MessageTraits<Gamepad::Gamepad_Analog> {
    static void Apply(const Gamepad::Gamepad_Analog& data,
                      int msgId, mapper::MappingEngine& engine,
                      mapper::GamepadState& state);
    static std::string Format(const Gamepad::Gamepad_Analog& data);
};
// ... similarly for Stick_TwoAxis and Button
```

This eliminates ~200 lines of duplication and ensures future DDS types require only a new traits specialization.

---

### 4.3 `MappingEngine::Apply()` Declared `const` but Mutates State  *(Medium Priority)*

**File:** [src/mapper/MappingEngine.h:53](src/mapper/MappingEngine.h#L53), [src/mapper/MappingEngine.cpp:56](src/mapper/MappingEngine.cpp#L56)

```cpp
bool Apply(const std::string& field, int message_id, float value, GamepadState& state) const;
// ...
mutable std::unordered_map<std::string, float> additive_state_;
```

Using `mutable` to allow mutation inside a `const` method is a pattern reserved for caches and lazy computation — not for observable behavioral state like the additive accumulator. A caller that calls `Apply()` twice with the same input gets different outputs depending on accumulated history; this is not a `const` operation by any meaningful definition.

**Recommendation:** Remove `const` from `Apply()`. The `mutable` keyword can then be removed. The method signature change is safe because callers hold non-const `MappingEngine` objects.

---

### 4.4 `IVigemClient` Interface Exists but Is Not Used  *(Medium Priority)*

**File:** [src/emulator/VigemClient.h:15](src/emulator/VigemClient.h#L15), [src/app/AppRunner.cpp:469](src/app/AppRunner.cpp#L469)

`IVigemClient` is defined but `AppRunner::Run()` instantiates `emulator::VigemClient` directly. This means there is no path to inject a mock, stub, or alternative backend without editing `AppRunner`.

**Recommendation:** Accept `IVigemClient&` (or `std::unique_ptr<IVigemClient>`) in `AppRunner::Run()` or in an `AppRunner` constructor. `main.cpp` and `ServiceMain.cpp` continue to construct the real `VigemClient`; tests can inject a mock.

The existing `IVigemClient` already has the right shape — this is a one-line caller change.

---

### 4.5 `RxTable.h` Includes `<windows.h>` Unconditionally  *(Medium Priority)*

**File:** [src/console/RxTable.h:7](src/console/RxTable.h#L7)

`<windows.h>` in a public header forces Windows API macros (`min`, `max`, `BOOL`, etc.) into every translation unit that includes `RxTable.h`. The existing `dds_includes.h` shows the team is already aware of Windows macro pollution.

**Recommendation:** Move all Windows API types (`HANDLE`, `SHORT`, `CONSOLE_SCREEN_BUFFER_INFO`, etc.) to the `.cpp` file. Replace public-facing members with opaque handles or forward declarations using a Pimpl pattern:

```cpp
// RxTable.h — no Windows headers
class RxTable {
public:
    // ... same public API ...
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

`RxTable.cpp` contains `Impl` with all Windows types. This also gives `RxTable` a move constructor essentially for free.

---

### 4.6 Cross-Module Dependency: `config` Imports from `mapper`  *(Medium Priority)*

**File:** [src/config/ConfigLoader.h:6](src/config/ConfigLoader.h#L6)

`ConfigLoader.h` includes `mapper/MappingEngine.h` to use `mapper::MappingDefinition`. This makes the `config` module depend on the `mapper` module. If `MappingDefinition` fields change (e.g., adding a new parameter), `ConfigLoader` must be updated too, even though configuration loading and value transformation are logically separate concerns.

**Recommendation:** Move `MappingDefinition` and `ControlTarget` into a shared header in the `config` module (or into a new `common/` module) that neither `config` nor `mapper` own:

```
src/common/
    MappingDefinition.h    # Pure data; no logic
```

Both `ConfigLoader.h` and `MappingEngine.h` include this. The dependency arrow becomes:

```
config → common ← mapper
```

instead of:

```
config → mapper
```

---

### 4.7 Inconsistent Error Propagation  *(Medium Priority)*

Three different error-handling styles are in use:

| Module | Style |
|---|---|
| `ConfigLoader` | Throws `std::runtime_error` / `std::invalid_argument` |
| `VigemClient` | Returns `bool`; stores message in `last_error_` |
| `AppRunner` | Returns `int` (EXIT_SUCCESS/FAILURE); stores message in `_lastError` |

This inconsistency means callers must handle three styles. `AppRunner::Run()` catches the ConfigLoader exception and converts it to a return code; a future caller might forget that conversion.

**Recommendation:** Pick one strategy for infrastructure errors (those that abort startup):

- **Option A — Exceptions throughout:** ConfigLoader already does this. VigemClient throws on `Connect()` failure; AppRunner's outer try/catch remains unchanged.
- **Option B — Error return value throughout:** Define a result type or use `std::expected<T, std::string>` (C++23) / `std::optional` with a separate error string. Exceptions only for programmer errors (wrong usage).

Option A is less code change and already partially in use. Either is fine; the goal is consistency.

---

### 4.8 `TopicType` Enum and Parser Belong with Configuration  *(Low Priority)*

**File:** [src/app/AppRunner.cpp:30–50](src/app/AppRunner.cpp#L30)

`enum class TopicType` and `ParseTopicType()` are defined in the anonymous namespace of `AppRunner.cpp`. They logically describe the type of a DDS subscription — information that belongs in the config layer since it's derived from `config.dds.type`.

**Recommendation:** Move `TopicType` to `ConfigLoader.h` (or the shared `common/` module proposed above) and parse it during config loading, storing it on `AppConfig`. This:
- Caches the parse result (currently called per handler setup, not per loop, so impact is small)
- Makes the config layer the single source of truth about topic type
- Removes the need for `AppRunner` to know DDS type name strings

---

### 4.9 Local Classes Inside `Run()` Should Be Promoted  *(Low Priority)*

**File:** [src/app/AppRunner.cpp:557–610](src/app/AppRunner.cpp#L557)

`TableTxStateListener` and `StatusSource` are defined as local classes inside `Run()`. This is legal C++ but hurts readability: the reader must scroll through variable declarations before reaching the main loop logic, and neither class can be forward-declared or reused.

**Recommendation:** Promote both to file-scope (anonymous namespace in `AppRunner.cpp` or, per 4.1 above, their own headers). `StatusSource` in particular would benefit from being in `StatusPoller.h`.

---

### 4.10 CMakeLists.txt: Duplicated `target_include_directories`  *(Low Priority)*

**File:** [CMakeLists.txt:140–152](CMakeLists.txt#L140)

The `target_include_directories` blocks for `dds-gamepad` and `dds-gamepad-service` are identical. This must be kept in sync manually.

**Recommendation:** Define a CMake interface library or use a function:

```cmake
set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds-cxx/include/ddscxx"
)
target_include_directories(${EXE_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_include_directories(${SERVICE_EXE_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
```

---

### 4.11 `UpdateRightTrigger` on `IVigemClient` Is Dead Code  *(Low Priority)*

**File:** [src/emulator/VigemClient.h:21](src/emulator/VigemClient.h#L21)

`IVigemClient::UpdateRightTrigger(uint8_t)` appears on the interface but is not called anywhere in the codebase (all state updates go through `UpdateState()`). Dead interface methods are misleading and require maintenance.

**Recommendation:** Remove `UpdateRightTrigger` from the interface and implementation unless there is a planned use for it.

---

## 5. Summary Table

| # | Issue | Priority | Effort |
|---|---|---|---|
| 4.1 | AppRunner does too much; Run() is 260 lines | High | Medium |
| 4.2 | Three duplicate handler structs + three duplicate Process functions | High | Medium |
| 4.3 | `Apply()` declared `const` but mutates via `mutable` | Medium | Small |
| 4.4 | `IVigemClient` interface not injected | Medium | Small |
| 4.5 | `<windows.h>` in public RxTable.h header | Medium | Medium |
| 4.6 | `config` module imports from `mapper` module | Medium | Medium |
| 4.7 | Inconsistent error propagation across modules | Medium | Medium |
| 4.8 | `TopicType` parsing belongs in config layer | Low | Small |
| 4.9 | Local classes inside `Run()` | Low | Small |
| 4.10 | Duplicate CMake include directories | Low | Trivial |
| 4.11 | Dead `UpdateRightTrigger` on interface | Low | Trivial |

---

## 6. Implementation Plan

The changes below are sequenced so that each step compiles and passes any existing smoke tests independently. Later steps build on earlier ones but none are blocking.

### Phase 1 — Safe, Small Fixes (no design change)

**Goal:** Eliminate clearly wrong patterns. Low risk, immediately reviewable.

1. **[4.3] Remove `const` from `MappingEngine::Apply()`** and drop the `mutable` keyword.
   - Edit [src/mapper/MappingEngine.h:53](src/mapper/MappingEngine.h#L53) and [src/mapper/MappingEngine.cpp:56](src/mapper/MappingEngine.cpp#L56).
   - All callers hold non-const engines; no caller changes needed.

2. **[4.10] Deduplicate CMake include directories.**
   - Edit [CMakeLists.txt:140–152](CMakeLists.txt#L140). Define a variable and apply once per target.

3. **[4.11] Remove dead `UpdateRightTrigger` from `IVigemClient`.**
   - Edit [src/emulator/VigemClient.h](src/emulator/VigemClient.h) and [src/emulator/VigemClient.cpp](src/emulator/VigemClient.cpp).

### Phase 2 — Config/Type Cleanup

**Goal:** Improve module boundaries and remove redundant parsing.

4. **[4.6 + 4.8] Move `MappingDefinition`, `ControlTarget`, and `TopicType` to `src/common/`.**
   - Create `src/common/MappingDefinition.h` containing `ControlTarget`, `MappingDefinition`, `TopicType`.
   - Update `ConfigLoader.h` to include `common/MappingDefinition.h` instead of `mapper/MappingEngine.h`.
   - Update `MappingEngine.h` similarly.
   - Add `TopicType` field to `AppConfig`; parse it in `ConfigLoader::Load()`.
   - Remove `ParseTopicType()` from `AppRunner.cpp`; read `config.topicType` instead.

5. **[4.7] Standardise error propagation.**
   - Choose exception-based for startup failures (consistent with ConfigLoader).
   - Change `VigemClient::Connect()` and `AddX360Controller()` to throw `std::runtime_error` on failure (bool return retained for backward compat if preferred — document the choice).
   - AppRunner's existing `try/catch` already handles this.

### Phase 3 — AppRunner Decomposition

**Goal:** Break the monolithic `Run()` into focused classes.

6. **[4.9] Promote `TableTxStateListener` and `StatusSource`** to anonymous-namespace scope in `AppRunner.cpp`. No behaviour change; just refactoring within the file.

7. **[4.1] Extract `StatusPoller`** to `src/app/StatusPoller.h/.cpp`.
   - Move `StatusSource`, `FormatReaderStatus()`, and the 500 ms polling block.
   - `AppRunner::Run()` instantiates a `StatusPoller` and calls `Poll(now, tablePtr)`.

8. **[4.4] Inject `IVigemClient`** instead of constructing `VigemClient` directly.
   - Add an overload `AppRunner::Run(const AppRunnerOptions&, IVigemClient&, const StopToken&)`.
   - Existing `Run(options, stopToken)` constructs `VigemClient` and delegates — preserving the public API.
   - `ServiceMain.cpp` and `main.cpp` continue to work unchanged.

### Phase 4 — Handler Deduplication

**Goal:** Replace 3×handler structs + 3×process functions with one template each.

9. **[4.2] Template the handler struct.**
   - Create `TopicHandler<MsgT>` in `AppRunner.cpp` anonymous namespace.
   - Define `MessageTraits<Gamepad::Gamepad_Analog>`, `MessageTraits<Gamepad::Stick_TwoAxis>`, `MessageTraits<Gamepad::Button>` with `Apply()` and `Format()` static methods.
   - Replace `ProcessAnalogSamples`, `ProcessStickSamples`, `ProcessButtonSamples` with one `ProcessSamples<MsgT>()` template.
   - The variant type alias becomes `std::variant<TopicHandler<Gamepad::Gamepad_Analog>, TopicHandler<Gamepad::Stick_TwoAxis>, TopicHandler<Gamepad::Button>>`.

### Phase 5 — Platform Isolation

**Goal:** Prevent Windows headers leaking through public headers.

10. **[4.5] Pimpl `RxTable`.**
    - Move all Windows API members to `RxTable::Impl` in `RxTable.cpp`.
    - `RxTable.h` includes no platform headers.
    - Adds a proper move constructor (currently deleted).

---

## 7. What Not to Change

- **The data flow pipeline** — it is clean and correct.
- **The YAML config schema** — it is well-designed and already in use.
- **The additive mapping logic** — the per-source accumulation design is sound; only the `const` annotation needs fixing (Phase 1, item 1).
- **The variant/visitor dispatch** — it is idiomatic and eliminates virtual dispatch. Phase 4 preserves this.
- **The StopToken shutdown mechanism** — correct memory ordering; no change needed.
- **The dual console/service execution model** — clean separation; `AppRunner` being shared is the right design.
