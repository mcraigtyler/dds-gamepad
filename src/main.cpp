// main.cpp
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

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
    std::cerr << "Usage: " << exe << " <config.yaml> [domain_id]" << std::endl;
}
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string config_path = argv[1];

    config::AppConfig app_config;
    try {
        app_config = config::ConfigLoader::Load(config_path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load config: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    int domain_id = app_config.dds.has_domain_id ? app_config.dds.domain_id : 0;
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

    mapper::MappingEngine mapping_engine(app_config.mappings);

    dds::domain::DomainParticipant participant(domain_id);
    dds::topic::Topic<Value::Msg> sub_topic(participant, app_config.dds.topic);

    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader<Value::Msg> reader(subscriber, sub_topic);

    std::cout << "Subscribing to '" << app_config.dds.topic << "' (domain " << domain_id << ")"
              << std::endl;

    while (true) {
        auto samples = reader.take();
        for (const auto& s : samples) {
            if (!s.info().valid()) {
                continue;
            }
            const int message_id = s.data().messageID();
            const float raw_value = s.data().value();
            if (!mapping_engine.Apply(message_id, raw_value)) {
                continue;
            }

#ifdef _WIN32
            if (!client.UpdateState(mapping_engine.State())) {
                std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
                return EXIT_FAILURE;
            }
#endif
            const auto& state = mapping_engine.State();
            std::cout << "rx messageID=" << message_id
                      << " value=" << raw_value
                      << " -> LT=" << static_cast<int>(state.left_trigger)
                      << " RT=" << static_cast<int>(state.right_trigger)
                      << " LX=" << state.left_stick_x
                      << " LY=" << state.left_stick_y
                      << " RX=" << state.right_stick_x
                      << " RY=" << state.right_stick_y
                      << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
