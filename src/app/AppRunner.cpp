#include "app/AppRunner.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "app/StatusPoller.h"
#include "common/OutputState.h"
#include "console/RxTable.h"
#include "config/ConfigLoader.h"
#include "dds_includes.h"
#include "emulator/IOutputDevice.h"
#include "emulator/VigemClient.h"
#include "mapper/MappingEngine.h"

#include "Gamepad.hpp"

namespace app
{
namespace
{
template <typename T, typename = void>
struct HasHasValue : std::false_type {};

template <typename T>
struct HasHasValue<T, std::void_t<decltype(std::declval<const T&>().has_value())>> : std::true_type {};

template <typename T, typename = void>
struct HasValueMethod : std::false_type {};

template <typename T>
struct HasValueMethod<T, std::void_t<decltype(std::declval<const T&>().value())>> : std::true_type {};

template <typename T, typename = void>
struct HasDereference : std::false_type {};

template <typename T>
struct HasDereference<T, std::void_t<decltype(*std::declval<const T&>())>> : std::true_type {};

template <typename T>
bool TryReadBool(const T& source, bool& out)
{
    using Decayed = std::decay_t<T>;
    if constexpr (std::is_same_v<Decayed, bool>) {
        out = source;
        return true;
    } else if constexpr (std::is_integral_v<Decayed>) {
        out = source != 0;
        return true;
    } else if constexpr (HasHasValue<Decayed>::value && HasValueMethod<Decayed>::value) {
        if (!source.has_value()) {
            return false;
        }
        out = static_cast<bool>(source.value());
        return true;
    } else if constexpr (HasDereference<Decayed>::value && std::is_convertible_v<Decayed, bool>) {
        if (!static_cast<bool>(source)) {
            return false;
        }
        out = static_cast<bool>(*source);
        return true;
    } else {
        return false;
    }
}

template <typename T>
bool TryReadButtonState(const T& source, Common::ButtonState_t& out)
{
    using Decayed = std::decay_t<T>;
    if constexpr (std::is_same_v<Decayed, Common::ButtonState_t>) {
        out = source;
        return true;
    } else if constexpr (HasHasValue<Decayed>::value && HasValueMethod<Decayed>::value) {
        if (!source.has_value()) {
            return false;
        }
        out = source.value();
        return true;
    } else if constexpr (HasDereference<Decayed>::value && std::is_convertible_v<Decayed, bool>) {
        if (!static_cast<bool>(source)) {
            return false;
        }
        out = *source;
        return true;
    } else {
        return false;
    }
}

dds::sub::qos::DataReaderQos MakeReaderQos(const dds::sub::Subscriber& subscriber)
{
    auto qos = subscriber.default_datareader_qos();
    qos << dds::core::policy::History::KeepLast(16);
    return qos;
}

std::string FormatBusId(const Common::BusIdentifier_t& id)
{
    std::ostringstream out;
    out << id.role() << ":" << id.sub_role();
    return out.str();
}

template <typename T>
int ResolveRoleFromData(const T& data)
{
    int role = 0;
    role = data.id().role();
    if (role != 0) {
        return role;
    }
    role = data.another_id().role();
    return role;
}

template <typename T>
const Common::BusIdentifier_t& ResolveBusIdFromData(const T& data)
{
    const auto& primary = data.id();
    if (primary.role() != 0 || primary.sub_role() != 0) {
        return primary;
    }
    const auto& alternate = data.another_id();
    if (alternate.role() != 0 || alternate.sub_role() != 0) {
        return alternate;
    }
    return primary;
}


template <typename T>
bool MatchesYokeId(const T& data, int yokeId)
{
    const auto& busId = ResolveBusIdFromData(data);
    return busId.sub_role() == yokeId;
}

struct RxOutput
{
    bool logRxRaw = false;
    bool logRx = true;
    console::RxTable* table = nullptr;
};

template <typename MsgT>
struct TopicHandler {
    std::string name;
    dds::topic::Topic<MsgT> topic;
    dds::sub::DataReader<MsgT> reader;
    mapper::MappingEngine mappingEngine;
    uint64_t totalValidSamples = 0;
    std::unordered_set<std::string> seenIds;

    TopicHandler(dds::domain::DomainParticipant& participant,
                 dds::sub::Subscriber& subscriber,
                 std::string topicName,
                 mapper::MappingEngine engine)
        : name(std::move(topicName)),
          topic(participant, name),
          reader(subscriber, topic, MakeReaderQos(subscriber)),
          mappingEngine(std::move(engine))
    {
    }
};

// MessageTraits<MsgT> — per-type extraction, formatting, and Apply logic.
// FormatTable() returns nullopt to skip the sample entirely (pre-filter).
template <typename MsgT>
struct MessageTraits;

template <>
struct MessageTraits<Gamepad::Gamepad_Analog> {
    static std::optional<std::string> FormatTable(const Gamepad::Gamepad_Analog& data)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3) << static_cast<float>(data.value());
        return out.str();
    }

    static void LogRxRaw(const std::string& name, const std::string& busId,
                         const Gamepad::Gamepad_Analog& data)
    {
        std::cout << "rx_raw topic=" << name
                  << " id=" << busId
                  << " value=" << static_cast<float>(data.value()) << std::endl;
    }

    static bool Apply(const Gamepad::Gamepad_Analog& data, int messageId,
                      mapper::MappingEngine& engine, common::OutputState& state)
    {
        return engine.Apply("value", messageId, static_cast<float>(data.value()), state);
    }

    static void LogRx(const std::string& name, const std::string& busId,
                      const Gamepad::Gamepad_Analog& data)
    {
        std::cout << "rx topic=" << name
                  << " id=" << busId
                  << " value=" << static_cast<float>(data.value()) << std::endl;
    }
};

template <>
struct MessageTraits<Gamepad::Stick_TwoAxis> {
    static std::optional<std::string> FormatTable(const Gamepad::Stick_TwoAxis& data)
    {
        std::ostringstream out;
        out << "x=" << std::fixed << std::setprecision(3) << static_cast<float>(data.x())
            << " y=" << std::fixed << std::setprecision(3) << static_cast<float>(data.y());
        return out.str();
    }

    static void LogRxRaw(const std::string& name, const std::string& busId,
                         const Gamepad::Stick_TwoAxis& data)
    {
        std::cout << "rx_raw topic=" << name
                  << " id=" << busId
                  << " x=" << static_cast<float>(data.x())
                  << " y=" << static_cast<float>(data.y()) << std::endl;
    }

    static bool Apply(const Gamepad::Stick_TwoAxis& data, int messageId,
                      mapper::MappingEngine& engine, common::OutputState& state)
    {
        bool updated = engine.Apply("x", messageId, static_cast<float>(data.x()), state);
        updated = engine.Apply("y", messageId, static_cast<float>(data.y()), state) || updated;
        return updated;
    }

    static void LogRx(const std::string& name, const std::string& busId,
                      const Gamepad::Stick_TwoAxis& data)
    {
        std::cout << "rx topic=" << name
                  << " id=" << busId
                  << " x=" << static_cast<float>(data.x())
                  << " y=" << static_cast<float>(data.y()) << std::endl;
    }
};

template <>
struct MessageTraits<Gamepad::Button> {
    static std::optional<std::string> FormatTable(const Gamepad::Button& data)
    {
        bool btnChanging = false;
        if (!TryReadBool(data.btnChanging(), btnChanging) || !btnChanging) {
            return std::nullopt;
        }
        Common::ButtonState_t btnState = Common::ButtonState_t::Invalid;
        if (!TryReadButtonState(data.btnState(), btnState)) {
            return std::nullopt;
        }
        if (btnState != Common::ButtonState_t::Down && btnState != Common::ButtonState_t::Up) {
            return std::nullopt;
        }
        return std::string(btnState == Common::ButtonState_t::Down ? "down" : "up");
    }

    static void LogRxRaw(const std::string& name, const std::string& busId,
                         const Gamepad::Button& data)
    {
        Common::ButtonState_t btnState = Common::ButtonState_t::Invalid;
        TryReadButtonState(data.btnState(), btnState);
        std::cout << "rx_raw topic=" << name
                  << " id=" << busId
                  << " state=" << (btnState == Common::ButtonState_t::Down ? "down" : "up")
                  << " changing=true" << std::endl;
    }

    static bool Apply(const Gamepad::Button& data, int messageId,
                      mapper::MappingEngine& engine, common::OutputState& state)
    {
        Common::ButtonState_t btnState = Common::ButtonState_t::Invalid;
        TryReadButtonState(data.btnState(), btnState);
        const float mappedValue = (btnState == Common::ButtonState_t::Down) ? 1.0f : 0.0f;
        return engine.Apply("btnState", messageId, mappedValue, state);
    }

    static void LogRx(const std::string& name, const std::string& busId,
                      const Gamepad::Button& data)
    {
        Common::ButtonState_t btnState = Common::ButtonState_t::Invalid;
        TryReadButtonState(data.btnState(), btnState);
        std::cout << "rx topic=" << name
                  << " id=" << busId
                  << " state=" << (btnState == Common::ButtonState_t::Down ? "down" : "up")
                  << std::endl;
    }
};

template <typename MsgT>
bool ProcessSamples(TopicHandler<MsgT>& handler,
                    common::OutputState& state,
                    emulator::IOutputDevice& client,
                    const RxOutput& output,
                    int yokeId)
{
    using Traits = MessageTraits<MsgT>;
    auto samples = handler.reader.take();
    for (const auto& s : samples) {
        if (!s.info().valid()) {
            continue;
        }
        ++handler.totalValidSamples;
        const auto& data = s.data();
        if (!MatchesYokeId(data, yokeId)) {
            continue;
        }
        const std::string busId = FormatBusId(ResolveBusIdFromData(data));
        handler.seenIds.insert(busId);
        const int messageId = ResolveRoleFromData(data);

        auto tableValue = Traits::FormatTable(data);
        if (!tableValue.has_value()) {
            continue;  // pre-filtered (e.g. button not changing)
        }

        if (output.table != nullptr) {
            output.table->Update(handler.name, busId, *tableValue);
        } else if (output.logRxRaw) {
            Traits::LogRxRaw(handler.name, busId, data);
        }

        if (!Traits::Apply(data, messageId, handler.mappingEngine, state)) {
            continue;
        }

        if (!client.UpdateState(state)) {
            return false;
        }

        if (output.table == nullptr && output.logRx) {
            Traits::LogRx(handler.name, busId, data);
        }
    }
    return true;
}

void SleepWithStop(const StopToken& stopToken, std::chrono::milliseconds duration)
{
    const auto end = std::chrono::steady_clock::now() + duration;
    while (!stopToken.StopRequested() && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

class TableTxStateListener final : public emulator::ITxStateListener
{
public:
    explicit TableTxStateListener(console::RxTable* tablePtr)
        : _table(tablePtr)
    {
    }

    void OnTxState(const common::OutputState& txState) override
    {
        if (_table == nullptr) {
            return;
        }

        std::ostringstream out;
        out << "state";
        for (const auto& [key, val] : txState.channels) {
            out << " " << key << "=" << std::fixed << std::setprecision(3) << val;
        }
        _table->SetTxStateLine(out.str());
    }

private:
    console::RxTable* _table;
};

} // namespace

int AppRunner::Run(const AppRunnerOptions& options, const StopToken& stopToken)
{
    // Load config early so we can select the right output backend before
    // constructing it. The inner Run() overload will load it again; that
    // redundant parse is acceptable for a startup-only code path.
    if (options.configFile.empty()) {
        SetLastError("Missing required configFile.");
        std::cerr << LastError() << std::endl;
        return EXIT_FAILURE;
    }

    config::RoleConfig roleConfig;
    try {
        roleConfig = config::ConfigLoader::Load(options.configFile);
    } catch (const std::exception& ex) {
        SetLastError(std::string("Failed to load config: ") + ex.what());
        std::cerr << LastError() << std::endl;
        return EXIT_FAILURE;
    }

    const std::string& backendType = roleConfig.output.type;

    if (backendType == "vigem_x360") {
        try {
            emulator::VigemClient client;
            client.Connect();           // throws std::runtime_error on failure
            client.AddX360Controller(); // throws std::runtime_error on failure
            return Run(options, client, stopToken);
        } catch (const std::exception& ex) {
            SetLastError(std::string("ViGEm setup failed: ") + ex.what());
            std::cerr << LastError() << std::endl;
            return EXIT_FAILURE;
        } catch (...) {
            SetLastError("ViGEm setup failed: unknown exception.");
            std::cerr << LastError() << std::endl;
            return EXIT_FAILURE;
        }
    }

    SetLastError("Unknown output backend '" + backendType +
                 "'. Supported types: vigem_x360.");
    std::cerr << LastError() << std::endl;
    return EXIT_FAILURE;
}

int AppRunner::Run(const AppRunnerOptions& options,
                   emulator::IOutputDevice& client,
                   const StopToken& stopToken)
{
    _lastError.clear();

    if (options.configFile.empty()) {
        SetLastError("Missing required configFile.");
        std::cerr << LastError() << std::endl;
        return EXIT_FAILURE;
    }

    try {
        config::RoleConfig roleConfig;
        try {
            roleConfig = config::ConfigLoader::Load(options.configFile);
        } catch (const std::exception& ex) {
            SetLastError(std::string("Failed to load config: ") + ex.what());
            std::cerr << LastError() << std::endl;
            return EXIT_FAILURE;
        }

        // Console mode keeps previous behavior: log tx state unless table mode.
        // Service mode should set logTxState=false to avoid per-sample output.
        client.SetLogState(options.logTxState && !options.tableMode);

        dds::domain::DomainParticipant participant(options.domainId);
        dds::sub::Subscriber subscriber(participant);

        using HandlerVariant = std::variant<
            TopicHandler<Gamepad::Gamepad_Analog>,
            TopicHandler<Gamepad::Stick_TwoAxis>,
            TopicHandler<Gamepad::Button>
        >;
        std::vector<HandlerVariant> handlers;
        handlers.reserve(roleConfig.app_configs.size());
        for (const auto& config : roleConfig.app_configs) {
            if (options.logStartup && !options.tableMode) {
                std::cout << "Loaded config file: " << options.configFile << std::endl;
                std::cout << "Role: " << roleConfig.name << " (yoke_id=" << options.yokeId << ")" << std::endl;
                break;
            }
        }

        for (const auto& config : roleConfig.app_configs) {
            if (options.logStartup && !options.tableMode) {
                std::cout << "Subscribing to '" << config.dds.topic << "' (domain " << options.domainId << ", yoke_id " << options.yokeId << ")"
                          << std::endl;
                for (const auto& mapping : config.mappings) {
                    std::cout << "  mapping name=" << mapping.name
                              << " id=" << mapping.id
                              << " field=" << mapping.field
                              << std::endl;
                }
            }
            switch (config.topicType) {
                case common::TopicType::GamepadAnalog:
                    handlers.emplace_back(
                        TopicHandler<Gamepad::Gamepad_Analog>(participant, subscriber,
                                                              config.dds.topic,
                                                              mapper::MappingEngine(config.mappings)));
                    break;
                case common::TopicType::StickTwoAxis:
                    handlers.emplace_back(
                        TopicHandler<Gamepad::Stick_TwoAxis>(participant, subscriber,
                                                             config.dds.topic,
                                                             mapper::MappingEngine(config.mappings)));
                    break;
                case common::TopicType::GamepadButton:
                    handlers.emplace_back(
                        TopicHandler<Gamepad::Button>(participant, subscriber,
                                                      config.dds.topic,
                                                      mapper::MappingEngine(config.mappings)));
                    break;
            }
        }

    common::OutputState state;

    console::RxTable table;
    console::RxTable* tablePtr = nullptr;
    if (options.tableMode) {
        std::vector<std::string> topicNames;
        topicNames.reserve(handlers.size());
        for (const auto& handler : handlers) {
            std::visit([&](const auto& topicHandler) {
                topicNames.push_back(topicHandler.name);
            }, handler);
        }

        if (!table.Begin(topicNames, true)) {
            std::cerr << "Failed to initialize console table output." << std::endl;
            return EXIT_FAILURE;
        }
        tablePtr = &table;
    }

        RxOutput output;
        output.logRxRaw = options.logRxRaw;
        output.logRx = options.logRx && !options.tableMode;
        output.table = tablePtr;

    TableTxStateListener txListener(tablePtr);
    if (options.tableMode) {
        client.SetTxStateListener(&txListener);
    }

    StatusPoller poller(tablePtr);
    if (options.tableMode) {
        for (auto& handler : handlers) {
            std::visit([&](auto& topicHandler) {
                poller.AddSource(topicHandler.name,
                                 topicHandler.reader,
                                 &topicHandler.totalValidSamples,
                                 &topicHandler.seenIds);
            }, handler);
        }
    }

        while (!stopToken.StopRequested()) {
        poller.Poll();

            for (auto& handler : handlers) {
                const bool ok = std::visit([&](auto& topicHandler) {
                    return ProcessSamples(topicHandler, state, client, output, options.yokeId);
                }, handler);
                if (!ok) {
                    SetLastError(std::string("Runtime error: ") + client.LastError());
                    std::cerr << LastError() << std::endl;
                    return EXIT_FAILURE;
                }
            }

            SleepWithStop(stopToken, std::chrono::milliseconds(50));
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        SetLastError(std::string("Unhandled exception: ") + ex.what());
        std::cerr << LastError() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        SetLastError("Unhandled unknown exception.");
        std::cerr << LastError() << std::endl;
        return EXIT_FAILURE;
    }
}

const std::string& AppRunner::LastError() const noexcept
{
    return _lastError;
}

void AppRunner::SetLastError(const std::string& error)
{
    _lastError = error;
}
} // namespace app
