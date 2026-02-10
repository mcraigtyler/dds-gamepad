#pragma once

#include <cstdint>

namespace mapper {
struct GamepadState {
    int16_t left_stick_x = 0;
    int16_t left_stick_y = 0;
    int16_t right_stick_x = 0;
    int16_t right_stick_y = 0;
    uint8_t left_trigger = 0;
    uint8_t right_trigger = 0;
    uint16_t buttons = 0;
};
}  // namespace mapper
