// main.cpp
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include "emulator/VigemClient.h"
#endif

#include "config/ConfigLoader.h"
#include "dds_includes.h"
#include "mapper/MappingEngine.h"
#include "Value.hpp"

namespace {
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

    struct TopicHandler {
        std::string name;
        dds::topic::Topic<Value::Msg> topic;
        dds::sub::DataReader<Value::Msg> reader;
        mapper::MappingEngine mapping_engine;

        TopicHandler(dds::domain::DomainParticipant& participant,
                     dds::sub::Subscriber& subscriber,
                     std::string topic_name,
                     mapper::MappingEngine engine)
            : name(std::move(topic_name)),
              topic(participant, name),
              reader(subscriber, topic),
              mapping_engine(std::move(engine)) {}
    };

    std::vector<TopicHandler> handlers;
    handlers.reserve(configs.size());
    for (const auto& config : configs) {
        std::cout << "Subscribing to '" << config.dds.topic << "' (domain " << domain_id << ")"
                  << std::endl;
        handlers.emplace_back(participant,
                              subscriber,
                              config.dds.topic,
                              mapper::MappingEngine(config.mappings));
    }

    mapper::GamepadState state;

    while (true) {
        for (auto& handler : handlers) {
            auto samples = handler.reader.take();
            for (const auto& s : samples) {
                if (!s.info().valid()) {
                    continue;
                }
                const int message_id = s.data().messageID();
                const float raw_value = s.data().value();
                std::cout << "rx topic=" << handler.name
                          << " messageID=" << message_id
                          << " value=" << raw_value
                          << std::endl;

                if (!handler.mapping_engine.Apply(message_id, raw_value, state)) {
                    std::cout << "rx topic=" << handler.name
                              << " messageID=" << message_id
                              << " skipped (no mapping matched)"
                              << std::endl;
                    continue;
                }

#ifdef _WIN32
                if (!client.UpdateState(state)) {
                    std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
                    return EXIT_FAILURE;
                }
#endif
                std::cout << "mapped topic=" << handler.name
                          << " messageID=" << message_id
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

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
