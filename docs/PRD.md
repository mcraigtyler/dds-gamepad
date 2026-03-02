# Product Requirements Document (PRD) — dds-gamepad

This document contains the product requirements, design decisions, and implementation details for the dds-gamepad project. It is intended for implementers, reviewers, and automated agents that will scaffold and implement the application.

## Purpose
Translate DDS messages carrying input data into a single virtual Xbox 360 controller exposed to Windows via ViGEm. The virtual controller will be consumed by an Unreal Engine game mapping vehicle controls to the emulated controller.

A final distributable installer should be provided and contain everything necessary to run the application on a machine with a clean install of Windows 11.

## Target summary
- Target OS: Windows 11 (x64)
- Language: C++ (native)
- Dev Environment: VS Code
- DDS Library: CycloneDDS (C runtime) + cyclonedds-cxx
- Config files: YAML (one topic/IDL per config)
- Virtual controller backend: ViGEm (ViGEmClient + ViGEmBus driver)

## High level goals
- Subscribe to a configured DDS topic and deserialize messages according to provided IDL files.
- Map message fields to gamepad controls (buttons, axes, triggers) using YAML configuration.
- Expose a single virtual Xbox 360 controller to Windows using ViGEm.
- Provide an installer that bundles or triggers installation of ViGEmBus (requires elevation).

## Acceptance criteria
- A reproducible build that produces a Windows executable which:
  - Subscribes to a configured DDS topic and receives messages matching provided IDL.
  - Uses a YAML mapping to convert message fields into gamepad inputs.
  - Creates and updates a ViGEm virtual Xbox 360 controller reflecting the mapped inputs.
  - Includes unit tests for mapping logic and a small integration harness to simulate DDS messages.

## Design and technology choices
- Preferred stack
  - C++ (native) for lowest-latency mapping and straightforward integration with CycloneDDS and ViGEmClient.
  - CMake + MSVC toolchain (VS Code) as the primary build environment.
  - vcpkg (optional/pinned) or FetchContent (default) for third-party dependencies.
- Key libraries
  - CycloneDDS / cyclonedds-cxx
  - ViGEmClient (client library)
  - yaml-cpp (config parsing)
  - spdlog (logging)

## Driver installation and packaging notes

Important: ViGEmBus is a kernel driver and must be installed on the target machine. The driver package distributed by the ViGEm project is signed; do not attempt to build and ship a kernel driver yourself unless you handle driver signing.

Installer strategy
- Bundle the official ViGEmBus installer (ViGEmBus_Setup.exe) with your installer and run it during install time with elevation, or download & run it as part of an installation action.
- Installer options: MSIX (modern), WiX/MSI (traditional & scriptable), or Inno Setup (simpler). For enterprise scenarios, MSI/WiX is preferable.
- Example PowerShell snippet to run the bundled installer silently (inside your installer or helper script):

```powershell
# Example: run bundled ViGEmBus installer with elevation (silent flag depends on the installer)
Start-Process -FilePath "$PSScriptRoot\ViGEmBus_Setup.exe" -ArgumentList '/S' -Verb RunAs -Wait
```


## Dependency and build strategy
- Default (recommended): Use CMake FetchContent to download and pin upstream sources during configure. This keeps the repo small and CI-friendly.
- Optional (deterministic): Use `vcpkg` in manifest mode or as a submodule for a pinned per-repo toolchain. Manifest mode is preferred when reproducibility is critical.
- Pin all third-party refs (tags/commits) to ensure deterministic CI builds.

## IDL handling
- Recommended: compile-time code generation. Place IDL files under `idl/` and reference them from each YAML mapping config.
- At configure time, run CycloneDDS IDL codegen (C/C++) and compile the generated sources into the project.

## Config schema (YAML)
- One topic/IDL per config file.
- Top-level fields: `dds` (topic, type, idl_file, qos) and `mapping` (array of mapping entries).
- Mapping entry fields: `name`, `id` (message id), `field` (dotted path into message), `to` (target control: button/axis), `scale`, `deadzone`, `invert`.

Example mapping snippet

```yaml
dds:
  topic: "vehicle.throttle_position"
  type: "Gamepad_Analog"
  idl_file: "idl/Gamepad.idl"

mapping:
  - name: throttle
    id: 1
    field: value
    to: axis:right_trigger
    scale: 1.0
    deadzone: 0.0
```

`Gamepad_Analog` is used for throttle and brake mappings (use the `value` field), and `Stick_TwoAxis.x` is used for steering inputs.

## Project layout (recommended)
```
dds-gamepad/
  CMakeLists.txt
  README.md
  docs/
    PRD.md
    Tasks.md
    Architecture.md
  idl/
  configs/
  src/
    app/
    dds/
    mapper/
    emulator/
    config/
  cmake/
  scripts/
  tests/
```

## Interfaces and testability
- Provide `IViGem` small interface to wrap ViGEmClient so mapping logic can be unit tested with a mock implementation.
- `MappingEngine` should be pure logic: input (field values) -> `GamepadState` output. Unit tests should cover scale, deadzone, invert, and axis normalization.

## Build-time codegen and tooling
- Add `cmake/FetchThirdParties.cmake` to centralize FetchContent pins and IDL codegen steps.
- Add `cmake/clang-tidy.cmake`, `.clang-tidy`, and `.clang-format` as early-in-phase artifacts so code is formatted and analyzed from the start.
- Provide `CMakePresets.json` entries for default and analysis builds (the latter using Ninja + clang-cl where appropriate for clang-tidy accuracy on Windows).

## Runtime and packaging notes
- ViGEmBus driver must be installed using the official signed installer. Provide `scripts/install_vigem.ps1` to download, verify, and run the installer with elevation.
- Packaging options: MSIX (modern), WiX/MSI (enterprise), Inno Setup (simple). MSI/WiX recommended for enterprise scenarios.
