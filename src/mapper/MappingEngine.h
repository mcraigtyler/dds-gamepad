#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mapper/GamepadState.h"

namespace mapper {
enum class ControlTarget {
    LeftTrigger,
    RightTrigger,
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    ButtonA,
    ButtonB,
    ButtonX,
    ButtonY,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight
};

struct MappingDefinition {
    std::string name;
    int id = 0;
    std::string field;
    ControlTarget target = ControlTarget::RightTrigger;
    float scale = 1.0f;
    float deadzone = 0.0f;
    bool invert = false;
    // Optional input range: when set, raw input values in [input_min,input_max]
    // are normalized to 0..1 for triggers or -1..1 for sticks before applying
    // scale/deadzone/invert.
    bool has_input_range = false;
    float input_min = 0.0f;
    float input_max = 1.0f;
};

class MappingEngine {
public:
    explicit MappingEngine(std::vector<MappingDefinition> mappings);

    bool Apply(const std::string& field, int message_id, float value, GamepadState& state) const;

private:
    std::vector<MappingDefinition> mappings_;
};
}  // namespace mapper
