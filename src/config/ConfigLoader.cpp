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

common::TopicType ParseTopicType(const std::string& type) {
    if (type == "Gamepad::Gamepad_Analog" || type == "Gamepad_Analog") {
        return common::TopicType::GamepadAnalog;
    }
    if (type == "Gamepad::Stick_TwoAxis" || type == "Stick_TwoAxis") {
        return common::TopicType::StickTwoAxis;
    }
    if (type == "Gamepad::Button" || type == "Button") {
        return common::TopicType::GamepadButton;
    }
    throw std::runtime_error("Unsupported DDS type '" + type +
                             "'. Expected Gamepad::Gamepad_Analog, Gamepad::Stick_TwoAxis, or Gamepad::Button.");
}

// Infers the ChannelType from the `output.to` target string.
// The trigger channels use the "axis:" prefix but a [0, 1] range.
// Unknown prefixes default to Axis for forward compatibility with custom backends.
common::ChannelType InferChannelType(const std::string& target) {
    if (target == "axis:left_trigger" || target == "axis:right_trigger") {
        return common::ChannelType::Trigger;
    }
    if (target.rfind("axis:", 0) == 0) {
        return common::ChannelType::Axis;
    }
    if (target.rfind("button:", 0) == 0 || target.rfind("dpad:", 0) == 0) {
        return common::ChannelType::Button;
    }
    return common::ChannelType::Axis;
}

// Parses an explicit `type:` value from the per-mapping output/gamepad node.
// Used for custom channel names (e.g. UDP protobuf fields) where the type
// cannot be inferred from the target string prefix.
common::ChannelType ParseChannelType(const std::string& type) {
    if (type == "axis")    return common::ChannelType::Axis;
    if (type == "trigger") return common::ChannelType::Trigger;
    if (type == "button")  return common::ChannelType::Button;
    throw std::runtime_error("Unknown channel type '" + type + "'. Expected: axis, trigger, button.");
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

std::string OptionalString(const YAML::Node& node, const std::string& key,
                           const std::string& default_value) {
    const auto field = node[key];
    if (!field) {
        return default_value;
    }
    return field.as<std::string>();
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
                         const common::MappingDefinition& mapping) {
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
        if (mapping.channelType != common::ChannelType::Axis) {
            throw std::runtime_error("Unsupported target for Stick_TwoAxis mapping '" + mapping.name + "'. Expected a stick axis target.");
        }
        return;
    }

    if (ddsType == "Gamepad::Button" || ddsType == "Button") {
        if (mapping.field != "btnState") {
            throw std::runtime_error("Unsupported field '" + rawField + "' for Button mapping '" + mapping.name + "'. Expected field: btnState.");
        }
        if (mapping.channelType != common::ChannelType::Button) {
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

    // Parse optional top-level output: section.
    // Absent output: defaults to vigem_x360 for backward compatibility.
    const YAML::Node outputNode = root["output"];
    if (outputNode && outputNode.IsMap()) {
        roleConfig.output.type = OptionalString(outputNode, "type", "vigem_x360");
        roleConfig.output.host = OptionalString(outputNode, "host", "");
        if (outputNode["port"] && outputNode["port"].IsScalar()) {
            roleConfig.output.port = static_cast<uint16_t>(outputNode["port"].as<int>());
        }
    }

    std::unordered_map<std::string, size_t> topicIndex;

    for (const auto& entry : mappingsNode) {
        if (!entry.IsMap()) {
            throw std::runtime_error("Each mapping entry must be a map.");
        }

        const YAML::Node ddsNode = entry["dds"];
        if (!ddsNode || !ddsNode.IsMap()) {
            throw std::runtime_error("Each mapping entry must contain a 'dds' map.");
        }

        // Accept `output:` (new schema) or `gamepad:` (legacy schema) per mapping.
        YAML::Node mappingOutputNode = entry["output"];
        if (!mappingOutputNode || !mappingOutputNode.IsMap()) {
            mappingOutputNode = entry["gamepad"];
        }
        if (!mappingOutputNode || !mappingOutputNode.IsMap()) {
            throw std::runtime_error("Each mapping entry must contain an 'output' (or 'gamepad') map.");
        }

        DdsConfig dds;
        dds.topic = RequireString(ddsNode, "topic");
        dds.type = RequireString(ddsNode, "type");
        dds.idl_file = RequireString(ddsNode, "idl_file");

        common::MappingDefinition mapping;
        mapping.name = RequireString(entry, "name");
        mapping.id = RequireInt(ddsNode, "id");

        const std::string rawField = RequireString(ddsNode, "field");
        mapping.field = NormalizeFieldName(rawField);

        const std::string targetStr = RequireString(mappingOutputNode, "to");
        mapping.target = targetStr;

        // Explicit `type:` under the output/gamepad node overrides prefix inference.
        // Required for custom channel names (e.g. UDP protobuf fields like "steering")
        // where the type cannot be inferred from the target string prefix.
        const std::string channelTypeStr = OptionalString(mappingOutputNode, "type", "");
        if (!channelTypeStr.empty()) {
            mapping.channelType = ParseChannelType(channelTypeStr);
        } else {
            mapping.channelType = InferChannelType(targetStr);
        }

        mapping.scale = OptionalFloat(mappingOutputNode, "scale", 1.0f);
        mapping.deadzone = OptionalFloat(mappingOutputNode, "deadzone", 0.0f);
        mapping.invert = OptionalBool(mappingOutputNode, "invert", false);
        mapping.additive = OptionalBool(mappingOutputNode, "additive", false);

        if (ddsNode["input_min"] && ddsNode["input_max"]) {
            mapping.input_min = ddsNode["input_min"].as<float>();
            mapping.input_max = ddsNode["input_max"].as<float>();
            mapping.has_input_range = true;
        }

        ValidateMappingType(dds.type, rawField, mapping);

        if ((mapping.channelType == common::ChannelType::Trigger ||
             mapping.channelType == common::ChannelType::Axis) &&
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
            appConfig.topicType = ParseTopicType(dds.type);
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
