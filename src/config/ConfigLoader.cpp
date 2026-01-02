#include "config/ConfigLoader.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace config {
namespace {
enum class MessageType {
    Unknown,
    ValueMsg,
    GamepadAnalog,
    StickTwoAxis
};

MessageType ParseMessageType(const std::string& value) {
    if (value.empty()) {
        return MessageType::Unknown;
    }
    if (value == "Value::Msg" || value == "Value.Msg") {
        return MessageType::ValueMsg;
    }
    if (value == "Gamepad::Gamepad_Analog" || value == "Gamepad_Analog") {
        return MessageType::GamepadAnalog;
    }
    if (value == "Gamepad::Stick_TwoAxis" || value == "Stick_TwoAxis") {
        return MessageType::StickTwoAxis;
    }

    throw std::runtime_error("Unsupported DDS type '" + value +
                             "'. Expected Gamepad::Gamepad_Analog or Gamepad::Stick_TwoAxis.");
}

std::string NormalizeFieldName(const std::string& value) {
    const auto pos = value.rfind('.');
    if (pos == std::string::npos) {
        return value;
    }
    return value.substr(pos + 1);
}

bool IsTriggerTarget(mapper::ControlTarget target) {
    return target == mapper::ControlTarget::LeftTrigger ||
           target == mapper::ControlTarget::RightTrigger;
}

bool IsStickTarget(mapper::ControlTarget target) {
    return target == mapper::ControlTarget::LeftStickX ||
           target == mapper::ControlTarget::LeftStickY ||
           target == mapper::ControlTarget::RightStickX ||
           target == mapper::ControlTarget::RightStickY;
}

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

    const YAML::Node mappings_node = root["mapping"];
    if (!mappings_node || !mappings_node.IsSequence()) {
        throw std::runtime_error("Missing required 'mapping' list in config.");
    }

    const MessageType message_type = ParseMessageType(config.dds.type);

    for (const auto& entry : mappings_node) {
        if (!entry.IsMap()) {
            throw std::runtime_error("Each mapping entry must be a map.");
        }

        mapper::MappingDefinition mapping;
        mapping.name = RequireString(entry, "name");
        mapping.id = RequireInt(entry, "id");
        const std::string raw_field = RequireString(entry, "field");
        mapping.field = NormalizeFieldName(raw_field);
        mapping.target = ParseTarget(RequireString(entry, "to"));
        mapping.scale = OptionalFloat(entry, "scale", 1.0f);
        mapping.deadzone = OptionalFloat(entry, "deadzone", 0.0f);
        mapping.invert = OptionalBool(entry, "invert", false);

        // Optional input range normalization fields. If both are present,
        // treat input values in [input_min,input_max] as the source range
        // to normalize from.
        if (entry["input_min"] && entry["input_max"]) {
            mapping.input_min = entry["input_min"].as<float>();
            mapping.input_max = entry["input_max"].as<float>();
            mapping.has_input_range = true;
        }

        if (message_type == MessageType::GamepadAnalog) {
            if (mapping.field != "value") {
                std::ostringstream message;
                message << "Unsupported field '" << raw_field
                        << "' in mapping '" << mapping.name
                        << "'. Expected field: value.";
                throw std::runtime_error(message.str());
            }
            if (!IsTriggerTarget(mapping.target)) {
                std::ostringstream message;
                message << "Unsupported target for Gamepad_Analog mapping '"
                        << mapping.name << "'. Expected a trigger axis target.";
                throw std::runtime_error(message.str());
            }
        } else if (message_type == MessageType::StickTwoAxis) {
            if (mapping.field != "x" && mapping.field != "y") {
                std::ostringstream message;
                message << "Unsupported field '" << raw_field
                        << "' in mapping '" << mapping.name
                        << "'. Expected field: x or y.";
                throw std::runtime_error(message.str());
            }
            if (!IsStickTarget(mapping.target)) {
                std::ostringstream message;
                message << "Unsupported target for Stick_TwoAxis mapping '"
                        << mapping.name << "'. Expected a stick axis target.";
                throw std::runtime_error(message.str());
            }
        } else if (message_type == MessageType::ValueMsg) {
            if (mapping.field != "value") {
                std::ostringstream message;
                message << "Unsupported field '" << raw_field
                        << "' in mapping '" << mapping.name
                        << "'. Expected field: value.";
                throw std::runtime_error(message.str());
            }
        } else if (mapping.field != "value" && mapping.field != "x" &&
                   mapping.field != "y") {
            std::ostringstream message;
            message << "Unsupported field '" << raw_field
                    << "' in mapping '" << mapping.name
                    << "'. Expected field: value, x, or y.";
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

std::vector<AppConfig> ConfigLoader::LoadDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    std::vector<AppConfig> configs;

    const fs::path root(path);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        throw std::runtime_error("Config path must be a directory: " + path);
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (ext == ".yaml" || ext == ".yml") {
            files.push_back(entry.path());
        }
    }

    if (files.empty()) {
        throw std::runtime_error("Config directory contains no .yaml or .yml files: " + path);
    }

    std::sort(files.begin(), files.end());
    configs.reserve(files.size());
    for (const auto& file : files) {
        configs.push_back(Load(file.string()));
    }

    return configs;
}
}  // namespace config
