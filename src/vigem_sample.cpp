#include "emulator/VigemClient.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    emulator::VigemClient client;

    try {
        client.Connect();
        client.AddX360Controller();
    } catch (const std::runtime_error& e) {
        std::cerr << "ViGEm setup failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Sweeping right trigger from 0 to 255 for 3 seconds...\n";
    const auto start = std::chrono::steady_clock::now();
    uint8_t value = 0;
    bool increasing = true;

    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
        if (!client.UpdateRightTrigger(value)) {
            std::cerr << "Failed to update right trigger: " << client.LastError() << "\n";
            return 1;
        }

        if (increasing) {
            if (value >= 250) {
                increasing = false;
            } else {
                value = static_cast<uint8_t>(value + 5);
            }
        } else {
            if (value <= 5) {
                increasing = true;
            } else {
                value = static_cast<uint8_t>(value - 5);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "Done.\n";
    return 0;
}
