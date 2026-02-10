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

The `dds-gamepad` executable now accepts a **single role config file**, domain id, and yoke id.

```powershell
.\install\boilerplate-dds\bin\dds-gamepad.exe <config_file> <domain_id> <yoke_id>
```

Example:

```powershell
.\install\boilerplate-dds\bin\dds-gamepad.exe .\install\boilerplate-dds\config\driver.yaml 0 1004 --table
```

### Role-based config schema

Configs are role-based (`config/driver.yaml`, `config/gunner.yaml`) and use this shape:

```yaml
role:
  name: "Driver"
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
    gamepad:
      to: axis:right_x
      scale: 1.0
      deadzone: 0.02
      invert: false
```

### Mapping notes

- One role config file can contain multiple mappings across topics.
- `dds.id` matches DDS role id values.
- `yoke_id` is no longer configured in YAML; pass it on the command line as `<yoke_id>` (or `--yoke-id` for service mode).
- `dds.field` supports `value`, `x`, and `y` depending on DDS type.
- `gamepad.to` supports `axis:left_trigger`, `axis:right_trigger`, `axis:left_x`, `axis:left_y`, `axis:right_x`, `axis:right_y`.
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
.\dds-gamepad.exe ..\config\driver.yaml 0 1004
```

## Windows Service

The install tree also includes a Windows Service binary: `bin\dds-gamepad-service.exe`.

Service conventions:
- Config file: defaults to `bin\config\driver.yaml` (next to the service executable)
- Domain id: supplied via the service `binPath` argument `--domain-id <n>`
- Yoke id: supplied via the service `binPath` argument `--yoke-id <sub_role>`
- Logs: Windows Event Log (Application) under source `dds-gamepad-service`

To install/start/stop the service (run PowerShell as Administrator):

```powershell
cd C:\dds-gamepad
.\install_service.ps1 -Action Install -InstallDir (Get-Location).ProviderPath -DomainId 0 -YokeId 1004 -ConfigFilePath "bin\config\driver.yaml" -StartType Automatic
.\install_service.ps1 -Action Start
```

To stop/uninstall:

```powershell
.\install_service.ps1 -Action Stop
.\install_service.ps1 -Action Uninstall
```

Notes:
- If the `installers` folder is missing from the install tree, place the `vc_redist.x64.exe` and ViGEmBus installer in the `installers` folder before running the installers.
- The app expects Release builds of third-party DLLs (no `*D.dll` debug runtimes) — ship Release artifacts only.
- Include `config\driver.yaml` and `config\gunner.yaml` when packaging so both console and service modes can load role mappings.
