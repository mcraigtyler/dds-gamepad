# DDS Gamepad
dds-gamepad subscribes to DDS topics that carry input values coming from an external input device and translates those messages into a single virtual Xbox 360-style gamepad exposed to Windows. The primary consumer of those inputs will be an Unreal Engine game (separate project) that maps the emulated gamepad to vehicle controls (steering, accelerator, brake).

This repository contains the application and documentation required to build an installer that will set up everything necessary on a clean Windows 11 machine.

## Development Envrionment Setup

Before opening VS Code run the following from powershell:

- Initialize Cyclone Libraries: `powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\install_cyclonedds.ps1 -Force -Generator "Visual Studio 17 2022"`
- Initlialize Git Submodules: `git submodule update --init --recursive`
- Initialize vcpkg Submodule: `.\external\vcpkg\bootstrap-vcpkg.bat`
- Install the ViGEmBus driver (required for virtual controllers). Download the latest installer from https://github.com/nefarius/ViGEmBus/releases and run it as administrator.
- ViGEmClient is fetched by CMake with FetchContent. If your build machine cannot access GitHub, clone https://github.com/ViGEm/ViGEmClient and configure with `-DVIGEMCLIENT_SOURCE_DIR=path\to\ViGEmClient`.

## Configure and Build Source

### Configure

If you use the cmake tool in VS Code the Configure in that extension will pull from the settings in the .vscode directory. Otherwise you can enter a command similar to the following:

`cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE "-DCMAKE_PREFIX_PATH=c:/dev/cvt/dds-boilerplate/install/cyclonedds;c:/dev/cvt/dds-boilerplate/install/cyclonedds-cxx" "-DCMAKE_CONFIGURATION_TYPES=Debug;Release" -DINSTALL_DIR=c:/dev/cvt/dds-boilerplate/install/boilerplate-dds --no-warn-unused-cli -S C:/dev/cvt/dds-boilerplate -B c:/dev/cvt/dds-boilerplate/build -G "Visual Studio 17 2022" -T host=x64 -A x6`


### Build
`cmake --build .\build --config Debug --target install`


## Execute Sim

`.\install\simple-dds\bin\simple-sim.exe --topics .\install\simple-dds\config\input`

## Run DDS Gamepad

The main `dds-gamepad` executable reads every YAML file in a config folder. Each config defines one DDS topic and a single mapping rule. Incoming values are expected to be in the `0.0` to `1.0` range and are scaled to the target control range.

`.\install\boilerplate-dds\bin\dds-gamepad.exe <config_dir> [domain_id]`

### Example config (one file per topic)

```yaml
dds:
  topic: "vehicle.throttle"
  type: "Gamepad_Analog"
  idl_file: "idl/Gamepad.idl"
  domain_id: 0

mapping:
  - name: throttle
    id: 1
    field: value
    to: axis:right_trigger
    scale: 1.0
    deadzone: 0.05
    invert: false
```

Steering example using the `Stick_TwoAxis.x` field:

```yaml
dds:
  topic: "vehicle.steering"
  type: "Stick_TwoAxis"
  idl_file: "idl/Gamepad.idl"
  domain_id: 0

mapping:
  - name: steering
    id: 2
    field: x
    to: axis:left_x
    scale: 1.0
    deadzone: 0.05
    invert: false
```

### Mapping notes

- Place one YAML file per DDS topic in the config directory.
- Each config must include exactly one mapping entry.
- `id` matches `role_id` values coming from DDS.
- `field` supports `value` for `Gamepad_Analog` and `x` for `Stick_TwoAxis` (steering uses `x` only).
- `to` supports `axis:left_trigger`, `axis:right_trigger`, `axis:left_x`, `axis:left_y`, `axis:right_x`, `axis:right_y`.
- `scale` is applied before `deadzone` and `invert`.
- `invert` flips axis direction for sticks or maps triggers as `1.0 - value`.

## ViGEm Sanity Check

The `vigem_sanity` executable creates a virtual Xbox 360 controller and sets the right trigger to a fixed value for a few seconds. Ensure the ViGEmBus driver is installed, then run:

`.\install\simple-dds\bin\vigem_sanity.exe`

## Install

To create a portable installer you can copy to another Windows machine, produce the install tree and zip it. The install tree will include the application binaries, configuration files and any optional installer binaries (Visual C++ redistributable and ViGEmBus) if present in `external/`.

1. Build and run the install target (Release recommended):

```powershell
cmake --build build --config Release --target install
```

2. Create a ZIP of the install folder (example from repository root):

```powershell
Compress-Archive -Path install\dds-gamepad\* -DestinationPath release\dds-gamepad-install.zip -Force
```

3. Copy the ZIP to the target machine and extract it (e.g. `C:\dds-gamepad`).

4. On the target machine, run the required installers (run as Administrator):

- Install the Visual C++ Redistributable (x64):

```powershell
.\installers\vc_redist.x64.exe /install /quiet /norestart
```

- Install the ViGEmBus driver (required for virtual controllers):

```powershell
.\installers\ViGEmBus_1.22.0_x64_x86_arm64.exe
```

5. Start the application (example):

```powershell
cd bin
.\dds-gamepad.exe ..\config
```

Notes:
- If the `installers` folder is missing from the install tree, place the `vc_redist.x64.exe` and ViGEmBus installer in the `installers` folder before running the installers.
- The app expects Release builds of third-party DLLs (no `*D.dll` debug runtimes) — ship Release artifacts only.
- Include the `config` folder contents when packaging so `dds-gamepad.exe` can find YAML files describing topics and mappings.
