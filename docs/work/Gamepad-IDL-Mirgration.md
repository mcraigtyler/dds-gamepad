# Gamepad IDL Migration

Summary

The project is migrating from `idl/Value.idl` to `idl/crte_idl/Gamepad.idl`. The new IDL defines `Gamepad_Analog` (used for Throttle and Brake) and `Stick_TwoAxis` (used for Steering — we will use the `x` field only). This document describes the required code, config, build, and documentation changes and provides a tasks checklist.

Context and rationale

- `Value.idl` provided a single `Value::Msg` message used for the initial POC throttle mapping.
- `Gamepad.idl` groups related controls into semantically meaningful structs and matches the eventual mapping schema: analog controls for triggers and two-axis sticks for steering.
- Moving to `Gamepad.idl` reduces custom handling in code and lets the YAML mapping reference richer message types and fields.

Required changes (high-level)

- Documentation: Update `docs/PRD.md` and `README.md` to reference `idl/crte_idl/Gamepad.idl` and to describe that `Gamepad_Analog` maps to throttle/brake and `Stick_TwoAxis.x` maps to steering.
- YAML configs: Replace `Value.idl` and `Value::Msg` in config YAMLs with `Gamepad.idl` types. Provide example mappings for throttle (Gamepad_Analog -> `axis:right_trigger`), brake (Gamepad_Analog -> `axis:left_trigger` or similar), and steering (`Stick_TwoAxis.x` -> `axis:left_x`/`axis:left_y` depending on project mapping).
- Build/IDL codegen: Ensure `idl/crte_idl/Gamepad.idl` is included in the IDL code generation step and generated sources are compiled into the project.
- ConfigLoader: Update `src/config/ConfigLoader.*` so it validates mapping entries that target `Gamepad_Analog` and `Stick_TwoAxis`, and supports dotted-field access (e.g., `field: x`).
- MappingEngine: Accept the new types and fields; map `Gamepad_Analog.value` to triggers (apply `scale`, `deadzone`, `invert`) and `Stick_TwoAxis.x` to steering axis (use only `x`, apply `scale`, `deadzone`, `invert`).
- Tests: Update unit tests in `tests/mapper/` and the integration harness to publish the new message types for automated verification.
- VIgEm wrapper & mocks: Verify `IViGem` and any mocks still meet the new mapping behavior. Update as required.
- Packaging: Update documentation/instructions for packaging/installer if the runtime expectations or generated artifacts change.

Relevant Files

- `idl/crte_idl/Gamepad.idl` - New IDL to be used for mappings.
- `docs/PRD.md` - Update to reference the new IDL and mapping choices.
- `README.md` - Update usage examples and example YAML to use `Gamepad.idl` types.
- `docs/Tasks.md` - (existing) update or reference; this new document captures the specific migration tasks.
- `src/config/ConfigLoader.*` - Parse and validate updated YAML mappings and dotted-field access.
- `src/mapper/MappingEngine.*` - Implement mapping from `Gamepad_Analog` and `Stick_TwoAxis.x` to gamepad outputs.
- `tests/mapper/MappingEngine.test.*` - Update/add tests for new types and dotted-field behavior.
- `tests/integration/DdsMappingHarness.*` - Extend to publish `Gamepad_Analog` and `Stick_TwoAxis` messages for verification.
- `CMakeLists.txt` / `cmake/FetchThirdParties.cmake` - Include `Gamepad.idl` in IDL codegen and compilation.

Notes

- Preserve existing mapping semantics: `scale` applied before `deadzone` and `invert` semantics unchanged.
- Steering messages will provide only the `x` value; mapping logic must tolerate missing `y` values.
- Keep `IViGem` interface minimal to allow unit testing of mapping logic without the kernel driver.

Tasks

- [ ] 1.0 Documentation updates (first step)
    - [x] 1.1 Create `docs/work/Gamepad-IDL-Mirgration.md` (this file).
    - [ ] 1.2 Update `docs/PRD.md` to reference `idl/crte_idl/Gamepad.idl` and document type usage (`Gamepad_Analog` for Throttle/Brake, `Stick_TwoAxis.x` for Steering).
    - [ ] 1.3 Update `README.md` examples and usage to show the new YAML mapping and example config snippet.
- [ ] 2.0 YAML config updates
    - [ ] 2.1 Replace `Value.idl` / `Value::Msg` references in existing config files with `Gamepad.idl` types.
    - [ ] 2.2 Add example config files demonstrating mappings for Throttle (`Gamepad_Analog`), Brake (`Gamepad_Analog`), and Steering (`Stick_TwoAxis.x`).
    - [ ] 2.3 Validate configs with the updated `ConfigLoader` once implemented.
- [ ] 3.0 IDL codegen and build
    - [ ] 3.1 Add `idl/crte_idl/Gamepad.idl` to the IDL code generation step in CMake.
    - [ ] 3.2 Ensure generated sources are compiled and linked into the app targets.
- [ ] 4.0 ConfigLoader changes
    - [ ] 4.1 Extend parsing to accept `Gamepad_Analog` and `Stick_TwoAxis` message types.
    - [ ] 4.2 Implement dotted-field access (e.g., `field: x`) and type-specific validation.
- [ ] 5.0 MappingEngine updates
    - [ ] 5.1 Map `Gamepad_Analog` values to trigger outputs (throttle/brake). Ensure correct normalization and clamping.
    - [ ] 5.2 Map `Stick_TwoAxis.x` to steering axis; accept only `x` as input and ignore `y` if absent.
    - [ ] 5.3 Preserve `scale`, `deadzone`, and `invert` behavior for all mappings.
- [ ] 6.0 ViGEm wrapper
    - [ ] 6.1 Confirm `IViGem` still satisfies test needs.
    - [ ] 6.2 Run `vigem_sanity` to verify controller creation still works after changes.
- [ ] 7.0 Packaging & docs follow-up
    - [ ] 7.1 Update packaging notes in `docs/PRD.md` and `README.md` if build/install artifacts change.
