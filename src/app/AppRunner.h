#pragma once

#include <string>

#include "app/StopToken.h"

namespace emulator { class IOutputDevice; }

namespace app
{

/// @brief Configuration bundle for an `AppRunner` session.
///
/// @details All fields are consumed once at startup inside `AppRunner::Run`.
/// Console mode sets all logging flags to `true` by default. Windows service
/// mode should set `logStartup`, `logRx`, and `logTxState` to `false` to
/// suppress per-sample `stdout` noise, which is not visible in service context
/// anyway (use the Windows Event Log instead).
struct AppRunnerOptions
{
    std::string configFile;      ///< Path to the role YAML config file. Required.
    int domainId = 0;            ///< CycloneDDS domain ID. Must match the DDS publisher.
    int yokeId = 0;              ///< Hardware yoke ID. Filters messages by `sub_role` field.
    bool logRxRaw   = false;     ///< Log raw pre-mapping DDS values. Very verbose; use with `--debug`.
    bool tableMode  = false;     ///< Enable non-scrolling live dashboard via `RxTable`.
    // Console defaults preserve previous behavior.
    // Service mode should typically set these to false.
    bool logStartup = true;      ///< Print config summary and subscription list at startup.
    bool logRx      = true;      ///< Log mapped receive events to `stdout`.
    bool logTxState = true;      ///< Log TX state to `stdout` after each `UpdateState` call.
};

/// @brief Orchestrates the DDS read loop and dispatches to the output backend.
///
/// @details `AppRunner` is the central coordinator of the application. It:
/// - Loads the YAML configuration via `config::ConfigLoader::Load`.
/// - Creates `TopicHandler` instances for each configured DDS subscription.
/// - Runs the polling loop: read samples → `mapper::MappingEngine::Apply` →
///   `emulator::IOutputDevice::UpdateState`.
/// - Drives `console::RxTable` and `StatusPoller` when `tableMode` is enabled.
///
/// It is shared by both the console entry point (`main.cpp`) and the Windows
/// service (`ServiceMain.cpp`). The service passes a `StopToken` tied to
/// `SERVICE_CONTROL_STOP`; the console blocks until Ctrl-C sets the token.
///
/// @section usage Usage
/// @code
/// app::StopSource source;
/// app::AppRunner  runner;
///
/// app::AppRunnerOptions opts;
/// opts.configFile = "config/driver.yaml";
/// opts.domainId   = 0;
/// opts.yokeId     = 1004;
/// opts.tableMode  = true;
///
/// int rc = runner.Run(opts, source.Token());
/// if (rc != EXIT_SUCCESS)
///     std::cerr << runner.LastError() << "\n";
/// @endcode
class AppRunner
{
public:
    // ── Run overloads ──────────────────────────────────────────────────────

    /// @brief Convenience overload that constructs the output backend internally.
    ///
    /// @details Reads `output.type` from the YAML config to select the backend:
    /// - `"vigem_x360"` (default): constructs `emulator::VigemClient`, calls
    ///   `Connect()` and `AddX360Controller()`, then delegates to the core overload.
    /// - `"udp_protobuf"`: constructs `emulator::UdpProtobufEmulator` (Phase 5
    ///   stub), calls `Connect()`, then delegates to the core overload.
    ///
    /// @param[in] options   Session configuration.
    /// @param[in] stopToken Signals when the loop should exit.
    /// @return `EXIT_SUCCESS` on clean shutdown; `EXIT_FAILURE` on error.
    ///         Call `LastError()` for a human-readable description.
    int Run(const AppRunnerOptions& options, const StopToken& stopToken);

    /// @brief Core overload — caller supplies the output device.
    ///
    /// @details Enables dependency injection for testing: pass a mock
    /// `IOutputDevice` to verify mapping behaviour without ViGEmBus installed.
    /// `Connect()` must have already been called on the device before invoking
    /// this overload directly.
    ///
    /// @param[in]     options   Session configuration.
    /// @param[in,out] client    Output backend. Must already be connected.
    /// @param[in]     stopToken Signals when the loop should exit.
    /// @return `EXIT_SUCCESS` on clean shutdown; `EXIT_FAILURE` on error.
    int Run(const AppRunnerOptions& options,
            emulator::IOutputDevice& client,
            const StopToken& stopToken);

    // ── Error reporting ────────────────────────────────────────────────────

    /// @brief Returns the last error recorded during `Run`.
    /// @return Human-readable description of the failure, or an empty string
    ///         if the last run completed successfully.
    const std::string& LastError() const noexcept;

private:
    void SetLastError(const std::string& error);

private:
    std::string _lastError;
};

} // namespace app
