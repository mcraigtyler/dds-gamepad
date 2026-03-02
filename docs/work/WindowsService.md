# Windows Service

## Summary
Today, `dds-gamepad` runs from a console and expects CLI arguments (`<config_dir> <domain_id> [--debug|--table]`). We need an additional way to run the same behavior as a **Windows Service**, so the app can be run either:

- interactively from a console (existing behavior), or
- headlessly as a background service managed by the Windows Service Control Manager (SCM).

This document defines the requirements, constraints, and acceptance criteria for adding a Windows Service target.

## Goals

- Provide a Windows Service entrypoint that runs the same core logic as the console app.
- Support start/stop via SCM with **graceful shutdown** (no forced kill required).
- Support deployment/operation on a typical Windows machine with minimal manual steps.

## Non-Goals

- Building a full MSI installer.
- Adding a UI for configuration.
- Supporting non-Windows platforms.

## Current Behavior (Baseline)

- Built target: `dds-gamepad` (console executable).
- CLI: `dds-gamepad <config_dir> <domain_id> [--debug | --table]`.
- Reads YAML configs from a directory (sorted `.yaml`/`.yml`).
- Connects to ViGEm, adds an Xbox 360 virtual controller, then continuously reads DDS samples and updates controller state.
- Runtime loop currently runs indefinitely and has no explicit cancellation/stop path.

## Proposed Deliverables

### 1) Additional Windows Service Target

- A new build target (recommended: `dds-gamepad-service.exe`) that:
	- registers with SCM,
	- reports service state transitions (START_PENDING → RUNNING → STOP_PENDING → STOPPED), and
	- runs the same application logic as the console mode.

### 2) Shared “Core Runner”

- The service and console entrypoints should share the same “runner” implementation so behavior stays consistent.
- The shared runner must support a cancellation signal (e.g., atomic flag/event) to allow clean exit.

## Functional Requirements

### Service Lifecycle

- **Start**: When SCM starts the service, it begins normal processing (loads config, connects to ViGEm, creates DDS participant/subscribers, processes samples).
- **Stop**: When SCM requests stop:
	- The service must stop the main loop within a bounded time (target: within a few seconds; exact value TBD).
	- The service must clean up resources (disconnect controller, dispose DDS entities if needed) and then report STOPPED.
- **Failure behavior**:
	- If startup fails (invalid config, ViGEm connect fails, DDS participant creation fails), the service should log an error and exit with a non-zero code / STOPPED status.
	- Optional (TBD): retry a few times on transient failures before giving up.

### Configuration (Service Mode)

The console app requires:

- `config_dir` (directory containing YAML configs)
- `domain_id` (DDS domain id)

The service mode must have an unambiguous way to supply these values.

Desired behavior:

- In service mode, the service reads configuration from a directory **next to the service executable**.
- The service must not rely on the process working directory.

Proposed convention:

- `config_dir` = `<service_exe_dir>\config\`
- `domain_id` = baked into the service install configuration (e.g., passed as a command-line argument in the service `binPath`).

Example service `binPath`:

- `dds-gamepad-service.exe --domain-id 0`

Notes:

- In service mode, `--table` and `--debug` are **no-ops**.

### Logging & Diagnostics

Services do not have an interactive console, so we must define where logs go.

Minimum requirement:

- Service mode must write human-readable logs somewhere persistent.

Chosen log sink (MVP):

- **Windows Event Log**.

Logging policy:

- Log service lifecycle events (start requested, running, stop requested, stopped).
- Log startup failures and runtime errors.
- Do not log per-message/per-sample RX/TX activity (avoid flooding the Event Log).

Log content should include:

- startup configuration (sanitized),
- subscription topics, domain id,
- ViGEm connection status,
- fatal errors + reason,
- stop requested / stop completed.

### Service Identity

- Define and use a stable:
	- **Service Name** (SCM internal name)
	- **Display Name** (what shows in Services.msc)
	- Optional description

Chosen identity:

- Service Name: `dds-gamepad-service`
- Display Name: `dds-gamepad-service`

### Installation / Uninstallation

Provide one supported installation path (scripted is fine):

- A PowerShell script (recommended) that can:
	- install the service,
	- set startup type (Automatic / Manual; default TBD),
	- start/stop the service,
	- uninstall/cleanly remove it.

The install docs/scripts must call out prerequisites:

- ViGEmBus driver must be installed and working.
- The service account must have permissions to access:
	- the config directory,
	- log directory,
	- the ViGEm driver.

### Shutdown Semantics

- The core loop must periodically check for a stop signal and exit.
- The service stop handler must not block indefinitely.

## Operational Considerations

- **Service account**: default is `LocalSystem` unless there’s a reason to run as a specific user.
- **Working directory**: the service should not assume a working directory; use absolute paths.
- **Config location**: config lives next to the `.exe`.
- **Recovery**: configure SCM recovery actions to restart on failure, but not indefinitely.
	- Retry policy: restart up to **3 times over 1 minute**.
	- If failures continue beyond the retry limit, stop retrying and log the repeated failure.

## Acceptance Criteria

- A new Windows Service target is buildable alongside the existing console executable.
- The service can be installed via a documented script/commands.
- Starting the service results in normal app behavior (config loaded, DDS subscriptions created, virtual controller active).
- Stopping the service cleanly stops the loop and returns service status to STOPPED without needing Task Manager.
- Service mode logs are accessible in the chosen log sink and include startup + shutdown markers and error context.

## Relevant Files

- `docs/work/WindowsService.md` - Requirements and acceptance criteria for the service deliverable.
- `CMakeLists.txt` - Add an additional build target for the service (and possibly a shared core library).
- `src/main.cpp` - Current console entrypoint; will likely be refactored to share core run logic.
- `src/config/ConfigLoader.cpp` - Existing config loading behavior the service must reuse.
- `src/emulator/VigemClient.cpp` / `src/emulator/VigemClient.h` - ViGEm connection lifecycle; needs clean shutdown semantics.
- `src/service/ServiceMain.cpp` - New: SCM glue (ServiceMain, control handler, status reporting).
- `src/app/AppRunner.cpp` / `src/app/AppRunner.h` - New: shared “core runner” with cancellation support.
- `Scripts/install_service.ps1` - New: install/start/stop/uninstall helper for the Windows service.
- `README.md` - Document how to run console vs service.

## Tasks

- [x] 1.0 Refactor core runtime into a reusable runner
- [x] 2.0 Implement Windows Service entrypoint and SCM integration
- [x] 3.0 Add service-mode configuration + logging strategy
- [x] 4.0 Add install/uninstall/start/stop scripts and documentation