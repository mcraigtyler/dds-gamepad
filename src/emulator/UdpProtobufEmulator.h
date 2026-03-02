#pragma once

#include <cstdint>
#include <string>

#include "emulator/IOutputDevice.h"

namespace emulator {

/// @brief Connection parameters for the UDP + protobuf output backend.
///
/// @details Populated from the top-level `output:` section in the role YAML
/// when `output.type` is `"udp_protobuf"`. Both fields are required for the
/// full Phase 5 implementation.
struct UdpProtobufConfig {
    std::string host; ///< UDP target hostname or IP address (e.g. `"192.168.1.100"`).
    uint16_t port = 0; ///< UDP target port number.
};

/// @brief Stub `IOutputDevice` for the `udp_protobuf` backend.
///
/// @details **All methods are no-ops.** This class exists as a placeholder so
/// `AppRunner` can select the `udp_protobuf` backend from the YAML config and
/// compile cleanly. The full implementation — UDP socket management and
/// protobuf serialisation — is deferred to Phase 5 once the message schema and
/// transport layer are finalised.
///
/// @warning Do not use this in production. `UpdateState` silently discards all
///          channel values without sending anything over the network.
class UdpProtobufEmulator final : public IOutputDevice {
public:
    /// @brief Constructs the emulator with the given connection config.
    /// @param[in] cfg UDP target address and port. Stored but not used until
    ///            Phase 5 implements the socket.
    explicit UdpProtobufEmulator(UdpProtobufConfig cfg);

    /// @brief No-op. Does not open a socket.
    /// @note Phase 5 will bind the UDP socket and resolve the target address here.
    void Connect() override;

    /// @brief No-op. Discards all channel values without transmitting.
    /// @param[in] state Ignored.
    /// @return Always `true`.
    /// @note Phase 5 will serialise `state` to protobuf and send it via UDP here.
    bool UpdateState(const common::OutputState& state) override;

    /// @brief Returns an empty string (no errors can occur in a no-op).
    /// @return Always an empty string.
    std::string LastError() const override;

private:
    UdpProtobufConfig cfg_;
};

}  // namespace emulator
