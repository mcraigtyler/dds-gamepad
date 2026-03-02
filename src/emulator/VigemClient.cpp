#include "emulator/VigemClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <ViGEm/Client.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace emulator {
namespace {
std::string StatusToString(VIGEM_ERROR status) {
    switch (status) {
        case VIGEM_ERROR_NONE:
            return "VIGEM_ERROR_NONE";
        case VIGEM_ERROR_BUS_NOT_FOUND:
            return "VIGEM_ERROR_BUS_NOT_FOUND";
        case VIGEM_ERROR_NO_FREE_SLOT:
            return "VIGEM_ERROR_NO_FREE_SLOT";
        case VIGEM_ERROR_INVALID_TARGET:
            return "VIGEM_ERROR_INVALID_TARGET";
        case VIGEM_ERROR_REMOVAL_FAILED:
            return "VIGEM_ERROR_REMOVAL_FAILED";
        case VIGEM_ERROR_ALREADY_CONNECTED:
            return "VIGEM_ERROR_ALREADY_CONNECTED";
        case VIGEM_ERROR_TARGET_UNINITIALIZED:
            return "VIGEM_ERROR_TARGET_UNINITIALIZED";
        case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN:
            return "VIGEM_ERROR_TARGET_NOT_PLUGGED_IN";
        case VIGEM_ERROR_BUS_VERSION_MISMATCH:
            return "VIGEM_ERROR_BUS_VERSION_MISMATCH";
        case VIGEM_ERROR_BUS_ACCESS_FAILED:
            return "VIGEM_ERROR_BUS_ACCESS_FAILED";
        case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED:
            return "VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED";
        case VIGEM_ERROR_CALLBACK_NOT_FOUND:
            return "VIGEM_ERROR_CALLBACK_NOT_FOUND";
        case VIGEM_ERROR_BUS_ALREADY_CONNECTED:
            return "VIGEM_ERROR_BUS_ALREADY_CONNECTED";
        case VIGEM_ERROR_NOT_SUPPORTED:
            return "VIGEM_ERROR_NOT_SUPPORTED";
        case VIGEM_ERROR_TIMED_OUT:
            return "VIGEM_ERROR_TIMED_OUT";
        default: {
            std::ostringstream stream;
            stream << "VIGEM_ERROR_UNKNOWN(" << static_cast<int>(status) << ")";
            return stream.str();
        }
    }
}

// XUSB_REPORT button/dpad masks
constexpr uint16_t kDpadUpMask    = 0x0001;
constexpr uint16_t kDpadDownMask  = 0x0002;
constexpr uint16_t kDpadLeftMask  = 0x0004;
constexpr uint16_t kDpadRightMask = 0x0008;
constexpr uint16_t kButtonAMask   = 0x1000;
constexpr uint16_t kButtonBMask   = 0x2000;
constexpr uint16_t kButtonXMask   = 0x4000;
constexpr uint16_t kButtonYMask   = 0x8000;

int16_t AxisFromNormalized(float value) {
    if (value <= -1.0f) {
        return std::numeric_limits<int16_t>::min();
    }
    if (value >= 1.0f) {
        return std::numeric_limits<int16_t>::max();
    }
    return static_cast<int16_t>(std::lround(value * static_cast<float>(std::numeric_limits<int16_t>::max())));
}

uint8_t TriggerFromNormalized(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(clamped * 255.0f));
}

// Builds an XUSB_REPORT from the generic OutputState channels.
// Channel keys are the raw "output.to" strings from the YAML config.
// Unknown channels are silently ignored.
XUSB_REPORT BuildReport(const common::OutputState& state) {
    XUSB_REPORT report;
    XUSB_REPORT_INIT(&report);
    for (const auto& [key, val] : state.channels) {
        if (key == "axis:left_trigger") {
            report.bLeftTrigger = TriggerFromNormalized(val);
        } else if (key == "axis:right_trigger") {
            report.bRightTrigger = TriggerFromNormalized(val);
        } else if (key == "axis:left_x") {
            report.sThumbLX = AxisFromNormalized(val);
        } else if (key == "axis:left_y") {
            report.sThumbLY = AxisFromNormalized(val);
        } else if (key == "axis:right_x") {
            report.sThumbRX = AxisFromNormalized(val);
        } else if (key == "axis:right_y") {
            report.sThumbRY = AxisFromNormalized(val);
        } else if (key == "button:a") {
            if (val > 0.5f) report.wButtons |= kButtonAMask;
            else report.wButtons &= static_cast<uint16_t>(~kButtonAMask);
        } else if (key == "button:b") {
            if (val > 0.5f) report.wButtons |= kButtonBMask;
            else report.wButtons &= static_cast<uint16_t>(~kButtonBMask);
        } else if (key == "button:x") {
            if (val > 0.5f) report.wButtons |= kButtonXMask;
            else report.wButtons &= static_cast<uint16_t>(~kButtonXMask);
        } else if (key == "button:y") {
            if (val > 0.5f) report.wButtons |= kButtonYMask;
            else report.wButtons &= static_cast<uint16_t>(~kButtonYMask);
        } else if (key == "dpad:up") {
            if (val > 0.5f) report.wButtons |= kDpadUpMask;
            else report.wButtons &= static_cast<uint16_t>(~kDpadUpMask);
        } else if (key == "dpad:down") {
            if (val > 0.5f) report.wButtons |= kDpadDownMask;
            else report.wButtons &= static_cast<uint16_t>(~kDpadDownMask);
        } else if (key == "dpad:left") {
            if (val > 0.5f) report.wButtons |= kDpadLeftMask;
            else report.wButtons &= static_cast<uint16_t>(~kDpadLeftMask);
        } else if (key == "dpad:right") {
            if (val > 0.5f) report.wButtons |= kDpadRightMask;
            else report.wButtons &= static_cast<uint16_t>(~kDpadRightMask);
        }
        // Unknown channels are silently ignored.
    }
    return report;
}
}  // namespace

VigemClient::VigemClient()
    : client_(nullptr),
      target_(nullptr),
      last_error_(),
      connected_(false),
    controller_added_(false),
    log_state_(false),
    tx_state_listener_(nullptr) {}

VigemClient::~VigemClient() {
    Disconnect();
}

void VigemClient::Connect() {
    if (!client_) {
        client_ = vigem_alloc();
        if (!client_) {
            throw std::runtime_error("Failed to allocate ViGEm client.");
        }
    }

    if (connected_) {
        return;
    }

    const auto status = vigem_connect(static_cast<PVIGEM_CLIENT>(client_));
    if (!VIGEM_SUCCESS(status)) {
        throw std::runtime_error("vigem_connect failed: " + StatusToString(status));
    }

    connected_ = true;
}

void VigemClient::AddX360Controller() {
    if (!connected_) {
        throw std::runtime_error("ViGEm client is not connected.");
    }

    if (!target_) {
        target_ = vigem_target_x360_alloc();
        if (!target_) {
            throw std::runtime_error("Failed to allocate Xbox 360 target.");
        }
    }

    if (controller_added_) {
        return;
    }

    const auto status = vigem_target_add(static_cast<PVIGEM_CLIENT>(client_),
                                         static_cast<PVIGEM_TARGET>(target_));
    if (!VIGEM_SUCCESS(status)) {
        throw std::runtime_error("vigem_target_add failed: " + StatusToString(status));
    }

    controller_added_ = true;
}

bool VigemClient::UpdateRightTrigger(uint8_t value) {
    ResetError();

    if (!controller_added_) {
        SetError("Xbox 360 controller has not been added.");
        return false;
    }

    XUSB_REPORT report;
    XUSB_REPORT_INIT(&report);
    report.bRightTrigger = value;

    const auto status = vigem_target_x360_update(static_cast<PVIGEM_CLIENT>(client_),
                                                 static_cast<PVIGEM_TARGET>(target_),
                                                 report);
    if (!VIGEM_SUCCESS(status)) {
        SetError("vigem_target_x360_update failed: " + StatusToString(status));
        return false;
    }

    return true;
}

bool VigemClient::UpdateState(const common::OutputState& state) {
    ResetError();

    if (!controller_added_) {
        SetError("Xbox 360 controller has not been added.");
        return false;
    }

    const XUSB_REPORT report = BuildReport(state);

    const auto status = vigem_target_x360_update(static_cast<PVIGEM_CLIENT>(client_),
                                                 static_cast<PVIGEM_TARGET>(target_),
                                                 report);
    if (!VIGEM_SUCCESS(status)) {
        SetError("vigem_target_x360_update failed: " + StatusToString(status));
        return false;
    }

    if (tx_state_listener_ != nullptr) {
        tx_state_listener_->OnTxState(state);
    } else if (log_state_) {
        std::cout << "tx state";
        for (const auto& [key, val] : state.channels) {
            std::cout << " " << key << "=" << val;
        }
        std::cout << std::endl;
    }

    return true;
}

void VigemClient::SetLogState(bool enabled) {
    log_state_ = enabled;
}

void VigemClient::SetTxStateListener(ITxStateListener* listener) {
    tx_state_listener_ = listener;
}

std::string VigemClient::LastError() const {
    return last_error_;
}

void VigemClient::SetError(const std::string& message) {
    last_error_ = message;
}

void VigemClient::ResetError() {
    last_error_.clear();
}

void VigemClient::Disconnect() {
    if (controller_added_ && client_ && target_) {
        vigem_target_remove(static_cast<PVIGEM_CLIENT>(client_),
                            static_cast<PVIGEM_TARGET>(target_));
        controller_added_ = false;
    }

    if (target_) {
        vigem_target_free(static_cast<PVIGEM_TARGET>(target_));
        target_ = nullptr;
    }

    if (client_) {
        if (connected_) {
            vigem_disconnect(static_cast<PVIGEM_CLIENT>(client_));
            connected_ = false;
        }
        vigem_free(static_cast<PVIGEM_CLIENT>(client_));
        client_ = nullptr;
    }
}
}  // namespace emulator
