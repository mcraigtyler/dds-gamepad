#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "dds_includes.h"

namespace console { class RxTable; }

namespace app {

// Polls DDS reader status metrics every 500 ms and pushes formatted strings
// to an RxTable.  Only active when a non-null table is provided.
class StatusPoller {
public:
    explicit StatusPoller(console::RxTable* table);

    void AddSource(std::string topicName,
                   const dds::sub::AnyDataReader& reader,
                   uint64_t* totalValidSamples,
                   std::unordered_set<std::string>* seenIds);

    // Call once per main-loop iteration.  No-op when no table was provided.
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
