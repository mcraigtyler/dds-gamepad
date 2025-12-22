// main.cpp
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#ifdef _WIN32
#include "emulator/VigemClient.h"
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

std::string FormatBusId(const Common::BusIdentifier_t& id) {
    std::ostringstream out;
    out << id.role() << ":" << id.sub_role();
    return out.str();
}

void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <config_dir> [domain_id]" << std::endl;
}

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
          reader(subscriber, topic),
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
          reader(subscriber, topic),
          mapping_engine(std::move(engine)) {}
};

#ifdef _WIN32
bool ProcessAnalogSamples(AnalogHandler& handler,
                          mapper::GamepadState& state,
                          emulator::VigemClient& client) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const float raw_value = static_cast<float>(data.value());
        std::cout << "rx_raw topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " value=" << raw_value << std::endl;
        if (!handler.mapping_engine.Apply("value", raw_value, state)) {
            continue;
        }
        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }
        std::cout << "rx topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " value=" << raw_value
                  << " -> LT=" << static_cast<int>(state.left_trigger)
                  << " RT=" << static_cast<int>(state.right_trigger)
                  << " LX=" << state.left_stick_x
                  << " LY=" << state.left_stick_y
                  << " RX=" << state.right_stick_x
                  << " RY=" << state.right_stick_y
                  << std::endl;
    }
    return true;
}

bool ProcessStickSamples(StickHandler& handler,
                         mapper::GamepadState& state,
                         emulator::VigemClient& client) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const float raw_x = static_cast<float>(data.x());
        const float raw_y = static_cast<float>(data.y());
        std::cout << "rx_raw topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " x=" << raw_x
                  << " y=" << raw_y << std::endl;
        bool updated = handler.mapping_engine.Apply("x", raw_x, state);
        updated = handler.mapping_engine.Apply("y", raw_y, state) || updated;
        if (!updated) {
            continue;
        }
        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }
        std::cout << "rx topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
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
    return true;
}
#else
bool ProcessAnalogSamples(AnalogHandler& handler,
                          mapper::GamepadState& state) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const float raw_value = static_cast<float>(data.value());
        std::cout << "rx_raw topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " value=" << raw_value << std::endl;
        if (!handler.mapping_engine.Apply("value", raw_value, state)) {
            continue;
        }
        std::cout << "rx topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " value=" << raw_value
                  << " -> LT=" << static_cast<int>(state.left_trigger)
                  << " RT=" << static_cast<int>(state.right_trigger)
                  << " LX=" << state.left_stick_x
                  << " LY=" << state.left_stick_y
                  << " RX=" << state.right_stick_x
                  << " RY=" << state.right_stick_y
                  << std::endl;
    }
    return true;
}

bool ProcessStickSamples(StickHandler& handler,
                         mapper::GamepadState& state) {
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        const auto& data = s.data();
        const float raw_x = static_cast<float>(data.x());
        const float raw_y = static_cast<float>(data.y());
        std::cout << "rx_raw topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
                  << " x=" << raw_x
                  << " y=" << raw_y << std::endl;
        bool updated = handler.mapping_engine.Apply("x", raw_x, state);
        updated = handler.mapping_engine.Apply("y", raw_y, state) || updated;
        if (!updated) {
            continue;
        }
        std::cout << "rx topic=" << handler.name
                  << " id=" << FormatBusId(data.id())
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

    const std::string config_path = argv[1];

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
    if (argc >= 3) {
        try {
            domain_id = std::stoi(argv[2]);
        } catch (const std::exception&) {
            std::cerr << "Invalid domain_id: " << argv[2] << std::endl;
            return EXIT_FAILURE;
        }
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
    std::cout << "ViGEm output is only supported on Windows; running in log-only mode." << std::endl;
#endif

    dds::domain::DomainParticipant participant(domain_id);
    dds::sub::Subscriber subscriber(participant);

    using TopicHandler = std::variant<AnalogHandler, StickHandler>;
    std::vector<TopicHandler> handlers;
    handlers.reserve(configs.size());
    for (const auto& config : configs) {
        std::cout << "Subscribing to '" << config.dds.topic << "' (domain " << domain_id << ")"
                  << std::endl;
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

    while (true) {
        for (auto& handler : handlers) {
#ifdef _WIN32
            struct HandlerVisitor {
                mapper::GamepadState& state;
                emulator::VigemClient& client;

                bool operator()(AnalogHandler& topic_handler) const {
                    return ProcessAnalogSamples(topic_handler, state, client);
                }

                bool operator()(StickHandler& topic_handler) const {
                    return ProcessStickSamples(topic_handler, state, client);
                }
            };
            bool ok = std::visit(HandlerVisitor{state, client}, handler);
#else
            struct HandlerVisitor {
                mapper::GamepadState& state;

                bool operator()(AnalogHandler& topic_handler) const {
                    return ProcessAnalogSamples(topic_handler, state);
                }

                bool operator()(StickHandler& topic_handler) const {
                    return ProcessStickSamples(topic_handler, state);
                }
            };
            bool ok = std::visit(HandlerVisitor{state}, handler);
#endif
            if (!ok) {
                return EXIT_FAILURE;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
