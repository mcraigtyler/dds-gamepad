#include "config/ConfigLoader.h"

#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace config {
namespace {
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

bool IsButtonTarget(mapper::ControlTarget target) {
    return target == mapper::ControlTarget::ButtonA ||
           target == mapper::ControlTarget::ButtonX ||
           target == mapper::ControlTarget::DpadUp ||
           target == mapper::ControlTarget::DpadDown ||
           target == mapper::ControlTarget::DpadLeft ||
           target == mapper::ControlTarget::DpadRight;
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
    if (value == "button:a") {
        return mapper::ControlTarget::ButtonA;
    }
    if (value == "button:x") {
        return mapper::ControlTarget::ButtonX;
    }
    if (value == "dpad:up") {
        return mapper::ControlTarget::DpadUp;
    }
    if (value == "dpad:down") {
        return mapper::ControlTarget::DpadDown;
    }
    if (value == "dpad:left") {
        return mapper::ControlTarget::DpadLeft;
    }
    if (value == "dpad:right") {
        return mapper::ControlTarget::DpadRight;
    }

    throw std::runtime_error("Unsupported mapping target '" + value +
                             "'. Expected axis:left_trigger, axis:right_trigger, axis:left_x, axis:left_y, axis:right_x, axis:right_y, button:a, button:x, dpad:up, dpad:down, dpad:left, dpad:right.");
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

void ValidateMappingType(const std::string& ddsType,
                         const std::string& rawField,
                         const mapper::MappingDefinition& mapping) {
    if (ddsType == "Gamepad::Gamepad_Analog" || ddsType == "Gamepad_Analog") {
        if (mapping.field != "value") {
            throw std::runtime_error("Unsupported field '" + rawField + "' for Gamepad_Analog mapping '" + mapping.name + "'. Expected field: value.");
        }
        return;
    }

    if (ddsType == "Gamepad::Stick_TwoAxis" || ddsType == "Stick_TwoAxis") {
        if (mapping.field != "x" && mapping.field != "y") {
            throw std::runtime_error("Unsupported field '" + rawField + "' for Stick_TwoAxis mapping '" + mapping.name + "'. Expected field: x or y.");
        }
        if (!IsStickTarget(mapping.target)) {
            throw std::runtime_error("Unsupported target for Stick_TwoAxis mapping '" + mapping.name + "'. Expected a stick axis target.");
        }
        return;
    }

    if (ddsType == "Gamepad::Button" || ddsType == "Button") {
        if (mapping.field != "btnState") {
            throw std::runtime_error("Unsupported field '" + rawField + "' for Button mapping '" + mapping.name + "'. Expected field: btnState.");
        }
        if (!IsButtonTarget(mapping.target)) {
            throw std::runtime_error("Unsupported target for Button mapping '" + mapping.name + "'. Expected a button or dpad target.");
        }
        return;
    }

    throw std::runtime_error("Unsupported DDS type '" + ddsType + "'. Expected Gamepad::Gamepad_Analog, Gamepad::Stick_TwoAxis, or Gamepad::Button.");
}

std::string BuildTopicKey(const DdsConfig& dds) {
    return dds.topic + "|" + dds.type + "|" + dds.idl_file;
}
}  // namespace

RoleConfig ConfigLoader::Load(const std::string& path) {
    const YAML::Node root = YAML::LoadFile(path);
    if (!root || !root.IsMap()) {
        throw std::runtime_error("Config file must contain a mapping at the root.");
    }

    const YAML::Node roleNode = root["role"];
    if (!roleNode || !roleNode.IsMap()) {
        throw std::runtime_error("Missing required 'role' section in config.");
    }

    const YAML::Node mappingsNode = root["mappings"];
    if (!mappingsNode || !mappingsNode.IsSequence()) {
        throw std::runtime_error("Missing required 'mappings' list in config.");
    }

    RoleConfig roleConfig;
    roleConfig.name = RequireString(roleNode, "name");
    roleConfig.yoke_id = RequireInt(roleNode, "yoke_id");

    std::unordered_map<std::string, size_t> topicIndex;

    for (const auto& entry : mappingsNode) {
        if (!entry.IsMap()) {
            throw std::runtime_error("Each mapping entry must be a map.");
        }

        const YAML::Node ddsNode = entry["dds"];
        if (!ddsNode || !ddsNode.IsMap()) {
            throw std::runtime_error("Each mapping entry must contain a 'dds' map.");
        }

        const YAML::Node gamepadNode = entry["gamepad"];
        if (!gamepadNode || !gamepadNode.IsMap()) {
            throw std::runtime_error("Each mapping entry must contain a 'gamepad' map.");
        }

        DdsConfig dds;
        dds.topic = RequireString(ddsNode, "topic");
        dds.type = RequireString(ddsNode, "type");
        dds.idl_file = RequireString(ddsNode, "idl_file");

        mapper::MappingDefinition mapping;
        mapping.name = RequireString(entry, "name");
        mapping.id = RequireInt(ddsNode, "id");

        const std::string rawField = RequireString(ddsNode, "field");
        mapping.field = NormalizeFieldName(rawField);
        mapping.target = ParseTarget(RequireString(gamepadNode, "to"));
        mapping.scale = OptionalFloat(gamepadNode, "scale", 1.0f);
        mapping.deadzone = OptionalFloat(gamepadNode, "deadzone", 0.0f);
        mapping.invert = OptionalBool(gamepadNode, "invert", false);

        if (ddsNode["input_min"] && ddsNode["input_max"]) {
            mapping.input_min = ddsNode["input_min"].as<float>();
            mapping.input_max = ddsNode["input_max"].as<float>();
            mapping.has_input_range = true;
        }

        ValidateMappingType(dds.type, rawField, mapping);

        if ((IsTriggerTarget(mapping.target) || IsStickTarget(mapping.target)) &&
            (mapping.deadzone < 0.0f || mapping.deadzone > 1.0f)) {
            std::ostringstream message;
            message << "Invalid deadzone in mapping '" << mapping.name
                    << "'. Expected range 0.0 to 1.0.";
            throw std::runtime_error(message.str());
        }

        const std::string key = BuildTopicKey(dds);
        auto found = topicIndex.find(key);
        if (found == topicIndex.end()) {
            AppConfig appConfig;
            appConfig.dds = dds;
            appConfig.mappings.push_back(mapping);
            roleConfig.app_configs.push_back(std::move(appConfig));
            topicIndex.emplace(key, roleConfig.app_configs.size() - 1);
            continue;
        }

        roleConfig.app_configs[found->second].mappings.push_back(mapping);
    }

    if (roleConfig.app_configs.empty()) {
        throw std::runtime_error("Config must include at least one mapping entry.");
    }

    return roleConfig;
}
}  // namespace config
