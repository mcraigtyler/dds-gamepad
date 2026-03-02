#pragma once

#include <string>
#include <unordered_map>

namespace common {

/// @brief Generic output state passed from `MappingEngine` to `IOutputDevice`.
///
/// @details `OutputState` is a flat key–value map that decouples the mapping
/// layer from the output backend. After `MappingEngine::Apply` processes each
/// DDS sample it writes results into this map; the backend (`VigemClient`,
/// `UdpProtobufEmulator`, …) then converts the map to its native wire format.
///
/// **Channel key conventions for the `vigem_x360` backend:**
///
/// | Key prefix          | Value range | Examples                          |
/// |---------------------|-------------|-----------------------------------|
/// | `"axis:left_x"`     | [-1.0, 1.0] | Left stick X                      |
/// | `"axis:left_y"`     | [-1.0, 1.0] | Left stick Y                      |
/// | `"axis:right_x"`    | [-1.0, 1.0] | Right stick X                     |
/// | `"axis:right_y"`    | [-1.0, 1.0] | Right stick Y                     |
/// | `"axis:left_trigger"`  | [0.0, 1.0] | Left shoulder trigger             |
/// | `"axis:right_trigger"` | [0.0, 1.0] | Right shoulder trigger            |
/// | `"button:a"` …      | 0.0 or 1.0  | Face buttons A / B / X / Y       |
/// | `"dpad:up"` …       | 0.0 or 1.0  | D-Pad directions                  |
///
/// For the `udp_protobuf` backend the key is the protobuf field name
/// (e.g. `"steering"`, `"throttle"`).
struct OutputState {
    /// Normalised channel values keyed by `output.to` target string from YAML.
    std::unordered_map<std::string, float> channels;
};

}  // namespace common
