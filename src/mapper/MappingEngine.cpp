#include "mapper/MappingEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>


namespace mapper {
namespace {
constexpr uint16_t kDpadUpMask = 0x0001;
constexpr uint16_t kDpadDownMask = 0x0002;
constexpr uint16_t kDpadLeftMask = 0x0004;
constexpr uint16_t kDpadRightMask = 0x0008;
constexpr uint16_t kButtonAMask = 0x1000;
constexpr uint16_t kButtonBMask = 0x2000;
constexpr uint16_t kButtonXMask = 0x4000;
constexpr uint16_t kButtonYMask = 0x8000;

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
    : mappings_(std::move(mappings)) {
    // Seed every additive mapping at 0 so sources that haven't sent yet
    // contribute nothing to the sum.
    for (const auto& m : mappings_) {
        if (m.additive) {
            additive_state_[m.name] = 0.0f;
        }
    }
}

bool MappingEngine::Apply(const std::string& field, int message_id, float value, GamepadState& state) const {
    bool updated = false;
    for (const auto& mapping : mappings_) {
        if (mapping.field != field) {
            continue;
        }
        if (mapping.id != message_id) {
            continue;
        }

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
            case ControlTarget::ButtonA:
            case ControlTarget::ButtonB:
            case ControlTarget::ButtonX:
            case ControlTarget::ButtonY:
            case ControlTarget::DpadUp:
            case ControlTarget::DpadDown:
            case ControlTarget::DpadLeft:
            case ControlTarget::DpadRight:
                mapped_value = (mapped_value > 0.5f) ? 1.0f : 0.0f;
                break;
        }

        // For additive stick/trigger targets: store this source's contribution
        // and write the sum of all additive contributions for this axis.
        // This correctly handles sources that arrive in separate read batches —
        // each source's last known value is retained until it sends a new one.
        if (mapping.additive) {
            additive_state_[mapping.name] = mapped_value;
            float sum = 0.0f;
            for (const auto& m : mappings_) {
                if (m.additive && m.target == mapping.target) {
                    sum += additive_state_.at(m.name);
                }
            }
            switch (mapping.target) {
                case ControlTarget::LeftTrigger:
                    state.left_trigger = TriggerFromNormalized(std::clamp(sum, 0.0f, 1.0f));
                    break;
                case ControlTarget::RightTrigger:
                    state.right_trigger = TriggerFromNormalized(std::clamp(sum, 0.0f, 1.0f));
                    break;
                case ControlTarget::LeftStickX:
                    state.left_stick_x = AxisFromNormalized(std::clamp(sum, -1.0f, 1.0f));
                    break;
                case ControlTarget::LeftStickY:
                    state.left_stick_y = AxisFromNormalized(std::clamp(sum, -1.0f, 1.0f));
                    break;
                case ControlTarget::RightStickX:
                    state.right_stick_x = AxisFromNormalized(std::clamp(sum, -1.0f, 1.0f));
                    break;
                case ControlTarget::RightStickY:
                    state.right_stick_y = AxisFromNormalized(std::clamp(sum, -1.0f, 1.0f));
                    break;
                default:
                    break;
            }
            updated = true;
            continue;
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
            case ControlTarget::ButtonA:
                if (mapped_value > 0.5f) {
                    state.buttons |= kButtonAMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kButtonAMask);
                }
                break;
            case ControlTarget::ButtonB:
                if (mapped_value > 0.5f) {
                    state.buttons |= kButtonBMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kButtonBMask);
                }
                break;
            case ControlTarget::ButtonX:
                if (mapped_value > 0.5f) {
                    state.buttons |= kButtonXMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kButtonXMask);
                }
                break;
            case ControlTarget::ButtonY:
                if (mapped_value > 0.5f) {
                    state.buttons |= kButtonYMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kButtonYMask);
                }
                break;
            case ControlTarget::DpadUp:
                if (mapped_value > 0.5f) {
                    state.buttons |= kDpadUpMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kDpadUpMask);
                }
                break;
            case ControlTarget::DpadDown:
                if (mapped_value > 0.5f) {
                    state.buttons |= kDpadDownMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kDpadDownMask);
                }
                break;
            case ControlTarget::DpadLeft:
                if (mapped_value > 0.5f) {
                    state.buttons |= kDpadLeftMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kDpadLeftMask);
                }
                break;
            case ControlTarget::DpadRight:
                if (mapped_value > 0.5f) {
                    state.buttons |= kDpadRightMask;
                } else {
                    state.buttons &= static_cast<uint16_t>(~kDpadRightMask);
                }
                break;
        }
        updated = true;
    }

    return updated;
}
}  // namespace mapper
