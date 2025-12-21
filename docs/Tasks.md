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

- [ ] 1.0 Integrate ViGEm (POC foundation)
    - [ ] 1.1 Add ViGEmClient dependency via FetchContent or vcpkg and verify it builds on Windows.
    - [ ] 1.2 Create a minimal `IViGem` wrapper and a small sample that creates a virtual Xbox 360 controller.
    - [ ] 1.3 Add a quick sanity executable or CMake target that updates a single control (e.g., right trigger) to validate runtime wiring.
- [ ] 2.0 Map DDS `Value` messages to throttle input (POC functionality)
    - [ ] 2.1 Update the DDS loop to consume `Value::Msg` and scale `value` into the right trigger range.
    - [ ] 2.2 Wire the DDS read loop to the ViGEm wrapper to update the virtual gamepad on each message.
    - [ ] 2.3 Add a small integration harness or mode to replay sample values for manual validation.
- [ ] 3.0 Add YAML-driven mapping configuration
    - [ ] 3.1 Define the YAML schema for DDS topic + mapping entries (per PRD) and add validation.
    - [ ] 3.2 Implement `ConfigLoader` to parse config files and produce mapping definitions.
    - [ ] 3.3 Extend mapping to support buttons, axes, triggers, scale, deadzone, and invert.
- [ ] 4.0 Structure the application for maintainability and testing
    - [ ] 4.1 Split runtime into modules (`dds/`, `mapper/`, `emulator/`, `config/`, `app/`).
    - [ ] 4.2 Implement `MappingEngine` as pure logic returning a `GamepadState` for easy unit testing.
    - [ ] 4.3 Add unit tests for mapping math and an integration test harness for DDS + mapping.
- [ ] 5.0 Packaging and installer deliverables
    - [ ] 5.1 Add `scripts/install_vigem.ps1` to download and run ViGEmBus installer with elevation.
    - [ ] 5.2 Create installer configuration (WiX/MSI or Inno Setup) that bundles the app and ViGEmBus installer.
    - [ ] 5.3 Document release steps and ensure build outputs match PRD acceptance criteria.
