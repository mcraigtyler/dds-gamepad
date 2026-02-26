#include "app/StatusPoller.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "console/RxTable.h"

namespace app {
namespace {

std::string FormatReaderStatus(dds::sub::AnyDataReader& reader,
                               double rxRatePerSecondPerIdAvg,
                               size_t uniqueIdCount)
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

} // namespace

StatusPoller::StatusSource::StatusSource(std::string topicName,
                                         const dds::sub::AnyDataReader& anyReader,
                                         uint64_t* totalSamples,
                                         std::unordered_set<std::string>* ids)
    : topic(std::move(topicName)),
      reader(anyReader),
      totalValidSamples(totalSamples),
      seenIds(ids)
{
}

StatusPoller::StatusPoller(console::RxTable* table)
    : table_(table),
      lastUpdate_(std::chrono::steady_clock::now() - std::chrono::seconds(10))
{
}

void StatusPoller::AddSource(std::string topicName,
                             const dds::sub::AnyDataReader& reader,
                             uint64_t* totalValidSamples,
                             std::unordered_set<std::string>* seenIds)
{
    sources_.emplace_back(std::move(topicName), reader, totalValidSamples, seenIds);
}

void StatusPoller::Poll()
{
    if (table_ == nullptr) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - lastUpdate_ < std::chrono::milliseconds(500)) {
        return;
    }

    const double dtSeconds = std::chrono::duration<double>(now - lastUpdate_).count();
    for (auto& src : sources_) {
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

            table_->SetTopicStatus(src.topic, FormatReaderStatus(src.reader, rateAvgPerId, uniqueIds));
        } catch (const std::exception& ex) {
            table_->SetTopicStatus(src.topic, std::string("status_error=") + ex.what());
        } catch (...) {
            table_->SetTopicStatus(src.topic, "status_error=unknown");
        }
    }
    lastUpdate_ = now;
}

} // namespace app
