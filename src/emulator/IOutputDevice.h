#pragma once

#include <string>

#include "mapper/GamepadState.h"

namespace emulator {

// Observer for outbound gamepad state updates.  Used by the console live-table
// to display the current TX state line.  Kept here (rather than in VigemClient.h)
// so AppRunner can define TableTxStateListener without coupling to VigemClient.
class ITxStateListener {
public:
    virtual ~ITxStateListener() = default;
    virtual void OnTxState(const mapper::GamepadState& state) = 0;
};

// Generic output-device interface.
// Implementations write the current GamepadState to a physical or virtual
// destination (ViGEmBus, UDP socket, …).
// NOTE: Phase 2 will replace GamepadState with the backend-agnostic OutputState.
class IOutputDevice {
public:
    virtual ~IOutputDevice() = default;
    // Startup — throws std::runtime_error on failure.
    virtual void Connect() = 0;
    // Hot-path update — returns false on failure; inspect LastError() for details.
    virtual bool UpdateState(const mapper::GamepadState& state) = 0;
    virtual std::string LastError() const = 0;
    // Optional: default no-ops so minimal implementations need not override.
    virtual void SetLogState(bool) {}
    virtual void SetTxStateListener(ITxStateListener*) {}
};

}  // namespace emulator
