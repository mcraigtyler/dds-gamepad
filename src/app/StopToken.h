#pragma once

#include <atomic>

namespace app
{
class StopToken
{
public:
    StopToken() noexcept
        : _stopRequested(nullptr)
    {
    }

    explicit StopToken(const std::atomic_bool* stopRequested) noexcept
        : _stopRequested(stopRequested)
    {
    }

    bool StopRequested() const noexcept
    {
        return _stopRequested != nullptr && _stopRequested->load(std::memory_order_relaxed);
    }

private:
    const std::atomic_bool* _stopRequested;
};

class StopSource
{
public:
    StopSource() noexcept
        : _stopRequested(false)
    {
    }

    StopToken Token() const noexcept
    {
        return StopToken(&_stopRequested);
    }

    void RequestStop() noexcept
    {
        _stopRequested.store(true, std::memory_order_relaxed);
    }

private:
    std::atomic_bool _stopRequested;
};
} // namespace app
