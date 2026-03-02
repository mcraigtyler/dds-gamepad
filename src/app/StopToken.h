#pragma once

#include <atomic>

namespace app
{

/// @brief Lightweight, copyable handle for observing a cooperative stop signal.
///
/// @details A `StopToken` is obtained from a `StopSource` and passed to any
/// component that should exit when a stop is requested (e.g. `AppRunner::Run`,
/// a worker thread). It holds a raw pointer to the source's `atomic_bool`, so
/// it is cheap to copy.
///
/// **Default-constructed** tokens always return `false` from `StopRequested()`
/// — they are permanently in the "never stop" state. This is intentional: it
/// lets components be unit-tested without wiring up a `StopSource`.
///
/// @note This is a simplified analogue of `std::stop_token` (C++20) using the
///       project's C++17 baseline and an explicit atomic pointer.
class StopToken
{
public:
    /// @brief Constructs a token in the "never stop" state.
    StopToken() noexcept
        : _stopRequested(nullptr)
    {
    }

    /// @brief Constructs a token bound to the given stop flag.
    /// @param[in] stopRequested Pointer to the `atomic_bool` owned by a
    ///            `StopSource`. Must remain valid for the lifetime of this token.
    explicit StopToken(const std::atomic_bool* stopRequested) noexcept
        : _stopRequested(stopRequested)
    {
    }

    /// @brief Checks whether a stop has been requested.
    /// @return `true` if the associated `StopSource::RequestStop()` has been
    ///         called; `false` if no stop has been requested or this token was
    ///         default-constructed.
    bool StopRequested() const noexcept
    {
        return _stopRequested != nullptr && _stopRequested->load(std::memory_order_relaxed);
    }

private:
    const std::atomic_bool* _stopRequested;
};

/// @brief Producer of a cooperative stop signal.
///
/// @details `StopSource` owns the `atomic_bool` that backs a set of
/// `StopToken` instances. Call `RequestStop()` from the controlling thread
/// (e.g. the Windows service `OnStop` handler or a Ctrl-C signal handler)
/// to signal all holders of associated tokens.
///
/// **Ownership:** `StopSource` must outlive every `StopToken` obtained from
/// it, because tokens hold a raw pointer to the source's internal flag.
///
/// @section usage Usage
/// @code
/// app::StopSource source;
/// std::thread worker([token = source.Token()]() {
///     while (!token.StopRequested()) {
///         // do work
///     }
/// });
/// source.RequestStop();
/// worker.join();
/// @endcode
class StopSource
{
public:
    /// @brief Constructs a source with the stop flag initialised to `false`.
    StopSource() noexcept
        : _stopRequested(false)
    {
    }

    /// @brief Returns a token bound to this source's stop flag.
    /// @return A `StopToken` that reflects future calls to `RequestStop()`.
    StopToken Token() const noexcept
    {
        return StopToken(&_stopRequested);
    }

    /// @brief Signals all associated tokens to stop.
    ///
    /// @details Sets the internal flag to `true` with relaxed memory order.
    /// Tokens poll the flag on their next `StopRequested()` call. This method
    /// is idempotent — calling it multiple times is safe.
    void RequestStop() noexcept
    {
        _stopRequested.store(true, std::memory_order_relaxed);
    }

private:
    std::atomic_bool _stopRequested;
};

} // namespace app
