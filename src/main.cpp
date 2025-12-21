// main.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include "emulator/VigemClient.h"
#endif

#include "dds_includes.h"
#include "Value.hpp"

namespace {
void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <sub_topic> [domain_id]" << std::endl;
}

uint8_t scale_to_trigger(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    const float scaled = clamped * 255.0f;
    return static_cast<uint8_t>(std::lround(scaled));
}
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string sub_topic_name = argv[1];

    int domain_id = 0;
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
    dds::topic::Topic<Value::Msg> sub_topic(participant, sub_topic_name);

    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader<Value::Msg> reader(subscriber, sub_topic);

    std::cout << "Subscribing to '" << sub_topic_name << "' (domain " << domain_id << ")"
              << std::endl;

    while (true) {
        auto samples = reader.take();
        for (const auto& s : samples) {
            if (!s.info().valid()) {
                continue;
            }
            const float raw_value = s.data().value();
            const uint8_t trigger_value = scale_to_trigger(raw_value);
            std::cout << "rx messageID=" << s.data().messageID()
                      << " value=" << raw_value << " -> trigger=" << static_cast<int>(trigger_value)
                      << std::endl;

#ifdef _WIN32
            if (!client.UpdateRightTrigger(trigger_value)) {
                std::cerr << "Failed to update right trigger: " << client.LastError() << std::endl;
                return EXIT_FAILURE;
            }
#endif
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
