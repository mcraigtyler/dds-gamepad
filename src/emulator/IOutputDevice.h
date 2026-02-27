#pragma once

#include <string>

#include "common/OutputState.h"

namespace emulator {

/// @brief Observer notified on every successful outbound gamepad state update.
///
/// @details Implement this interface to receive a callback each time
/// `IOutputDevice::UpdateState` pushes a new frame to the backend.
/// The primary consumer is `AppRunner`'s internal `TableTxStateListener`,
/// which forwards the state to `RxTable::SetTxStateLine` for the live
/// console dashboard.
///
/// Kept in `IOutputDevice.h` rather than `VigemClient.h` so `AppRunner`
/// can define a concrete listener without coupling to the ViGEmBus backend.
class ITxStateListener {
public:
    virtual ~ITxStateListener() = default;

    /// @brief Called after each successful `UpdateState` submission.
    /// @param[in] state The normalised output state that was just sent to
    ///            the backend.
    virtual void OnTxState(const common::OutputState& state) = 0;
};

/// @brief Generic output-device abstraction for gamepad emulation backends.
///
/// @details Implementations write the current `OutputState` to a physical or
/// virtual destination — ViGEmBus (Xbox 360 controller), a UDP socket, or a
/// test mock. `AppRunner` holds a reference to this interface and never
/// depends on the concrete backend type.
///
/// **Lifecycle:**
/// 1. Construct the concrete implementation.
/// 2. Call `Connect()` — throws `std::runtime_error` on failure.
/// 3. Call `UpdateState()` on every read-loop iteration.
///
/// @section optional_hooks Optional Hooks
/// `SetLogState` and `SetTxStateListener` have default no-op implementations
/// so minimal backends (e.g. test stubs) need not override them.
class IOutputDevice {
public:
    virtual ~IOutputDevice() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────

    /// @brief Opens the connection to the output backend.
    /// @throws std::runtime_error If the backend cannot be initialised
    ///         (e.g. ViGEmBus driver not installed, socket bind failure).
    virtual void Connect() = 0;

    // ── Hot-path ───────────────────────────────────────────────────────────

    /// @brief Pushes the current normalised channel state to the backend.
    ///
    /// @details Called once per read-loop iteration by `AppRunner` after
    /// `MappingEngine::Apply` has updated the `OutputState`. This is the
    /// hot path — avoid allocations or blocking I/O in implementations.
    ///
    /// @param[in] state Normalised output state. Channel keys are the
    ///            `output.to` strings from the YAML config (e.g.
    ///            `"axis:left_x"`, `"button:a"`).
    /// @return `true` on success.
    /// @retval false On a recoverable error; inspect `LastError()` for details.
    virtual bool UpdateState(const common::OutputState& state) = 0;

    /// @brief Returns the last error message produced by `UpdateState`.
    /// @return A non-empty string when the most recent `UpdateState` call
    ///         returned `false`; an empty string otherwise.
    virtual std::string LastError() const = 0;

    // ── Optional hooks ─────────────────────────────────────────────────────

    /// @brief Enables or disables per-frame console logging of TX state.
    /// @param[in] enabled `true` to log each submitted state to `stdout`.
    /// @note Default implementation is a no-op.
    virtual void SetLogState(bool enabled) {}

    /// @brief Registers an observer for TX state callbacks.
    /// @param[in] listener Observer to notify on each successful `UpdateState`.
    ///            Pass `nullptr` to deregister. Ownership is not transferred.
    /// @note Default implementation is a no-op.
    virtual void SetTxStateListener(ITxStateListener* listener) {}
};

}  // namespace emulator
