#pragma once

#include <string>
#include <vector>

#include "mapper/MappingEngine.h"

namespace config {
struct DdsConfig {
    std::string topic;
    std::string type;
    std::string idl_file;
};

struct AppConfig {
    DdsConfig dds;
    std::vector<mapper::MappingDefinition> mappings;
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
