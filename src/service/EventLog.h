#pragma once

#include <Windows.h>

#include <string>

namespace service
{

/// @brief Thin wrapper around the Windows Event Log API.
///
/// @details `EventLog` writes messages to the Windows Application event log
/// under a named source. It is used by the `dds-gamepad-service` executable to
/// report startup, shutdown, and error events in a form visible via
/// Windows Event Viewer.
///
/// **Prerequisites:** The event source name must be registered in the Windows
/// registry before messages can be written. The `install_service.ps1` script
/// handles this registration automatically during service installation.
///
/// All public methods are `noexcept` — log failures are silently swallowed so
/// that logging never interrupts the service lifecycle.
///
/// @note This class is Windows-only and must not be used in cross-platform code.
class EventLog
{
public:
    /// @brief Opens a handle to the Windows Event Log for the given source.
    /// @param[in] sourceName Event source name as registered in the registry
    ///            (e.g. `L"dds-gamepad-service"`). Must remain valid for the
    ///            duration of this object's lifetime.
    /// @note If the source is not registered the handle may be invalid;
    ///       subsequent write calls will fail silently.
    explicit EventLog(const wchar_t* sourceName) noexcept;

    /// @brief Deregisters the event source handle.
    ~EventLog() noexcept;

    EventLog(const EventLog&)            = delete;
    EventLog& operator=(const EventLog&) = delete;

    // ── Logging ────────────────────────────────────────────────────────────

    /// @brief Writes an informational message to the event log.
    /// @param[in] message Human-readable message text.
    void Info(const std::wstring& message) noexcept;

    /// @brief Writes a warning message to the event log.
    /// @param[in] message Human-readable message text.
    void Warning(const std::wstring& message) noexcept;

    /// @brief Writes an error message to the event log.
    /// @param[in] message Human-readable message text.
    void Error(const std::wstring& message) noexcept;

private:
    void Write(WORD type, const std::wstring& message) noexcept;

private:
    HANDLE _handle;
};

} // namespace service
