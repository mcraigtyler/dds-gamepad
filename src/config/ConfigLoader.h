#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/MappingDefinition.h"

namespace config {

struct DdsConfig {
    std::string topic;
    std::string type;
    std::string idl_file;
};

struct AppConfig {
    DdsConfig dds;
    // TopicType is resolved from dds.type during Load() so AppRunner never
    // needs to do string comparisons at setup time.
    common::TopicType topicType = common::TopicType::GamepadAnalog;
    std::vector<common::MappingDefinition> mappings;
};

// Parsed from the top-level `output:` YAML section.
// Selects the output backend and provides its connection parameters.
// If `output:` is absent from the YAML, type defaults to "vigem_x360".
struct OutputConfig {
    std::string type = "vigem_x360";  // "vigem_x360" or "udp_protobuf"
    std::string host;                 // UDP target host (udp_protobuf only)
    uint16_t port = 0;                // UDP target port (udp_protobuf only)
};

struct RoleConfig {
    std::string name;
    OutputConfig output;
    std::vector<AppConfig> app_configs;
};

class ConfigLoader {
public:
    static RoleConfig Load(const std::string& path);
};

}  // namespace config
