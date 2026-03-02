#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/MappingDefinition.h"

namespace config {

/// @brief Raw DDS subscription parameters from the `dds:` YAML section.
///
/// @details Stored alongside each `AppConfig` for traceability, but the
/// string-based `type` field is resolved to `common::TopicType` during
/// `ConfigLoader::Load` so that `AppRunner` never performs string comparisons
/// at run time.
struct DdsConfig {
    std::string topic;    ///< DDS topic name (e.g. `"Gamepad_Stick_TwoAxis"`).
    std::string type;     ///< Fully-qualified IDL type (e.g. `"Gamepad::Stick_TwoAxis"`).
    std::string idl_file; ///< Path to the IDL file (e.g. `"idl/Gamepad.idl"`).
};

/// @brief Groups all mapping definitions that share a single DDS subscription.
///
/// @details One `AppConfig` corresponds to one unique `{topic, type, idl_file}`
/// triplet in the YAML config. Multiple `MappingDefinition` entries can share
/// the same subscription (e.g. `x` and `y` fields from the same
/// `Gamepad_Stick_TwoAxis` topic both live in one `AppConfig`).
/// `AppRunner` creates one `TopicHandler` per `AppConfig`.
struct AppConfig {
    DdsConfig dds;   ///< Raw DDS subscription parameters.
    /// @brief Resolved topic type used at run time instead of `dds.type` string.
    /// @details Parsed from `dds.type` during `ConfigLoader::Load` so `AppRunner`
    ///          never needs string comparisons at setup time.
    common::TopicType topicType = common::TopicType::GamepadAnalog;
    std::vector<common::MappingDefinition> mappings; ///< Mappings within this subscription.
};

/// @brief Output backend selection and connection parameters.
///
/// @details Parsed from the optional top-level `output:` YAML section.
/// If the `output:` key is absent the type defaults to `"vigem_x360"`,
/// which is backwards-compatible with role files that pre-date the
/// multi-backend support.
///
/// Supported types:
/// - `"vigem_x360"` — virtual Xbox 360 controller via ViGEmBus. `host` and
///   `port` are ignored.
/// - `"udp_protobuf"` — UDP socket with protobuf serialisation (Phase 5 stub;
///   currently a no-op). Requires `host` and `port`.
struct OutputConfig {
    std::string type = "vigem_x360"; ///< Backend type: `"vigem_x360"` or `"udp_protobuf"`.
    std::string host;                ///< UDP target host. Required for `udp_protobuf`.
    uint16_t    port = 0;            ///< UDP target port. Required for `udp_protobuf`.
};

/// @brief Top-level configuration for a single operator role.
///
/// @details `RoleConfig` is the root parse result returned by
/// `ConfigLoader::Load`. It contains everything needed to set up one complete
/// DDS-to-gamepad session: the role name, the output backend selection, and
/// the full set of DDS subscriptions with their mappings.
///
/// Each entry in `app_configs` becomes one `TopicHandler` in `AppRunner`.
struct RoleConfig {
    std::string name;                    ///< Human-readable role name (e.g. `"Driver"`).
    OutputConfig output;                 ///< Output backend selection and parameters.
    std::vector<AppConfig> app_configs;  ///< One entry per unique DDS subscription.
};

/// @brief Parses a role YAML file into a `RoleConfig`.
class ConfigLoader {
public:
    /// @brief Loads and validates a role YAML configuration file.
    ///
    /// @details Reads the file at `path`, validates the structure, resolves
    /// `dds.type` strings to `common::TopicType` enum values, and populates
    /// all mapping definition fields including optional input-range and
    /// additive flags.
    ///
    /// **Validation performed:**
    /// - Required fields are present (`role.name`, `mappings[].dds.*`,
    ///   `mappings[].output.to`).
    /// - `dds.type` is one of the three supported IDL types.
    /// - `deadzone` is in [0.0, 1.0].
    /// - `output.type` is `"vigem_x360"` or `"udp_protobuf"`.
    ///
    /// @param[in] path Filesystem path to the YAML role config file.
    /// @return Populated `RoleConfig` ready for use by `AppRunner`.
    /// @throws YAML::BadFile If the file cannot be opened.
    /// @throws std::runtime_error If required fields are missing, a DDS type
    ///         string is unrecognised, or a field value is out of range.
    static RoleConfig Load(const std::string& path);
};

}  // namespace config
