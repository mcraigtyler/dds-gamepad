## Relevant Files

- `CMakeLists.txt` - Core build configuration that will need updates for ViGEm, yaml-cpp, spdlog, tests, and IDL codegen.
- `vcpkg.json` - Dependency manifest (if used) for ViGEmClient, yaml-cpp, spdlog, and test frameworks.
- `src/main.cpp` - Current DDS sample app; will be refactored into an app entrypoint and/or replaced by a structured runtime.
- `src/dds_includes.h` - Central include point for DDS headers; may expand with config or mapping includes.
- `idl/Value.idl` - Existing DDS message definition for the POC throttle mapping.
- `docs/PRD.md` - Source requirements and acceptance criteria.
- `scripts/install_vigem.ps1` - New helper to download and install ViGEmBus.
- `src/emulator/VigemClient.*` - New wrapper around ViGEmClient for testability.
- `src/mapper/MappingEngine.*` - Mapping logic that converts DDS values to gamepad state.
- `src/config/ConfigLoader.*` - YAML configuration parsing and validation.
- `tests/mapper/MappingEngine.test.*` - Unit tests for mapping rules (scale/deadzone/invert).
- `tests/integration/DdsMappingHarness.*` - Integration harness to simulate DDS messages.
- `installer/` or `packaging/` - Installer scripts and packaging configuration (WiX/MSI or Inno Setup).

### Notes

- Unit tests should typically be placed alongside the code files they are testing (e.g., `MappingEngine.cpp` and `MappingEngine.test.cpp` in the same directory).
- Keep the first POC minimal and hardcoded to Value.idl + throttle mapping to validate the concept before adding generalized YAML mapping.

## Tasks

- [x] 1.0 Integrate ViGEm (POC foundation)
    - [x] 1.1 Add ViGEmClient dependency via FetchContent or vcpkg and verify it builds on Windows.
    - [x] 1.2 Create a minimal `IViGem` wrapper and a small sample that creates a virtual Xbox 360 controller.
    - [x] 1.3 Add a quick sanity executable or CMake target that updates a single control (e.g., right trigger) to validate runtime wiring.
- [x] 2.0 Map DDS `Value` messages to throttle input (POC functionality)
    - [x] 2.1 Update the DDS loop to consume `Value::Msg` and scale `value` into the right trigger range.
    - [x] 2.2 Wire the DDS read loop to the ViGEm wrapper to update the virtual gamepad on each message.
    - [x] 2.3 Add a small integration harness or mode to replay sample values for manual validation.
- [x] 3.0 Read Value.idl messages from Topic and convert to Throttle inputs
    - [x] 3.1 Confirm command-line topic argument handling in `src/main.cpp` and fail fast if missing/invalid.
    - [x] 3.2 Subscribe to the provided DDS topic for `Value::Msg` and read incoming samples in the main loop.
    - [x] 3.3 Remove existing publisher in main, it is no longer needed from the boilerplate.
    - [x] 3.4 Define the expected DDS value range and add a scaling + clamp step to map into the gamepad right trigger range. Incoming DDS messages will have a value range of 0 to 1 for throttle position.
    - [x] 3.5 Replace the 3-second emulated trigger cycle with DDS-driven trigger updates.
    - [x] 3.6 Add minimal logging or console output to verify incoming values and resulting trigger output during manual testing.

- [ ] 4.0 Add YAML-driven mapping configuration
    - [ ] 4.1 Define the YAML schema for DDS topic + mapping entries (per PRD) and add validation.
    - [ ] 4.2 Implement `ConfigLoader` to parse config files and produce mapping definitions.
    - [ ] 4.3 Extend mapping to support steering axes, triggers, scale, deadzone, and invert.
- [ ] 5.0 Structure the application for maintainability and testing
    - [ ] 5.1 Split runtime into modules (`dds/`, `mapper/`, `emulator/`, `config/`, `app/`).
    - [ ] 5.2 Implement `MappingEngine` as pure logic returning a `GamepadState` for easy unit testing.
    - [ ] 5.3 Add unit tests for mapping math and an integration test harness for DDS + mapping.
- [ ] 6.0 Packaging and installer deliverables
    - [ ] 6.1 Add `scripts/install_vigem.ps1` to download and run ViGEmBus installer with elevation.
    - [ ] 6.2 Create installer configuration (WiX/MSI or Inno Setup) that bundles the app and ViGEmBus installer.
    - [ ] 6.3 Document release steps and ensure build outputs match PRD acceptance criteria.
