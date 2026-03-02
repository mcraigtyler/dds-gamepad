#pragma once

#include <cstdint>
#include <string>

#include "emulator/IOutputDevice.h"

namespace emulator {

/// @brief `IOutputDevice` implementation that drives a virtual Xbox 360
///        controller via the ViGEmBus driver.
///
/// @details `VigemClient` wraps the ViGEmClient SDK. It is non-copyable and
/// non-movable because the underlying `PVIGEM_CLIENT` and `PVIGEM_TARGET`
/// handles are opaque raw pointers managed by the SDK.
///
/// **Startup sequence** (both steps are required):
/// 1. `Connect()` — allocates the ViGEmBus client handle and connects to the
///    driver. Throws if ViGEmBus is not installed.
/// 2. `AddX360Controller()` — allocates and plugs in a virtual Xbox 360 pad.
///    Throws if the controller cannot be added. Must be called after `Connect`.
///
/// The convenience `AppRunner::Run` overload calls both steps automatically.
/// When using the injectable `Run` overload for testing, call both steps on
/// the `VigemClient` before passing it in.
///
/// `UpdateState` converts `OutputState::channels` to an `XUSB_REPORT` and
/// submits it to the virtual controller every call.
class VigemClient final : public IOutputDevice {
public:
    VigemClient();
    ~VigemClient() override;

    VigemClient(const VigemClient&)            = delete;
    VigemClient& operator=(const VigemClient&) = delete;
    VigemClient(VigemClient&&)                 = delete;
    VigemClient& operator=(VigemClient&&)      = delete;

    // ── Lifecycle ──────────────────────────────────────────────────────────

    /// @brief Connects to the ViGEmBus driver.
    /// @throws std::runtime_error If the ViGEmBus driver is not installed or
    ///         the client handle cannot be allocated.
    void Connect() override;

    /// @brief Allocates and plugs in a virtual Xbox 360 controller.
    /// @pre `Connect()` must have been called successfully.
    /// @throws std::runtime_error If the controller target cannot be created
    ///         or plugged in.
    void AddX360Controller();

    // ── Hot-path ───────────────────────────────────────────────────────────

    /// @brief Updates the virtual controller with the current output state.
    /// @param[in] state Normalised channel map. Keys must use `vigem_x360`
    ///            conventions (e.g. `"axis:left_x"`, `"button:a"`).
    /// @return `true` on success; `false` on ViGEm API failure.
    bool UpdateState(const common::OutputState& state) override;

    /// @brief Returns the last error from `UpdateState`.
    /// @return Non-empty string after a failed call; empty string otherwise.
    std::string LastError() const override;

    // ── Utilities ──────────────────────────────────────────────────────────

    /// @brief Directly updates the right trigger of the virtual controller.
    /// @param[in] value Raw trigger value in [0, 255].
    /// @return `true` on success.
    /// @note Used only by the `vigem_sanity` executable for driver validation.
    ///       Not part of the `IOutputDevice` interface.
    bool UpdateRightTrigger(uint8_t value);

    // ── Optional hooks ─────────────────────────────────────────────────────

    /// @brief Enables or disables per-frame TX state logging to `stdout`.
    /// @param[in] enabled `true` to log each submitted `XUSB_REPORT`.
    void SetLogState(bool enabled) override;

    /// @brief Registers an observer for TX state callbacks.
    /// @param[in] listener Observer notified after each successful `UpdateState`.
    ///            Pass `nullptr` to deregister. Ownership is not transferred.
    void SetTxStateListener(ITxStateListener* listener) override;

private:
    void SetError(const std::string& message);
    void ResetError();
    void Disconnect();

    void* client_;
    void* target_;
    std::string last_error_;
    bool connected_;
    bool controller_added_;
    bool log_state_;
    ITxStateListener* tx_state_listener_;
};

}  // namespace emulator
