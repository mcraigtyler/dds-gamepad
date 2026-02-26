#pragma once

#include <string>

// Shared data types used by both the config and mapper modules.
// Keeping these in a common header breaks the config → mapper include dependency.

namespace common {

// Describes how a float value should be normalised for its output channel.
// Used by MappingEngine to apply deadzone and clamping in the correct range,
// and by backends to convert the normalised float to a native wire type.
enum class ChannelType {
    Axis,     // normalised float in [-1, 1]; sticks
    Trigger,  // normalised float in [0, 1]; triggers
    Button    // binary 0.0 or 1.0
};

// Describes a single DDS-value → output-channel mapping, as parsed from YAML.
struct MappingDefinition {
    std::string name;
    int id = 0;
    std::string field;
    // Raw `output.to` string from the YAML config; used as the OutputState channel key
    // and (for UDP backends) as the protobuf field name.
    std::string target;
    ChannelType channelType = ChannelType::Axis;
    float scale = 1.0f;
    float deadzone = 0.0f;
    bool invert = false;
    // Optional input range: when set, raw input values in [input_min, input_max]
    // are normalized to 0..1 for triggers or -1..1 for sticks before applying
    // scale/deadzone/invert.
    bool has_input_range = false;
    float input_min = 0.0f;
    float input_max = 1.0f;
    // When true, this mapping's contribution is summed with other additive
    // mappings targeting the same axis rather than replacing the axis value.
    // The engine tracks each source's last contribution so batches from
    // different sources don't interfere.
    bool additive = false;
};

// Identifies which DDS message type a subscription uses.
// Parsed from the YAML dds.type string during config loading so that
// AppRunner never needs to do string comparisons at setup time.
enum class TopicType {
    GamepadAnalog,
    StickTwoAxis,
    GamepadButton
};

}  // namespace common
