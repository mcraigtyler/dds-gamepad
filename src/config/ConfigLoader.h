#pragma once

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

struct RoleConfig {
    std::string name;
    std::vector<AppConfig> app_configs;
};

class ConfigLoader {
public:
    static RoleConfig Load(const std::string& path);
};

}  // namespace config
