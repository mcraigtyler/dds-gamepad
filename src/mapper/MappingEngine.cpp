#include "mapper/MappingEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mapper {
namespace {
float ApplyDeadzone(float value, float deadzone) {
    if (deadzone <= 0.0f) {
        return value;
    }
    if (std::fabs(value) <= deadzone) {
        return 0.0f;
    }
    return value;
}

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
}  // namespace

MappingEngine::MappingEngine(std::vector<MappingDefinition> mappings)
    : mappings_(std::move(mappings)), state_() {}

bool MappingEngine::Apply(int message_id, float value) {
    bool updated = false;
    for (const auto& mapping : mappings_) {
        if (mapping.id != message_id) {
            continue;
        }

        float mapped_value = value * mapping.scale;
        switch (mapping.target) {
            case ControlTarget::LeftTrigger:
            case ControlTarget::RightTrigger:
                if (mapping.invert) {
                    mapped_value = 1.0f - mapped_value;
                }
                mapped_value = ApplyDeadzone(mapped_value, mapping.deadzone);
                mapped_value = std::clamp(mapped_value, 0.0f, 1.0f);
                break;
            case ControlTarget::LeftStickX:
            case ControlTarget::LeftStickY:
            case ControlTarget::RightStickX:
            case ControlTarget::RightStickY:
                if (mapping.invert) {
                    mapped_value = -mapped_value;
                }
                mapped_value = ApplyDeadzone(mapped_value, mapping.deadzone);
                mapped_value = std::clamp(mapped_value, -1.0f, 1.0f);
                break;
        }

        switch (mapping.target) {
            case ControlTarget::LeftTrigger:
                state_.left_trigger = TriggerFromNormalized(mapped_value);
                break;
            case ControlTarget::RightTrigger:
                state_.right_trigger = TriggerFromNormalized(mapped_value);
                break;
            case ControlTarget::LeftStickX:
                state_.left_stick_x = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::LeftStickY:
                state_.left_stick_y = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::RightStickX:
                state_.right_stick_x = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::RightStickY:
                state_.right_stick_y = AxisFromNormalized(mapped_value);
                break;
        }
        updated = true;
    }

    return updated;
}

const GamepadState& MappingEngine::State() const {
    return state_;
}
}  // namespace mapper
