#pragma once

#include <string>
#include <vector>

#include "mapper/MappingEngine.h"

namespace config {
struct DdsConfig {
    std::string topic;
    std::string type;
    std::string idl_file;
    bool has_domain_id = false;
    int domain_id = 0;
};

struct AppConfig {
    DdsConfig dds;
    std::vector<mapper::MappingDefinition> mappings;
};

class ConfigLoader {
public:
    static AppConfig Load(const std::string& path);
    static std::vector<AppConfig> LoadDirectory(const std::string& path);
};
}  // namespace config
