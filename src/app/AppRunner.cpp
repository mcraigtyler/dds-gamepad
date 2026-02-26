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

#include "console/RxTable.h"
#include "config/ConfigLoader.h"
#include "dds_includes.h"
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

struct AnalogHandler {
    std::string name;
    dds::topic::Topic<Gamepad::Gamepad_Analog> topic;
    dds::sub::DataReader<Gamepad::Gamepad_Analog> reader;
    mapper::MappingEngine mappingEngine;
    uint64_t totalValidSamples = 0;
    std::unordered_set<std::string> seenIds;

    AnalogHandler(dds::domain::DomainParticipant& participant,
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

struct StickHandler {
    std::string name;
    dds::topic::Topic<Gamepad::Stick_TwoAxis> topic;
    dds::sub::DataReader<Gamepad::Stick_TwoAxis> reader;
    mapper::MappingEngine mappingEngine;
    uint64_t totalValidSamples = 0;
    std::unordered_set<std::string> seenIds;

    StickHandler(dds::domain::DomainParticipant& participant,
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


struct ButtonHandler {
    std::string name;
    dds::topic::Topic<Gamepad::Button> topic;
    dds::sub::DataReader<Gamepad::Button> reader;
    mapper::MappingEngine mappingEngine;
    uint64_t totalValidSamples = 0;
    std::unordered_set<std::string> seenIds;

    ButtonHandler(dds::domain::DomainParticipant& participant,
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

bool ProcessAnalogSamples(AnalogHandler& handler,
                          mapper::GamepadState& state,
                          emulator::VigemClient& client,
                          const RxOutput& output,
                          int yokeId)
{
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
        const float rawValue = static_cast<float>(data.value());

        if (output.table != nullptr) {
            std::ostringstream value;
            value << std::fixed << std::setprecision(3) << rawValue;
            output.table->Update(handler.name, busId, value.str());
        } else if (output.logRxRaw) {
            std::cout << "rx_raw topic=" << handler.name
                      << " id=" << busId
                      << " value=" << rawValue << std::endl;
        }

        if (!handler.mappingEngine.Apply("value", messageId, rawValue, state)) {
            continue;
        }

        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }

        if (output.table == nullptr && output.logRx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << busId
                      << " value=" << rawValue
                      << std::endl;
        }
    }
    return true;
}

bool ProcessStickSamples(StickHandler& handler,
                         mapper::GamepadState& state,
                         emulator::VigemClient& client,
                         const RxOutput& output,
                         int yokeId)
{
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
        const float rawX = static_cast<float>(data.x());
        const float rawY = static_cast<float>(data.y());

        if (output.table != nullptr) {
            std::ostringstream value;
            value << "x=" << std::fixed << std::setprecision(3) << rawX
                  << " y=" << std::fixed << std::setprecision(3) << rawY;
            output.table->Update(handler.name, busId, value.str());
        } else if (output.logRxRaw) {
            std::cout << "rx_raw topic=" << handler.name
                      << " id=" << busId
                      << " x=" << rawX
                      << " y=" << rawY << std::endl;
        }

        bool updated = handler.mappingEngine.Apply("x", messageId, rawX, state);
        updated = handler.mappingEngine.Apply("y", messageId, rawY, state) || updated;
        if (!updated) {
            continue;
        }

        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }

        if (output.table == nullptr && output.logRx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << busId
                      << " x=" << rawX
                      << " y=" << rawY
                      << std::endl;
        }
    }
    return true;
}


bool ProcessButtonSamples(ButtonHandler& handler,
                          mapper::GamepadState& state,
                          emulator::VigemClient& client,
                          const RxOutput& output,
                          int yokeId)
{
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

        bool btnChanging = false;
        if (!TryReadBool(data.btnChanging(), btnChanging) || !btnChanging) {
            continue;
        }

        Common::ButtonState_t btnState = Common::ButtonState_t::Invalid;
        if (!TryReadButtonState(data.btnState(), btnState)) {
            continue;
        }

        float mappedValue = 0.0f;
        if (btnState == Common::ButtonState_t::Down) {
            mappedValue = 1.0f;
        } else if (btnState == Common::ButtonState_t::Up) {
            mappedValue = 0.0f;
        } else {
            continue;
        }

        const int messageId = ResolveRoleFromData(data);

        if (output.table != nullptr) {
            output.table->Update(handler.name,
                                 busId,
                                 btnState == Common::ButtonState_t::Down ? "down" : "up");
        } else if (output.logRxRaw) {
            std::cout << "rx_raw topic=" << handler.name
                      << " id=" << busId
                      << " state=" << (btnState == Common::ButtonState_t::Down ? "down" : "up")
                      << " changing=true" << std::endl;
        }

        if (!handler.mappingEngine.Apply("btnState", messageId, mappedValue, state)) {
            continue;
        }

        if (!client.UpdateState(state)) {
            std::cerr << "Failed to update controller state: " << client.LastError() << std::endl;
            return false;
        }

        if (output.table == nullptr && output.logRx) {
            std::cout << "rx topic=" << handler.name
                      << " id=" << busId
                      << " state=" << (btnState == Common::ButtonState_t::Down ? "down" : "up")
                      << std::endl;
        }
    }

    return true;
}

std::string FormatReaderStatus(dds::sub::AnyDataReader& reader, double rxRatePerSecondPerIdAvg, size_t uniqueIdCount)
{
    const auto matched = reader.subscription_matched_status();
    const auto liveliness = reader.liveliness_changed_status();
    const auto qos = reader.requested_incompatible_qos_status();
    const auto deadline = reader.requested_deadline_missed_status();
    const auto lost = reader.sample_lost_status();
    const auto rejected = reader.sample_rejected_status();

    std::ostringstream out;
    out << std::fixed << std::setprecision(1)
        << "rate=" << rxRatePerSecondPerIdAvg << "/s"
        << " ids=" << uniqueIdCount
        << " writers=" << matched.current_count()
        << " alive=" << liveliness.alive_count()
        << " notAlive=" << liveliness.not_alive_count()
        << " qosIncompat=" << qos.total_count()
        << " deadlineMiss=" << deadline.total_count()
        << " lost=" << lost.total_count()
        << " rejected=" << rejected.total_count();
    return out.str();
}

void SleepWithStop(const StopToken& stopToken, std::chrono::milliseconds duration)
{
    const auto end = std::chrono::steady_clock::now() + duration;
    while (!stopToken.StopRequested() && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
} // namespace

int AppRunner::Run(const AppRunnerOptions& options, const StopToken& stopToken)
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

        emulator::VigemClient client;
        if (!client.Connect()) {
            SetLastError(std::string("Failed to connect to ViGEm: ") + client.LastError());
            std::cerr << LastError() << std::endl;
            return EXIT_FAILURE;
        }
        if (!client.AddX360Controller()) {
            SetLastError(std::string("Failed to add Xbox 360 controller: ") + client.LastError());
            std::cerr << LastError() << std::endl;
            return EXIT_FAILURE;
        }

        // Console mode keeps previous behavior: log tx state unless table mode.
        // Service mode should set logTxState=false to avoid per-sample output.
        client.SetLogState(options.logTxState && !options.tableMode);

        dds::domain::DomainParticipant participant(options.domainId);
        dds::sub::Subscriber subscriber(participant);

        using TopicHandler = std::variant<AnalogHandler, StickHandler, ButtonHandler>;
        std::vector<TopicHandler> handlers;
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
                    handlers.emplace_back(AnalogHandler(participant,
                                                        subscriber,
                                                        config.dds.topic,
                                                        mapper::MappingEngine(config.mappings)));
                    break;
                case common::TopicType::StickTwoAxis:
                    handlers.emplace_back(StickHandler(participant,
                                                       subscriber,
                                                       config.dds.topic,
                                                       mapper::MappingEngine(config.mappings)));
                    break;
                case common::TopicType::GamepadButton:
                    handlers.emplace_back(ButtonHandler(participant,
                                                        subscriber,
                                                        config.dds.topic,
                                                        mapper::MappingEngine(config.mappings)));
                    break;
            }
        }

    mapper::GamepadState state;

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

    class TableTxStateListener final : public emulator::ITxStateListener
    {
    public:
        explicit TableTxStateListener(console::RxTable* tablePtr)
            : _table(tablePtr)
        {
        }

        void OnTxState(const mapper::GamepadState& txState) override
        {
            if (_table == nullptr) {
                return;
            }

            std::ostringstream out;
            out << "state"
                << " LT=" << static_cast<int>(txState.left_trigger)
                << " RT=" << static_cast<int>(txState.right_trigger)
                << " LX=" << txState.left_stick_x
                << " LY=" << txState.left_stick_y
                << " RX=" << txState.right_stick_x
                << " RY=" << txState.right_stick_y
                << " Btn=0x" << std::hex << txState.buttons << std::dec;
            _table->SetTxStateLine(out.str());
        }

    private:
        console::RxTable* _table;
    };

    TableTxStateListener txListener(tablePtr);
    if (options.tableMode) {
        client.SetTxStateListener(&txListener);
    }

    struct StatusSource {
        std::string topic;
        dds::sub::AnyDataReader reader;
        uint64_t* totalValidSamples = nullptr;
        std::unordered_set<std::string>* seenIds = nullptr;
        uint64_t lastTotalValidSamples = 0;
        bool hasLastTotal = false;

        StatusSource(std::string topicName,
                     const dds::sub::AnyDataReader& anyReader,
                     uint64_t* totalSamples,
                     std::unordered_set<std::string>* ids)
            : topic(std::move(topicName)),
              reader(anyReader),
              totalValidSamples(totalSamples),
              seenIds(ids)
        {
        }
    };

    std::vector<StatusSource> statusSources;
    if (options.tableMode) {
        statusSources.reserve(handlers.size());
        for (auto& handler : handlers) {
            std::visit([&](auto& topicHandler) {
                statusSources.emplace_back(topicHandler.name,
                                           topicHandler.reader,
                                           &topicHandler.totalValidSamples,
                                           &topicHandler.seenIds);
            }, handler);
        }
    }

    auto lastStatusUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(10);

        while (!stopToken.StopRequested()) {
        if (tablePtr != nullptr) {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastStatusUpdate >= std::chrono::milliseconds(500)) {
                const double dtSeconds = std::chrono::duration<double>(now - lastStatusUpdate).count();
                for (auto& src : statusSources) {
                    try {
                        double rateTotal = 0.0;
                        if (src.totalValidSamples != nullptr && dtSeconds > 0.0) {
                            const uint64_t total = *src.totalValidSamples;
                            if (src.hasLastTotal) {
                                const uint64_t delta = total - src.lastTotalValidSamples;
                                rateTotal = static_cast<double>(delta) / dtSeconds;
                            }
                            src.lastTotalValidSamples = total;
                            src.hasLastTotal = true;
                        }

                        size_t uniqueIds = 0;
                        if (src.seenIds != nullptr) {
                            uniqueIds = src.seenIds->size();
                        }
                        const size_t divisor = (uniqueIds > 0) ? uniqueIds : 1;
                        const double rateAvgPerId = rateTotal / static_cast<double>(divisor);

                        tablePtr->SetTopicStatus(src.topic, FormatReaderStatus(src.reader, rateAvgPerId, uniqueIds));
                    } catch (const std::exception& ex) {
                        tablePtr->SetTopicStatus(src.topic, std::string("status_error=") + ex.what());
                    } catch (...) {
                        tablePtr->SetTopicStatus(src.topic, "status_error=unknown");
                    }
                }
                lastStatusUpdate = now;
            }
        }

            for (auto& handler : handlers) {
            struct HandlerVisitor {
                mapper::GamepadState& state;
                emulator::VigemClient& client;
                const RxOutput& output;
                int yokeId;

                bool operator()(AnalogHandler& topicHandler) const
                {
                    return ProcessAnalogSamples(topicHandler, state, client, output, yokeId);
                }

                bool operator()(StickHandler& topicHandler) const
                {
                    return ProcessStickSamples(topicHandler, state, client, output, yokeId);
                }

                bool operator()(ButtonHandler& topicHandler) const
                {
                    return ProcessButtonSamples(topicHandler, state, client, output, yokeId);
                }
            };

                const bool ok = std::visit(HandlerVisitor{state, client, output, options.yokeId}, handler);
                if (!ok) {
                    SetLastError(std::string("Runtime error: ") + client.LastError());
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
