#pragma once

#include <cstdint>
#include <string>

#include "emulator/IOutputDevice.h"

namespace emulator {

// Configuration for the UDP + protobuf output backend.
// Populated from the top-level `output:` section in the role YAML.
struct UdpProtobufConfig {
    std::string host;
    uint16_t port = 0;
};

// Stub implementation of IOutputDevice for the udp_protobuf backend.
// All methods are no-ops; full UDP socket and protobuf serialization
// will be added in Phase 5 once the schema and transport are finalised.
class UdpProtobufEmulator final : public IOutputDevice {
public:
    explicit UdpProtobufEmulator(UdpProtobufConfig cfg);

    // IOutputDevice interface.
    void Connect() override;
    bool UpdateState(const common::OutputState& state) override;
    std::string LastError() const override;

private:
    UdpProtobufConfig cfg_;
};

}  // namespace emulator
