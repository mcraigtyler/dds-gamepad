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

The main `dds-boilerplate` executable subscribes to a DDS topic carrying `Value::Msg` samples. Incoming values are expected to be in the `0.0` to `1.0` range and are scaled to the Xbox 360 right trigger range (`0` to `255`).

`.\install\boilerplate-dds\bin\dds-boilerplate.exe <dds_topic> [domain_id]`

## ViGEm Sanity Check

The `vigem_sanity` executable creates a virtual Xbox 360 controller and sets the right trigger to a fixed value for a few seconds. Ensure the ViGEmBus driver is installed, then run:

`.\install\simple-dds\bin\vigem_sanity.exe`
