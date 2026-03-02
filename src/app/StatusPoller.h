#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "dds_includes.h"

namespace console { class RxTable; }

namespace app {

/// @brief Display-only component that renders DDS health metrics to the live
///        console dashboard. Has no effect on the data pipeline.
///
/// @details `StatusPoller` is **purely a display component**. It never reads
/// DDS samples, never writes to `OutputState`, and never calls
/// `IOutputDevice::UpdateState`. It has no bearing on gamepad output.
///
/// It is only active when `AppRunner` is running in `tableMode` (the
/// `--table` CLI flag). When constructed with `nullptr`, every method is a
/// no-op and the poller has zero runtime cost.
///
/// When active, `Poll()` is called once per read-loop iteration (every 50 ms).
/// It checks whether 500 ms have elapsed and, if so, queries two sources of
/// information for each registered topic:
/// - The `totalValidSamples` counter (written by `ProcessSamples` in
///   `AppRunner`; read here to compute a per-second rate).
/// - DDS QoS status APIs on the `AnyDataReader` —
///   `subscription_matched_status`, `liveliness_changed_status`,
///   `sample_lost_status`, etc. These are **read-only status queries** that
///   do not consume any samples from the DDS queue.
///
/// All computed metrics are forwarded to `RxTable::SetTopicStatus` for display.
class StatusPoller {
public:
    /// @brief Constructs the poller.
    /// @param[in] table Dashboard to update. Pass `nullptr` to disable all
    ///            polling (no-op mode).
    explicit StatusPoller(console::RxTable* table);

    /// @brief Registers a DDS reader and its associated counters for polling.
    ///
    /// @details Called once per topic subscription during `AppRunner` setup.
    /// The poller does not take ownership of any of the pointed-to objects —
    /// all must remain valid for the lifetime of the `StatusPoller`.
    ///
    /// @param[in]     topicName          Display name used to identify the
    ///                table row (must match an entry from `RxTable::Begin`).
    /// @param[in]     reader             DDS reader whose QoS status APIs are
    ///                queried for display metrics (matched writers, liveliness,
    ///                lost/rejected samples). Never used to read data.
    /// @param[in,out] totalValidSamples  Pointer to the per-topic sample counter
    ///                maintained by `TopicHandler` in `AppRunner`. The poller
    ///                reads this on each poll to compute the delta.
    /// @param[in,out] seenIds            Pointer to the set of source IDs seen
    ///                on this topic. Displayed in the status cell.
    void AddSource(std::string topicName,
                   const dds::sub::AnyDataReader& reader,
                   uint64_t* totalValidSamples,
                   std::unordered_set<std::string>* seenIds);

    /// @brief Samples all registered readers and updates the dashboard.
    ///
    /// @details Computes the elapsed time since the last update. If 500 ms or
    /// more have passed, it calculates the samples-per-second rate for each
    /// source and calls `RxTable::SetTopicStatus` with the result.
    ///
    /// @note This is a no-op when the table pointer is `nullptr`.
    void Poll();

private:
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
                     std::unordered_set<std::string>* ids);
    };

    console::RxTable* table_;
    std::vector<StatusSource> sources_;
    std::chrono::steady_clock::time_point lastUpdate_;
};

} // namespace app
