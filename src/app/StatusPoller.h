#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "dds_includes.h"

namespace console { class RxTable; }

namespace app {

/// @brief Polls DDS reader status metrics every 500 ms and forwards formatted
///        strings to an `RxTable` dashboard row.
///
/// @details `StatusPoller` is driven by `AppRunner`'s main read loop via a
/// single `Poll()` call per iteration. It accumulates sample-count deltas
/// across sources registered with `AddSource` and computes a samples-per-second
/// figure for each topic.
///
/// When constructed with `nullptr` as the table pointer, all methods are
/// no-ops. This allows `AppRunner` to create a `StatusPoller` unconditionally
/// and only activate it when `tableMode` is enabled.
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
    /// @param[in]     reader             DDS reader whose status conditions are
    ///                queried each poll interval.
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
