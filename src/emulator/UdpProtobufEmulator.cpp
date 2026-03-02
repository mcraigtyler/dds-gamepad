#include "emulator/UdpProtobufEmulator.h"

namespace emulator {

UdpProtobufEmulator::UdpProtobufEmulator(UdpProtobufConfig cfg)
    : cfg_(std::move(cfg)) {}

void UdpProtobufEmulator::Connect() {
    // TODO(Phase 5): Resolve cfg_.host, open SOCK_DGRAM UDP socket (WSAStartup on Windows).
}

bool UdpProtobufEmulator::UpdateState(const common::OutputState& /*state*/) {
    // TODO(Phase 5): Serialize state.channels to a protobuf message and sendto().
    return true;
}

std::string UdpProtobufEmulator::LastError() const {
    // TODO(Phase 5): Surface last socket or serialization error.
    return "";
}

}  // namespace emulator
