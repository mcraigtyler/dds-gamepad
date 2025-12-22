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
    : mappings_(std::move(mappings)) {}

bool MappingEngine::Apply(int message_id, float value, GamepadState& state) const {
    bool updated = false;
    for (const auto& mapping : mappings_) {
        // Mapping definitions are provided per-topic (one mapping per config file).
        // The publisher uses a global sequence `messageID`, so matching on
        // `message_id` prevents mappings from applying. Apply mappings for
        // every received sample for this topic instead of filtering by id.

        // Start with the raw value. If an input range is provided, normalize
        // the raw value into 0..1 (triggers) or -1..1 (sticks). Then apply
        // the config `scale` multiplier.
        float mapped_value = value;
        if (mapping.has_input_range) {
            const float in_min = mapping.input_min;
            const float in_max = mapping.input_max;
            if (in_max != in_min) {
                const float t = (value - in_min) / (in_max - in_min);
                // For triggers (0..1)
                if (mapping.target == ControlTarget::LeftTrigger ||
                    mapping.target == ControlTarget::RightTrigger) {
                    mapped_value = t;
                } else {
                    // For sticks: map 0..1 -> -1..1
                    mapped_value = t * 2.0f - 1.0f;
                }
            } else {
                mapped_value = 0.0f;
            }
        }
        mapped_value = mapped_value * mapping.scale;
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
                state.left_trigger = TriggerFromNormalized(mapped_value);
                break;
            case ControlTarget::RightTrigger:
                state.right_trigger = TriggerFromNormalized(mapped_value);
                break;
            case ControlTarget::LeftStickX:
                state.left_stick_x = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::LeftStickY:
                state.left_stick_y = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::RightStickX:
                state.right_stick_x = AxisFromNormalized(mapped_value);
                break;
            case ControlTarget::RightStickY:
                state.right_stick_y = AxisFromNormalized(mapped_value);
                break;
        }
        updated = true;
    }

    return updated;
}
}  // namespace mapper
