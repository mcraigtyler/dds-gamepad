#include "emulator/VigemClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <ViGEm/Client.h>

#include <iostream>
#include <sstream>

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

bool VigemClient::Connect() {
    ResetError();

    if (!client_) {
        client_ = vigem_alloc();
        if (!client_) {
            SetError("Failed to allocate ViGEm client.");
            return false;
        }
    }

    if (connected_) {
        return true;
    }

    const auto status = vigem_connect(static_cast<PVIGEM_CLIENT>(client_));
    if (!VIGEM_SUCCESS(status)) {
        SetError("vigem_connect failed: " + StatusToString(status));
        return false;
    }

    connected_ = true;
    return true;
}

bool VigemClient::AddX360Controller() {
    ResetError();

    if (!connected_) {
        SetError("ViGEm client is not connected.");
        return false;
    }

    if (!target_) {
        target_ = vigem_target_x360_alloc();
        if (!target_) {
            SetError("Failed to allocate Xbox 360 target.");
            return false;
        }
    }

    if (controller_added_) {
        return true;
    }

    const auto status = vigem_target_add(static_cast<PVIGEM_CLIENT>(client_),
                                         static_cast<PVIGEM_TARGET>(target_));
    if (!VIGEM_SUCCESS(status)) {
        SetError("vigem_target_add failed: " + StatusToString(status));
        return false;
    }

    controller_added_ = true;
    return true;
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

bool VigemClient::UpdateState(const mapper::GamepadState& state) {
    ResetError();

    if (!controller_added_) {
        SetError("Xbox 360 controller has not been added.");
        return false;
    }

    XUSB_REPORT report;
    XUSB_REPORT_INIT(&report);
    report.sThumbLX = state.left_stick_x;
    report.sThumbLY = state.left_stick_y;
    report.sThumbRX = state.right_stick_x;
    report.sThumbRY = state.right_stick_y;
    report.bLeftTrigger = state.left_trigger;
    report.bRightTrigger = state.right_trigger;
    report.wButtons = state.buttons;

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
        std::cout << "tx state"
                  << " LT=" << static_cast<int>(state.left_trigger)
                  << " RT=" << static_cast<int>(state.right_trigger)
                  << " LX=" << state.left_stick_x
                  << " LY=" << state.left_stick_y
                  << " RX=" << state.right_stick_x
                  << " RY=" << state.right_stick_y
                  << " Btn=0x" << std::hex << state.buttons << std::dec
                  << std::endl;
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
