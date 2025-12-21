// main.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dds_includes.h"
#include "Value.hpp"

#ifdef _WIN32
#include "emulator/VigemClient.h"
#endif

namespace {
void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <sub_topic> [domain_id]" << std::endl;
}

uint8_t scale_to_trigger(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(clamped * 255.0f));
}
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string sub_topic_name = argv[1];
    if (sub_topic_name.empty()) {
        std::cerr << "Sub topic name must not be empty." << std::endl;
        return EXIT_FAILURE;
    }

    int domain_id = 0;
    if (argc >= 3) {
        try {
            size_t parsed = 0;
            domain_id = std::stoi(argv[2], &parsed);
            if (parsed != std::string(argv[2]).size() || domain_id < 0) {
                throw std::invalid_argument("domain_id");
            }
        } catch (const std::exception&) {
            std::cerr << "Invalid domain_id: " << argv[2] << std::endl;
            return EXIT_FAILURE;
        }
    }

    dds::domain::DomainParticipant participant(domain_id);

    dds::topic::Topic<Value::Msg> sub_topic(participant, sub_topic_name);

    dds::sub::Subscriber subscriber(participant);

    dds::sub::DataReader<Value::Msg> reader(subscriber, sub_topic);

#ifdef _WIN32
    emulator::VigemClient gamepad;
    if (!gamepad.Connect()) {
        std::cerr << "Failed to connect to ViGEm: " << gamepad.LastError() << std::endl;
        return EXIT_FAILURE;
    }
    if (!gamepad.AddX360Controller()) {
        std::cerr << "Failed to add Xbox 360 controller: " << gamepad.LastError() << std::endl;
        return EXIT_FAILURE;
    }
#else
    std::cout << "ViGEm is only available on Windows; DDS values will be logged only."
              << std::endl;
#endif

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
#ifdef _WIN32
            if (!gamepad.UpdateRightTrigger(trigger_value)) {
                std::cerr << "Failed to update right trigger: " << gamepad.LastError()
                          << std::endl;
            }
#endif
            std::cout << "rx messageID=" << s.data().messageID()
                      << " value=" << raw_value
                      << " -> trigger=" << static_cast<int>(trigger_value) << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
