#include "mapper/MappingEngine.h"

#include <algorithm>
#include <cmath>


namespace mapper {

using common::ChannelType;
using common::MappingDefinition;

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

bool MappingEngine::Apply(const std::string& field, int message_id, float value, common::OutputState& state) {
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
                if (mapping.channelType == ChannelType::Trigger) {
                    mapped_value = t;  // 0..1
                } else {
                    mapped_value = t * 2.0f - 1.0f;  // -1..1
                }
            } else {
                mapped_value = 0.0f;
            }
        }
        mapped_value = mapped_value * mapping.scale;

        switch (mapping.channelType) {
            case ChannelType::Trigger:
                if (mapping.invert) {
                    mapped_value = 1.0f - mapped_value;
                }
                mapped_value = ApplyDeadzone(mapped_value, mapping.deadzone);
                mapped_value = std::clamp(mapped_value, 0.0f, 1.0f);
                break;
            case ChannelType::Axis:
                if (mapping.invert) {
                    mapped_value = -mapped_value;
                }
                mapped_value = ApplyDeadzone(mapped_value, mapping.deadzone);
                mapped_value = std::clamp(mapped_value, -1.0f, 1.0f);
                break;
            case ChannelType::Button:
                mapped_value = (mapped_value > 0.5f) ? 1.0f : 0.0f;
                break;
        }

        // For additive targets: store this source's contribution and write the
        // sum of all additive contributions for this channel.
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
            if (mapping.channelType == ChannelType::Trigger) {
                sum = std::clamp(sum, 0.0f, 1.0f);
            } else {
                sum = std::clamp(sum, -1.0f, 1.0f);
            }
            state.channels[mapping.target] = sum;
            updated = true;
            continue;
        }

        state.channels[mapping.target] = mapped_value;
        updated = true;
    }

    return updated;
}
}  // namespace mapper
