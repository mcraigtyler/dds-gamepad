#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "common/MappingDefinition.h"
#include "common/OutputState.h"

namespace mapper {

/// @brief Applies scale, deadzone, invert, and input-range transforms to raw
///        DDS values and writes normalised results into an `OutputState`.
///
/// @details `MappingEngine` is constructed once with the full list of
/// `MappingDefinition` entries parsed from the YAML config. On each DDS
/// message, `AppRunner` calls `Apply` with the field name, source ID, and raw
/// float value. The engine finds all matching definitions and writes the
/// transformed value(s) into the provided `OutputState::channels` map.
///
/// **Additive mappings:** When a `MappingDefinition` has `additive = true`,
/// the engine accumulates contributions from multiple DDS sources into a single
/// output channel rather than overwriting it. The last computed contribution
/// from each source is stored in `additive_state_` (keyed by mapping name) so
/// that a message from one source does not reset contributions from other
/// sources that haven't sent since the previous loop iteration.
///
/// **Input-range normalisation:** When `has_input_range` is set, raw values in
/// `[input_min, input_max]` are linearly mapped to the channel's natural range
/// before scale/deadzone/invert are applied.
class MappingEngine {
public:
    /// @brief Constructs the engine with the given mapping definitions.
    /// @param[in] mappings List of mappings parsed from the YAML config.
    ///            The vector is moved in; the caller should not use it after
    ///            this call.
    explicit MappingEngine(std::vector<common::MappingDefinition> mappings);

    /// @brief Applies all matching mappings for the given field/ID pair and
    ///        updates the output state.
    ///
    /// @details Iterates over all `MappingDefinition` entries. A definition
    /// matches when `definition.field == field` and
    /// `definition.id == message_id`. For each match the engine:
    /// 1. Applies input-range normalisation (if `has_input_range`).
    /// 2. Applies scale and invert.
    /// 3. Applies deadzone (output is zeroed inside the deadzone band).
    /// 4. Clamps to the channel's natural range.
    /// 5. Writes the result into `state.channels[definition.target]`, or
    ///    accumulates it for additive targets.
    ///
    /// @param[in]     field      DDS field name (e.g. `"x"`, `"y"`, `"value"`,
    ///                `"btnState"`).
    /// @param[in]     message_id DDS role ID from the received message.
    /// @param[in]     value      Raw float value from the DDS sample.
    /// @param[in,out] state      Output state map updated in place. Channels
    ///                not matched by any definition are left unchanged.
    /// @return `true` if at least one mapping matched and `state` was updated.
    /// @retval false If no mapping matched `field` + `message_id`; `state`
    ///         is not modified.
    bool Apply(const std::string& field, int message_id, float value, common::OutputState& state);

private:
    std::vector<common::MappingDefinition> mappings_;
    // Stores the last computed contribution (after scale/invert/deadzone) for
    // each additive mapping, keyed by mapping name. Initialised to 0 so that
    // sources which haven't yet sent a message contribute nothing to the sum.
    std::unordered_map<std::string, float> additive_state_;
};

}  // namespace mapper
