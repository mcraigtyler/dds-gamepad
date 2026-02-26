#pragma once

#include <memory>
#include <string>
#include <vector>

namespace console
{
class RxTable
{
public:
    RxTable() noexcept;
    ~RxTable() noexcept;

    RxTable(const RxTable&) = delete;
    RxTable& operator=(const RxTable&) = delete;

    RxTable(RxTable&&) noexcept;
    RxTable& operator=(RxTable&&) noexcept;

    bool Begin() noexcept;
    bool Begin(const std::vector<std::string>& topics) noexcept;
    bool Begin(const std::vector<std::string>& topics, bool includeTxLine) noexcept;
    void End() noexcept;

    void Update(const std::string& topic, const std::string& id, const std::string& value) noexcept;
    void SetTopicStatus(const std::string& topic, const std::string& status) noexcept;
    void SetTxStateLine(const std::string& text) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace console
