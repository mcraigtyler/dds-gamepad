// main.cpp
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#ifdef _WIN32
#include "emulator/VigemClient.h"
#include "console/RxTable.h"
#endif

#include "config/ConfigLoader.h"
#include "dds_includes.h"
#include "mapper/MappingEngine.h"
#include "Gamepad.hpp"

namespace {
enum class TopicType {
    GamepadAnalog,
    StickTwoAxis
};

TopicType ParseTopicType(const std::string& type) {
    if (type == "Gamepad::Gamepad_Analog" || type == "Gamepad_Analog") {
        return TopicType::GamepadAnalog;
    }
    if (type == "Gamepad::Stick_TwoAxis" || type == "Stick_TwoAxis") {
        return TopicType::StickTwoAxis;
    }

    throw std::runtime_error("Unsupported DDS type '" + type +
                             "'. Expected Gamepad::Gamepad_Analog or Gamepad::Stick_TwoAxis.");
}

dds::sub::qos::DataReaderQos MakeReaderQos(const dds::sub::Subscriber& subscriber) {
    // If the publisher writes multiple updates back-to-back for what are
    // logically different controls (e.g., brake + throttle) but the keyed
    // field is not set correctly, DDS may treat them as the same instance and
    // KeepLast(1) will drop earlier samples. Keep a small history depth to
    // avoid losing those updates.
    auto qos = subscriber.default_datareader_qos();
    qos << dds::core::policy::History::KeepLast(16);
    return qos;
}

std::string FormatBusId(const Common::BusIdentifier_t& id) {
    std::ostringstream out;
    out << id.role() << ":" << id.sub_role();
    return out.str();
}

// Resolve the logical `role` value from message payload. Some IDL variants
// place the BusIdentifier in `id` while others use `another_id`. Prefer a
// non-zero role value when available.
template <typename T>
int ResolveRoleFromData(const T& data) {
    int role = 0;
    // primary id field
    role = data.id().role();
    if (role != 0) return role;
    // fallback to alternate id field present on some IDL structs
    role = data.another_id().role();
    return role;
}

template <typename T>
const Common::BusIdentifier_t& ResolveBusIdFromData(const T& data) {
    const auto& primary = data.id();
    if (primary.role() != 0 || primary.sub_role() != 0) {
        return primary;
    }
    const auto& alternate = data.another_id();
    if (alternate.role() != 0 || alternate.sub_role() != 0) {
        return alternate;
    }
    return primary;
}

void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <config_dir> [domain_id] [--debug | --table]" << std::endl;
    std::cerr << "       " << exe << " --help" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Args:" << std::endl;
    std::cerr << "  <config_dir>      Directory containing one or more YAML config files." << std::endl;
    std::cerr << "  [domain_id]       Optional DDS domain id override (integer)." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --debug            Enable verbose raw input logging (rx_raw...)." << std::endl;
    std::cerr << "  --table            Live table (one row per topic+id). Displays rx only; no scrolling." << std::endl;
    std::cerr << "  -h, --help         Show this help text." << std::endl;
}

#ifdef _WIN32
struct RxOutput
{
    bool logRxRaw = false;
    bool logRx = true;
    console::RxTable* table = nullptr;
};
#else
struct RxOutput
{
    bool logRxRaw = false;
    bool logRx = true;
};
#endif

int ResolveDomainId(const std::vector<config::AppConfig>& configs) {
    int domain_id = 0;
    bool has_domain_id = false;
    for (const auto& config : configs) {
        if (!config.dds.has_domain_id) {
            continue;
        }
        if (!has_domain_id) {
            domain_id = config.dds.domain_id;
            has_domain_id = true;
        } else if (domain_id != config.dds.domain_id) {
            throw std::runtime_error("Configs specify multiple domain_id values; supply [domain_id] on the command line.");
        }
    }
    return domain_id;
}

struct AnalogHandler {
    std::string name;
    dds::topic::Topic<Gamepad::Gamepad_Analog> topic;
    dds::sub::DataReader<Gamepad::Gamepad_Analog> reader;
    mapper::MappingEngine mapping_engine;

    AnalogHandler(dds::domain::DomainParticipant& participant,
                  dds::sub::Subscriber& subscriber,
                  std::string topic_name,
                  mapper::MappingEngine engine)
        : name(std::move(topic_name)),
          topic(participant, name),
                    reader(subscriber, topic, MakeReaderQos(subscriber)),
          mapping_engine(std::move(engine)) {}
};

struct StickHandler {
    std::string name;
    dds::topic::Topic<Gamepad::Stick_TwoAxis> topic;
    dds::sub::DataReader<Gamepad::Stick_TwoAxis> reader;
    mapper::MappingEngine mapping_engine;

    StickHandler(dds::domain::DomainParticipant& participant,
                 dds::sub::Subscriber& subscriber,
                 std::string topic_name,
                 mapper::MappingEngine engine)
        : name(std::move(topic_name)),
          topic(participant, name),
                    reader(subscriber, topic, MakeReaderQos(subscriber)),
          mapping_engine(std::move(engine)) {}
};

#ifdef _WIN32
bool ProcessAnalogSamples(AnalogHandler& handler,
                          mapper::GamepadState& state,
                          emulator::VigemClient& client,
                          const RxOutput& output) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const int message_id = ResolveRoleFromData(data);
        const float raw_value = static_cast<float>(data.value());

        const std::string bus_id = FormatBusId(ResolveBusIdFromData(data));
        if (output.table != nullptr) {
            std::ostringstream value;
            value << std::fixed << std::setprecision(3) << raw_value;
            output.table->Update(handler.name, bus_id, value.str());
        } else {
            if (output.logRxRaw) {
                std::cout << "rx_raw topic=" << handler.name
                          << " id=" << bus_id
                          << " value=" << raw_value << std::endl;
            }
        }

        if (!handler.mapping_engine.Apply("value", message_id, raw_value, state)) {
            continue;
        }
        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }
        if (output.table == nullptr && output.logRx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << bus_id
                      << " value=" << raw_value
                      << " -> LT=" << static_cast<int>(state.left_trigger)
                      << " RT=" << static_cast<int>(state.right_trigger)
                      << " LX=" << state.left_stick_x
                      << " LY=" << state.left_stick_y
                      << " RX=" << state.right_stick_x
                      << " RY=" << state.right_stick_y
                      << std::endl;
        }
    }
    return true;
}

bool ProcessStickSamples(StickHandler& handler,
                         mapper::GamepadState& state,
                         emulator::VigemClient& client,
                         const RxOutput& output) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const int message_id = ResolveRoleFromData(data);
        const float raw_x = static_cast<float>(data.x());
        const float raw_y = static_cast<float>(data.y());

        const std::string bus_id = FormatBusId(ResolveBusIdFromData(data));
        if (output.table != nullptr) {
            std::ostringstream value;
            value << "x=" << std::fixed << std::setprecision(3) << raw_x
                  << " y=" << std::fixed << std::setprecision(3) << raw_y;
            output.table->Update(handler.name, bus_id, value.str());
        } else {
            if (output.logRxRaw) {
                std::cout << "rx_raw topic=" << handler.name
                          << " id=" << bus_id
                          << " x=" << raw_x
                          << " y=" << raw_y << std::endl;
            }
        }

        bool updated = handler.mapping_engine.Apply("x", message_id, raw_x, state);
        updated = handler.mapping_engine.Apply("y", message_id, raw_y, state) || updated;
        if (!updated) {
            continue;
        }
        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }
        if (output.table == nullptr && output.logRx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << bus_id
                      << " x=" << raw_x
                      << " y=" << raw_y
                      << " -> LT=" << static_cast<int>(state.left_trigger)
                      << " RT=" << static_cast<int>(state.right_trigger)
                      << " LX=" << state.left_stick_x
                      << " LY=" << state.left_stick_y
                      << " RX=" << state.right_stick_x
                      << " RY=" << state.right_stick_y
                      << std::endl;
        }
    }
    return true;
}
#else
bool ProcessAnalogSamples(AnalogHandler& handler,
                          mapper::GamepadState& state,
                          bool log_rx_raw,
                          bool log_rx) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const int message_id = ResolveRoleFromData(data);
        const float raw_value = static_cast<float>(data.value());
        if (log_rx_raw) {
            std::cout << "rx_raw topic=" << handler.name
                      << " id=" << FormatBusId(ResolveBusIdFromData(data))
                      << " value=" << raw_value << std::endl;
        }
        if (!handler.mapping_engine.Apply("value", message_id, raw_value, state)) {
            continue;
        }
        if (log_rx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << FormatBusId(ResolveBusIdFromData(data))
                      << " value=" << raw_value
                      << " -> LT=" << static_cast<int>(state.left_trigger)
                      << " RT=" << static_cast<int>(state.right_trigger)
                      << " LX=" << state.left_stick_x
                      << " LY=" << state.left_stick_y
                      << " RX=" << state.right_stick_x
                      << " RY=" << state.right_stick_y
                      << std::endl;
        }
    }
    return true;
}

bool ProcessStickSamples(StickHandler& handler,
                         mapper::GamepadState& state,
                         bool log_rx_raw,
                         bool log_rx) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const int message_id = ResolveRoleFromData(data);
        const float raw_x = static_cast<float>(data.x());
        const float raw_y = static_cast<float>(data.y());
        if (log_rx_raw) {
            std::cout << "rx_raw topic=" << handler.name
                      << " id=" << FormatBusId(ResolveBusIdFromData(data))
                      << " x=" << raw_x
                      << " y=" << raw_y << std::endl;
        }
        bool updated = handler.mapping_engine.Apply("x", message_id, raw_x, state);
        updated = handler.mapping_engine.Apply("y", message_id, raw_y, state) || updated;
        if (!updated) {
            continue;
        }
        if (log_rx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << FormatBusId(ResolveBusIdFromData(data))
                      << " x=" << raw_x
                      << " y=" << raw_y
                      << " -> LT=" << static_cast<int>(state.left_trigger)
                      << " RT=" << static_cast<int>(state.right_trigger)
                      << " LX=" << state.left_stick_x
                      << " LY=" << state.left_stick_y
                      << " RX=" << state.right_stick_x
                      << " RY=" << state.right_stick_y
                      << std::endl;
        }
    }
    return true;
}
#endif
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    bool log_rx_raw = false;
    bool table_mode = false;
    std::string config_path;
    std::optional<int> domain_override;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (arg == "--debug") {
            log_rx_raw = true;
            continue;
        }
        if (arg == "--table") {
            table_mode = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        if (config_path.empty()) {
            config_path = arg;
            continue;
        }
        if (!domain_override.has_value()) {
            try {
                domain_override = std::stoi(arg);
            } catch (const std::exception&) {
                std::cerr << "Invalid domain_id: " << arg << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }

        std::cerr << "Unexpected argument: " << arg << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (config_path.empty()) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (table_mode && log_rx_raw) {
        std::cerr << "Options --debug and --table are mutually exclusive." << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

#ifndef _WIN32
    if (table_mode) {
        std::cerr << "--table is only supported on Windows." << std::endl;
        return EXIT_FAILURE;
    }
#endif

    std::vector<config::AppConfig> configs;
    try {
        configs = config::ConfigLoader::LoadDirectory(config_path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load config: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    int domain_id = 0;
    try {
        domain_id = ResolveDomainId(configs);
    } catch (const std::exception& ex) {
        std::cerr << "Invalid domain_id: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    if (domain_override.has_value()) {
        domain_id = *domain_override;
    }

#ifdef _WIN32
    emulator::VigemClient client;
    if (!client.Connect()) {
        std::cerr << "Failed to connect to ViGEm: " << client.LastError() << std::endl;
        return EXIT_FAILURE;
    }
    if (!client.AddX360Controller()) {
        std::cerr << "Failed to add Xbox 360 controller: " << client.LastError() << std::endl;
        return EXIT_FAILURE;
    }
#else
    if (!table_mode) {
        std::cout << "ViGEm output is only supported on Windows; running in log-only mode." << std::endl;
    }
#endif

    dds::domain::DomainParticipant participant(domain_id);
    dds::sub::Subscriber subscriber(participant);

    using TopicHandler = std::variant<AnalogHandler, StickHandler>;
    std::vector<TopicHandler> handlers;
    handlers.reserve(configs.size());
    for (const auto& config : configs) {
        if (!table_mode) {
            std::cout << "Subscribing to '" << config.dds.topic << "' (domain " << domain_id << ")"
                      << std::endl;
        }
        switch (ParseTopicType(config.dds.type)) {
            case TopicType::GamepadAnalog:
                handlers.emplace_back(AnalogHandler(participant,
                                                    subscriber,
                                                    config.dds.topic,
                                                    mapper::MappingEngine(config.mappings)));
                break;
            case TopicType::StickTwoAxis:
                handlers.emplace_back(StickHandler(participant,
                                                   subscriber,
                                                   config.dds.topic,
                                                   mapper::MappingEngine(config.mappings)));
                break;
        }
    }

    mapper::GamepadState state;

#ifdef _WIN32
    console::RxTable table;
    console::RxTable* table_ptr = nullptr;
    if (table_mode) {
        if (!table.Begin()) {
            std::cerr << "Failed to initialize console table output." << std::endl;
            return EXIT_FAILURE;
        }
        table_ptr = &table;
    }
    RxOutput output;
    output.logRxRaw = log_rx_raw;
    output.logRx = !table_mode;
    output.table = table_ptr;
#else
    const bool log_rx = true;
#endif

    while (true) {
        for (auto& handler : handlers) {
#ifdef _WIN32
            struct HandlerVisitor {
                mapper::GamepadState& state;
                emulator::VigemClient& client;
                const RxOutput& output;

                bool operator()(AnalogHandler& topic_handler) const {
                    return ProcessAnalogSamples(topic_handler, state, client, output);
                }

                bool operator()(StickHandler& topic_handler) const {
                    return ProcessStickSamples(topic_handler, state, client, output);
                }
            };
            bool ok = std::visit(HandlerVisitor{state, client, output}, handler);
#else
            struct HandlerVisitor {
                mapper::GamepadState& state;
                bool log_rx_raw;
                bool log_rx;

                bool operator()(AnalogHandler& topic_handler) const {
                    return ProcessAnalogSamples(topic_handler, state, log_rx_raw, log_rx);
                }

                bool operator()(StickHandler& topic_handler) const {
                    return ProcessStickSamples(topic_handler, state, log_rx_raw, log_rx);
                }
            };
            bool ok = std::visit(HandlerVisitor{state, log_rx_raw, log_rx}, handler);
#endif
            if (!ok) {
                return EXIT_FAILURE;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
