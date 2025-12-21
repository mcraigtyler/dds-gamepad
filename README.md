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

The main `dds-gamepad` executable reads a YAML config that defines the DDS topic plus mapping rules. Incoming values are expected to be in the `0.0` to `1.0` range and are scaled to the target control range.

`.\install\boilerplate-dds\bin\dds-gamepad.exe <config.yaml> [domain_id]`

### Example config

```yaml
dds:
  topic: "vehicle.inputs"
  type: "Value::Msg"
  idl_file: "idl/Value.idl"
  domain_id: 0

mapping:
  - name: throttle
    id: 1
    field: value
    to: axis:right_trigger
    scale: 1.0
    deadzone: 0.05
    invert: false
  - name: brake
    id: 2
    field: value
    to: axis:left_trigger
    scale: 1.0
    deadzone: 0.05
  - name: steering
    id: 3
    field: value
    to: axis:left_x
    scale: 2.0
    deadzone: 0.02
    invert: true
```

### Mapping notes

- `id` matches `Value::Msg::messageID` values coming from DDS.
- `field` currently supports `value`.
- `to` supports `axis:left_trigger`, `axis:right_trigger`, `axis:left_x`, `axis:left_y`, `axis:right_x`, `axis:right_y`.
- `scale` is applied before `deadzone` and `invert`.
- `invert` flips axis direction for sticks or maps triggers as `1.0 - value`.

## ViGEm Sanity Check

The `vigem_sanity` executable creates a virtual Xbox 360 controller and sets the right trigger to a fixed value for a few seconds. Ensure the ViGEmBus driver is installed, then run:

`.\install\simple-dds\bin\vigem_sanity.exe`
