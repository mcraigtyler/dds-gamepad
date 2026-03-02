#pragma once

#include <string>

// Shared data types used by both the config and mapper modules.
// Keeping these in a common header breaks the config → mapper include dependency.

namespace common {

/// @brief Normalisation domain of an output channel.
///
/// @details `MappingEngine` uses this to apply the correct clamping range and
/// deadzone logic. `VigemClient` uses it to convert normalised floats to the
/// native Xbox 360 wire types (`int16_t`, `uint8_t`, or bitmask).
enum class ChannelType {
    Axis,    ///< Stick axis. Normalised float in [-1.0, 1.0].
    Trigger, ///< Shoulder trigger. Normalised float in [0.0, 1.0].
    Button   ///< Binary state. 1.0 = pressed, 0.0 = released.
};

/// @brief Single DDS-value → output-channel mapping, as parsed from YAML.
///
/// @details `MappingDefinition` captures everything `MappingEngine::Apply`
/// needs to transform one raw DDS float into a normalised output channel value.
/// Fields are grouped by purpose:
///
/// **Identity** — which DDS messages this mapping handles:
/// - `name`, `id`, `field`
///
/// **Output target** — where the result is written:
/// - `target`, `channelType`
///
/// **Transform** — how the raw value is processed:
/// - `scale`, `deadzone`, `invert`
///
/// **Input-range normalisation** (optional):
/// - `has_input_range`, `input_min`, `input_max`
///
/// **Additive mode** (optional):
/// - `additive`
struct MappingDefinition {
    std::string name;   ///< Human-readable mapping name (from `name:` YAML key).
    int id = 0;         ///< DDS role ID to match against `message.id`.
    std::string field;  ///< DDS field name to match (e.g. `"x"`, `"value"`, `"btnState"`).

    /// @brief Output channel key written into `OutputState::channels`.
    /// @details For `vigem_x360` backends this is the `output.to` string from
    ///          YAML (e.g. `"axis:left_x"`, `"button:a"`). For `udp_protobuf`
    ///          backends it is the protobuf field name (e.g. `"steering"`).
    std::string target;

    ChannelType channelType = ChannelType::Axis; ///< Normalisation domain of the target channel.
    float scale    = 1.0f;  ///< Multiplier applied to the normalised value after deadzone.
    float deadzone = 0.0f;  ///< Fractional deadzone half-width in [0.0, 1.0]. Zero disables.
    bool  invert   = false; ///< When `true`, the sign of the normalised value is flipped.

    /// @brief When `true`, `input_min` and `input_max` define the raw input range.
    /// @details Raw values in [`input_min`, `input_max`] are linearly mapped to
    ///          [0, 1] for triggers or [-1, 1] for sticks before the transform
    ///          pipeline (scale/deadzone/invert) is applied.
    bool  has_input_range = false;
    float input_min = 0.0f; ///< Lower bound of the raw input range. Used when `has_input_range` is `true`.
    float input_max = 1.0f; ///< Upper bound of the raw input range. Used when `has_input_range` is `true`.

    /// @brief When `true`, this mapping's contribution is summed with other
    ///        additive mappings on the same target axis.
    /// @details Allows multiple DDS sources to drive a single output axis by
    ///          accumulating their contributions. The engine tracks each source's
    ///          last contribution independently so messages from different sources
    ///          do not interfere with each other.
    bool additive = false;
};

/// @brief Identifies which DDS message type a subscription uses.
///
/// @details Resolved from the `dds.type` YAML string during
/// `config::ConfigLoader::Load` so that `AppRunner` can use a switch statement
/// rather than string comparisons when setting up topic handlers.
enum class TopicType {
    GamepadAnalog, ///< `Gamepad::Gamepad_Analog` — single `Double_t value` field.
    StickTwoAxis,  ///< `Gamepad::Stick_TwoAxis` — `Double_t x` and `Double_t y` fields.
    GamepadButton  ///< `Gamepad::Button` — optional `btnState` field (`ButtonState_t` enum).
};

}  // namespace common
