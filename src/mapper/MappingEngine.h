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
    RightStickY
};

struct MappingDefinition {
    std::string name;
    int id = 0;
    std::string field;
    ControlTarget target = ControlTarget::RightTrigger;
    float scale = 1.0f;
    float deadzone = 0.0f;
    bool invert = false;
};

class MappingEngine {
public:
    explicit MappingEngine(std::vector<MappingDefinition> mappings);

    bool Apply(int message_id, float value, GamepadState& state) const;

private:
    std::vector<MappingDefinition> mappings_;
};
}  // namespace mapper
