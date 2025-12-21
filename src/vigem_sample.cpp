#include "emulator/VigemClient.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    emulator::VigemClient client;

    if (!client.Connect()) {
        std::cerr << "Failed to connect to ViGEm: " << client.LastError() << "\n";
        return 1;
    }

    if (!client.AddX360Controller()) {
        std::cerr << "Failed to add Xbox 360 controller: " << client.LastError() << "\n";
        return 1;
    }

    if (!client.UpdateRightTrigger(200)) {
        std::cerr << "Failed to update right trigger: " << client.LastError() << "\n";
        return 1;
    }

    std::cout << "Right trigger set to 200. Keeping device alive for 3 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "Done.\n";
    return 0;
}
