#include "config/ConfigLoader.h"

#include <sstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace config {
namespace {
mapper::ControlTarget ParseTarget(const std::string& value) {
    if (value == "axis:left_trigger") {
        return mapper::ControlTarget::LeftTrigger;
    }
    if (value == "axis:right_trigger") {
        return mapper::ControlTarget::RightTrigger;
    }
    if (value == "axis:left_x") {
        return mapper::ControlTarget::LeftStickX;
    }
    if (value == "axis:left_y") {
        return mapper::ControlTarget::LeftStickY;
    }
    if (value == "axis:right_x") {
        return mapper::ControlTarget::RightStickX;
    }
    if (value == "axis:right_y") {
        return mapper::ControlTarget::RightStickY;
    }

    throw std::runtime_error("Unsupported mapping target '" + value +
                             "'. Expected axis:left_trigger, axis:right_trigger, axis:left_x, axis:left_y, axis:right_x, axis:right_y.");
}

std::string RequireString(const YAML::Node& node, const std::string& key) {
    const auto field = node[key];
    if (!field || !field.IsScalar()) {
        throw std::runtime_error("Missing or invalid '" + key + "' field in config.");
    }
    return field.as<std::string>();
}

int RequireInt(const YAML::Node& node, const std::string& key) {
    const auto field = node[key];
    if (!field || !field.IsScalar()) {
        throw std::runtime_error("Missing or invalid '" + key + "' field in config.");
    }
    return field.as<int>();
}

float OptionalFloat(const YAML::Node& node, const std::string& key, float default_value) {
    const auto field = node[key];
    if (!field) {
        return default_value;
    }
    return field.as<float>();
}

bool OptionalBool(const YAML::Node& node, const std::string& key, bool default_value) {
    const auto field = node[key];
    if (!field) {
        return default_value;
    }
    return field.as<bool>();
}
}  // namespace

AppConfig ConfigLoader::Load(const std::string& path) {
    const YAML::Node root = YAML::LoadFile(path);
    if (!root || !root.IsMap()) {
        throw std::runtime_error("Config file must contain a mapping at the root.");
    }

    const YAML::Node dds_node = root["dds"];
    if (!dds_node || !dds_node.IsMap()) {
        throw std::runtime_error("Missing required 'dds' section in config.");
    }

    AppConfig config;
    config.dds.topic = RequireString(dds_node, "topic");
    if (dds_node["type"]) {
        config.dds.type = dds_node["type"].as<std::string>();
    }
    if (dds_node["idl_file"]) {
        config.dds.idl_file = dds_node["idl_file"].as<std::string>();
    }
    if (dds_node["domain_id"]) {
        config.dds.has_domain_id = true;
        config.dds.domain_id = dds_node["domain_id"].as<int>();
    }

    const YAML::Node mappings_node = root["mapping"];
    if (!mappings_node || !mappings_node.IsSequence()) {
        throw std::runtime_error("Missing required 'mapping' list in config.");
    }

    for (const auto& entry : mappings_node) {
        if (!entry.IsMap()) {
            throw std::runtime_error("Each mapping entry must be a map.");
        }

        mapper::MappingDefinition mapping;
        mapping.name = RequireString(entry, "name");
        mapping.id = RequireInt(entry, "id");
        mapping.field = RequireString(entry, "field");
        mapping.target = ParseTarget(RequireString(entry, "to"));
        mapping.scale = OptionalFloat(entry, "scale", 1.0f);
        mapping.deadzone = OptionalFloat(entry, "deadzone", 0.0f);
        mapping.invert = OptionalBool(entry, "invert", false);

        if (mapping.field != "value") {
            std::ostringstream message;
            message << "Unsupported field '" << mapping.field
                    << "' in mapping '" << mapping.name
                    << "'. Expected field: value.";
            throw std::runtime_error(message.str());
        }
        if (mapping.deadzone < 0.0f || mapping.deadzone > 1.0f) {
            std::ostringstream message;
            message << "Invalid deadzone in mapping '" << mapping.name
                    << "'. Expected range 0.0 to 1.0.";
            throw std::runtime_error(message.str());
        }

        config.mappings.push_back(mapping);
    }

    if (config.mappings.empty()) {
        throw std::runtime_error("Config must include at least one mapping entry.");
    }

    return config;
}
}  // namespace config
