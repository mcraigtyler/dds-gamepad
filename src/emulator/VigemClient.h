#pragma once

#include <cstdint>
#include <string>

#include "mapper/GamepadState.h"

namespace emulator {
class IVigemClient {
public:
    virtual ~IVigemClient() = default;
    virtual bool Connect() = 0;
    virtual bool AddX360Controller() = 0;
    virtual bool UpdateRightTrigger(uint8_t value) = 0;
    virtual bool UpdateState(const mapper::GamepadState& state) = 0;
    virtual std::string LastError() const = 0;
};

class VigemClient final : public IVigemClient {
public:
    VigemClient();
    ~VigemClient() override;

    VigemClient(const VigemClient&) = delete;
    VigemClient& operator=(const VigemClient&) = delete;
    VigemClient(VigemClient&&) = delete;
    VigemClient& operator=(VigemClient&&) = delete;

    bool Connect() override;
    bool AddX360Controller() override;
    bool UpdateRightTrigger(uint8_t value) override;
    bool UpdateState(const mapper::GamepadState& state) override;
    std::string LastError() const override;

private:
    void SetError(const std::string& message);
    void ResetError();
    void Disconnect();

    void* client_;
    void* target_;
    std::string last_error_;
    bool connected_;
    bool controller_added_;
};
}  // namespace emulator
