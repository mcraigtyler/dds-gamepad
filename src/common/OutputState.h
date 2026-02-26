#pragma once

#include <string>
#include <unordered_map>

namespace common {

// Generic output state passed from MappingEngine to IOutputDevice.
// Each channel key is the raw `output.to` string from the YAML config
// (e.g. "axis:left_trigger", "button:a").
// Values are normalised floats: triggers in [0, 1], axes in [-1, 1], buttons 0.0 or 1.0.
// Backend implementations (VigemClient, UdpProtobufEmulator, …) convert channels
// to their native wire format.
struct OutputState {
    std::unordered_map<std::string, float> channels;
};

}  // namespace common
