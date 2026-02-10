# DDS GamePad Installer (Windows)

This install folder contains the helper script to install and manage the **dds-gamepad Windows Service**.

These instructions are written to work from **any folder you unzip to**.

## What’s in this ZIP

- `bin\dds-gamepad.exe` — console app (runs in a terminal)
- `bin\dds-gamepad-service.exe` — Windows Service binary
- `bin\config\driver.yaml` / `bin\config\gunner.yaml` — role configuration files
- `install_service.ps1` — install/uninstall/start/stop helper
- `installers\` — optional prerequisites installers (may be present)

## Prerequisites (run once on the machine)

1) **Visual C++ Redistributable (x64)**

If `installers\vc_redist.x64.exe` exists, run (Administrator recommended):

```powershell
.\installers\vc_redist.x64.exe /install /quiet /norestart
```

2) **ViGEmBus driver** (required for the virtual Xbox 360 controller)

If `installers\ViGEmBus_*.exe` exists, run (Administrator):

```powershell
.\installers\ViGEmBus_1.22.0_x64_x86_arm64.exe
```

If the `installers\` folder is missing, obtain these from your IT/package source.

## Install as a Windows Service

1) Open **PowerShell as Administrator**.
2) `cd` to the folder where you extracted the ZIP.

Example:

```powershell
cd C:\dds-gamepad
```

3) Install the service (uses the extracted folder as the install root):

```powershell
.\install_service.ps1 -Action Install -InstallDir (Resolve-Path .).Path -DomainId 0 -YokeId 1004 -ConfigFilePath "bin\config\driver.yaml" -StartType Automatic
```

4) Start the service:

```powershell
.\install_service.ps1 -Action Start
```

5) Check service status:

```powershell
.\install_service.ps1 -Action Status
```

### Stop / restart / uninstall

```powershell
.\install_service.ps1 -Action Stop
.\install_service.ps1 -Action Restart
.\install_service.ps1 -Action Uninstall
```

## Configuration

- The service reads a single config file (default): `bin\config\driver.yaml`.
- To run a different role, reinstall with `-ConfigFilePath` set to another role file (for example `bin\config\gunner.yaml`).
- After changing config, restart the service:

```powershell
.\install_service.ps1 -Action Restart
```

## Logs / troubleshooting

The service writes lifecycle/errors to **Windows Event Log**:

- Event Viewer → **Windows Logs** → **Application**
- Source: `dds-gamepad-service`

Common checks:

- Confirm the service executable exists: `bin\dds-gamepad-service.exe`
- Confirm config exists (example): `bin\config\driver.yaml`
- Confirm prerequisites installed (VC++ redist + ViGEmBus)

## Run in console (non-service)

If you want to run interactively (useful for debugging), from the ZIP root:

```powershell
.\bin\dds-gamepad.exe .\bin\config\driver.yaml 0 1004 --table
```
Where `0` is the DDS domain id and `1004` is the yoke sub_role id to accept.
