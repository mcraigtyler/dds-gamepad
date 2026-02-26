# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Project Does

`dds-gamepad` subscribes to DDS (Data Distribution Service) topics carrying input values from an external hardware device and translates them into a virtual Xbox 360 gamepad exposed to Windows via the ViGEmBus driver. The virtual gamepad is consumed by an Unreal Engine game that maps it to vehicle controls (steering, throttle, brake, turret, weapons).

## Prerequisites (One-Time Setup)

```powershell
# Initialize CycloneDDS libraries
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\install_cyclonedds.ps1 -Force -Generator "Visual Studio 17 2022"

# Initialize git submodules (including vcpkg)
git submodule update --init --recursive

# Bootstrap vcpkg
.\external\vcpkg\bootstrap-vcpkg.bat

# Install ViGEmBus driver (required for virtual controller) — download from GitHub releases and run as admin
```

## Build Commands

Configure (adjust paths for local CycloneDDS install location):
```powershell
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
  "-DCMAKE_PREFIX_PATH=c:/dev/cvt/dds-boilerplate/install/cyclonedds;c:/dev/cvt/dds-boilerplate/install/cyclonedds-cxx" \
  "-DCMAKE_CONFIGURATION_TYPES=Debug;Release" \
  -DINSTALL_DIR=c:/dev/cvt/dds-boilerplate/install/dds-gamepad \
  -S C:/dev/cvt/dds-gamepad2 -B C:/dev/cvt/dds-gamepad2/build \
  -G "Visual Studio 17 2022" -T host=x64 -A x64
```

Build and install (Debug):
```powershell
cmake --build .\build --config Debug --target install
```

Build and install (Release — required for packaging):
```powershell
cmake --build .\build --config Release --target install
```

The VS Code CMake extension reads `.vscode/` settings and handles configure automatically.

## Run

```powershell
# Console mode with live table display
.\install\dds-gamepad\bin\dds-gamepad.exe <config_file> <domain_id> <yoke_id> [--table | --debug]

# Example
.\install\dds-gamepad\bin\dds-gamepad.exe config\driver.yaml 0 1004 --table

# ViGEm sanity check (validates driver installation)
.\install\dds-gamepad\bin\vigem_sanity.exe
```

`--table`: Live dashboard (non-scrolling). `--debug`: Verbose raw input logging.

## Architecture

### Data Flow

```
DDS topics → AppRunner (read loop) → MappingEngine → OutputState → IOutputDevice → backend
                                                                         │
                                                          VigemClient → virtual Xbox 360 pad
```

### Key Files

| File | Role |
|------|------|
| `src/app/AppRunner.cpp` | Core orchestrator. Manages DDS readers, input loop, mapping dispatch, and console output. Constructs the output backend from `config.output.type`. |
| `src/config/ConfigLoader.cpp` | Parses role YAML into `RoleConfig` / `MappingDefinition` structs, including the `output:` backend section. |
| `src/mapper/MappingEngine.cpp` | Applies scale, deadzone, and invert to raw DDS values; writes normalised floats to `OutputState::channels`. |
| `src/emulator/IOutputDevice.h` | Generic output-device interface (`Connect`, `UpdateState(OutputState)`). |
| `src/emulator/VigemClient.cpp` | Implements `IOutputDevice`; converts `OutputState` channels to `XUSB_REPORT` for the virtual Xbox 360 controller. |
| `src/emulator/UdpProtobufEmulator.cpp` | Stub `IOutputDevice` for the `udp_protobuf` backend (no-op; full UDP + protobuf implementation deferred to Phase 5). |
| `src/common/OutputState.h` | Generic `unordered_map<string, float>` channel map passed between mapper and backend. |
| `src/service/ServiceMain.cpp` | Windows service lifecycle (OnStart/OnStop); re-uses AppRunner in a worker thread. |
| `src/console/RxTable.cpp` | Live console table using cursor positioning. |

### IDL / DDS Message Types

IDL files in `idl/` define the DDS message types. CMake generates C++ from them via `idlcxx_generate`. The three active DDS types used in mappings:

- `Gamepad::Gamepad_Analog` — single `Double_t value` field
- `Gamepad::Stick_TwoAxis` — `Double_t x` and `Double_t y` fields
- `Gamepad::Button` — optional `btnState` field (`ButtonState_t` enum: Down/Up)

Messages are filtered by `id` (DDS role id) and `yoke_id` (matched against `sub_role` in the message).

### Configuration Schema

Role YAML files (`config/driver.yaml`, `config/gunner.yaml`, `config/combined.yaml`):

```yaml
role:
  name: "Driver"

# Optional. Selects the output backend. Defaults to vigem_x360 if absent.
output:
  type: vigem_x360        # "vigem_x360" (default) or "udp_protobuf" (stub — full implementation Phase 5)
  # host: 192.168.1.100   # udp_protobuf only
  # port: 5000            # udp_protobuf only

mappings:
  - name: steering
    dds:
      topic: "Gamepad_Stick_TwoAxis"
      type: "Gamepad::Stick_TwoAxis"
      idl_file: "idl/Gamepad.idl"
      id: 30           # DDS role id to match
      field: x         # "x", "y", "value", or "btnState"
      input_min: -110.0
      input_max: 110.0
    output:             # was: gamepad: — both keys accepted for backward compatibility
      to: axis:right_x  # See valid targets below
      scale: 1.0
      deadzone: 0.02
      invert: false
      # type: axis      # Optional. Explicit channel type: axis, trigger, or button.
                        # Inferred from the "to" prefix for vigem_x360 channels.
                        # Required for udp_protobuf configs using custom field names.
```

Valid `output.to` targets for `vigem_x360`:
- Triggers: `axis:left_trigger`, `axis:right_trigger`
- Sticks: `axis:left_x`, `axis:left_y`, `axis:right_x`, `axis:right_y`
- Buttons: `button:a`, `button:b`, `button:x`, `button:y`
- D-Pad: `dpad:up`, `dpad:down`, `dpad:left`, `dpad:right`

For `udp_protobuf`, `output.to` is the protobuf field name (e.g. `steering`, `throttle`); `output.type` must be set explicitly (`axis`, `trigger`, or `button`).

`yoke_id` is **not** in YAML — pass it as a CLI argument (console) or `--yoke-id` (service).

### Build Targets

- `dds-gamepad` — console executable
- `dds-gamepad-service` — Windows service executable
- `vigem_sanity` — virtual controller sanity check
- `cyclonedds_lib` — IDL-generated C++ library (internal)

### Dependencies

- **CycloneDDS-CXX**: DDS middleware (pre-installed via `install_cyclonedds.ps1`)
- **yaml-cpp**: Config parsing (via vcpkg)
- **ViGEmClient** v1.21.222.0: Virtual input library (fetched by CMake via FetchContent from GitHub; use `-DVIGEMCLIENT_SOURCE_DIR=<path>` if GitHub is unavailable)
- **ViGEmBus driver**: Must be installed separately on any machine that runs the app

## Windows Service

```powershell
# Install and start (run PowerShell as Administrator)
cd C:\dds-gamepad
.\install_service.ps1 -Action Install -InstallDir (Get-Location).ProviderPath -DomainId 0 -YokeId 1004 -ConfigFilePath "bin\config\driver.yaml" -StartType Automatic
.\install_service.ps1 -Action Start

# Stop / uninstall
.\install_service.ps1 -Action Stop
.\install_service.ps1 -Action Uninstall
```

Service logs to Windows Event Log (Application) under source `dds-gamepad-service`. Config defaults to `bin\config\driver.yaml` next to the service executable.

## Packaging

```powershell
# Build release install tree, then zip it
cmake --build build --config Release --target install
Compress-Archive -Path install\dds-gamepad\* -DestinationPath release\dds-gamepad-install.zip -Force
```

Ship Release builds only — the app expects Release DLLs (no `*D.dll` debug runtimes).
