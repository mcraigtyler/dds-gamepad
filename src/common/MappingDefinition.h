#pragma once

#include <string>

// Shared data types used by both the config and mapper modules.
// Keeping these in a common header breaks the config → mapper include dependency.

namespace common {

// Identifies which Xbox 360 virtual-controller output a mapping writes to.
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

// Describes a single DDS-value → gamepad-output mapping, as parsed from YAML.
struct MappingDefinition {
    std::string name;
    int id = 0;
    std::string field;
    ControlTarget target = ControlTarget::RightTrigger;
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
