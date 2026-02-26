#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "common/MappingDefinition.h"
#include "common/OutputState.h"

namespace mapper {

class MappingEngine {
public:
    explicit MappingEngine(std::vector<common::MappingDefinition> mappings);

    bool Apply(const std::string& field, int message_id, float value, common::OutputState& state);

private:
    std::vector<common::MappingDefinition> mappings_;
    // Stores the last computed contribution (after scale/invert/deadzone) for
    // each additive mapping, keyed by mapping name. Initialised to 0 so that
    // sources which haven't yet sent a message contribute nothing to the sum.
    std::unordered_map<std::string, float> additive_state_;
};

}  // namespace mapper
