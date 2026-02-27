#pragma once

#include <cstdint>

namespace mapper {

/// @brief Raw Xbox 360 controller state using native wire types.
///
/// @details `GamepadState` stores gamepad values in the same integer types
/// expected by the ViGEmBus `XUSB_REPORT` structure, as opposed to the
/// normalised floats used by `common::OutputState`.
///
/// **This struct is not used by the production pipeline.** The main data
/// path goes through `common::OutputState`, and `VigemClient::UpdateState`
/// performs the normalised-float → wire-type conversion internally.
///
/// `GamepadState` is retained for the `vigem_sanity` executable, which
/// tests the ViGEmBus driver directly without going through the full DDS
/// pipeline.
///
/// | Field           | Type       | Range            |
/// |-----------------|------------|------------------|
/// | Stick axes      | `int16_t`  | [-32768, 32767]  |
/// | Triggers        | `uint8_t`  | [0, 255]         |
/// | Button bitmask  | `uint16_t` | bit flags        |
struct GamepadState {
    int16_t  left_stick_x  = 0; ///< Left stick horizontal axis. Negative = left.
    int16_t  left_stick_y  = 0; ///< Left stick vertical axis. Negative = down.
    int16_t  right_stick_x = 0; ///< Right stick horizontal axis. Negative = left.
    int16_t  right_stick_y = 0; ///< Right stick vertical axis. Negative = down.
    uint8_t  left_trigger  = 0; ///< Left shoulder trigger. 0 = released, 255 = fully pressed.
    uint8_t  right_trigger = 0; ///< Right shoulder trigger. 0 = released, 255 = fully pressed.
    uint16_t buttons       = 0; ///< Button bitmask. See ViGEmBus `XUSB_BUTTON` enum for bit positions.
};

}  // namespace mapper
