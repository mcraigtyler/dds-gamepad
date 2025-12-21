// main.cpp
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dds_includes.h"
#include "Value.hpp"

namespace {
void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <sub_topic> <pub_topic> [domain_id]" << std::endl;
}
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string sub_topic_name = argv[1];
    const std::string pub_topic_name = argv[2];

    int domain_id = 0;
    if (argc >= 4) {
        try {
            domain_id = std::stoi(argv[3]);
        } catch (const std::exception&) {
            std::cerr << "Invalid domain_id: " << argv[3] << std::endl;
            return EXIT_FAILURE;
        }
    }

    dds::domain::DomainParticipant participant(domain_id);

    dds::topic::Topic<Value::Msg> sub_topic(participant, sub_topic_name);
    dds::topic::Topic<Value::Msg> pub_topic(participant, pub_topic_name);

    dds::sub::Subscriber subscriber(participant);
    dds::pub::Publisher publisher(participant);

    dds::sub::DataReader<Value::Msg> reader(subscriber, sub_topic);
    dds::pub::DataWriter<Value::Msg> writer(publisher, pub_topic);

    std::cout << "Subscribing to '" << sub_topic_name << "', publishing to '" << pub_topic_name
              << "' (domain " << domain_id << ")" << std::endl;

    uint32_t message_id = 0;
    auto next_pub = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_pub) {
            Value::Msg msg;
            msg.messageID(static_cast<long>(message_id));
            msg.value(static_cast<float>(message_id));
            writer.write(msg);
            ++message_id;
            next_pub = now + std::chrono::seconds(1);
        }

        auto samples = reader.take();
        for (const auto& s : samples) {
            if (!s.info().valid()) {
                continue;
            }
            std::cout << "rx messageID=" << s.data().messageID()
                      << " value=" << s.data().value() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
