#pragma once

#include <cstdint>
#include <string>

#include "emulator/IOutputDevice.h"

namespace emulator {

class VigemClient final : public IOutputDevice {
public:
    VigemClient();
    ~VigemClient() override;

    VigemClient(const VigemClient&) = delete;
    VigemClient& operator=(const VigemClient&) = delete;
    VigemClient(VigemClient&&) = delete;
    VigemClient& operator=(VigemClient&&) = delete;

    void Connect() override;
    void AddX360Controller();
    bool UpdateRightTrigger(uint8_t value);  // used by vigem_sanity; not part of IOutputDevice
    bool UpdateState(const common::OutputState& state) override;
    std::string LastError() const override;

    void SetLogState(bool enabled) override;
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
