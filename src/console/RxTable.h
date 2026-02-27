#pragma once

#include <memory>
#include <string>
#include <vector>

namespace console
{

/// @brief Non-scrolling live console dashboard for DDS receive activity.
///
/// @details `RxTable` uses cursor-positioning escape sequences to update a
/// fixed-height table in place, avoiding the scrolling wall of text produced
/// by plain `std::cout` logging.
///
/// It uses the **pimpl idiom** (`Impl`) to confine Windows console API headers
/// (`<windows.h>`) to the `.cpp` file, keeping this header dependency-free.
///
/// **Lifecycle:**
/// 1. Call `Begin(topics, includeTxLine)` once to reserve rows and position
///    the cursor.
/// 2. Call `Update`, `SetTopicStatus`, and `SetTxStateLine` from the
///    DDS read loop as data arrives.
/// 3. Call `End()` on exit to restore the cursor below the table.
///
/// @note Copy is deleted because the internal cursor state cannot be shared.
///       Move is permitted so the object can be conditionally constructed in
///       `AppRunner`.
class RxTable
{
public:
    RxTable() noexcept;
    ~RxTable() noexcept;

    RxTable(const RxTable&)            = delete;
    RxTable& operator=(const RxTable&) = delete;

    RxTable(RxTable&&) noexcept;
    RxTable& operator=(RxTable&&) noexcept;

    // ── Setup ──────────────────────────────────────────────────────────────

    /// @brief Initialises the table with no topic rows and no TX state line.
    /// @return `true` if the console supports cursor positioning; `false` if
    ///         output is redirected or the terminal does not support ANSI.
    bool Begin() noexcept;

    /// @brief Initialises the table with one row per named DDS topic.
    /// @param[in] topics Ordered list of topic display names.
    /// @return `true` on success.
    bool Begin(const std::vector<std::string>& topics) noexcept;

    /// @brief Initialises the table with topic rows and an optional TX state row.
    /// @param[in] topics         Ordered list of topic display names.
    /// @param[in] includeTxLine  When `true`, reserves a final row for the TX
    ///            state line updated by `SetTxStateLine`.
    /// @return `true` on success.
    bool Begin(const std::vector<std::string>& topics, bool includeTxLine) noexcept;

    /// @brief Cleans up console state and positions the cursor below the table.
    void End() noexcept;

    // ── Live update ────────────────────────────────────────────────────────

    /// @brief Updates the value cell for a topic row.
    /// @param[in] topic  Topic name (must match an entry from `Begin`).
    /// @param[in] id     Source identifier string (e.g. `"30:1004"`).
    /// @param[in] value  Formatted value to display (e.g. `"0.512"`).
    void Update(const std::string& topic,
                const std::string& id,
                const std::string& value) noexcept;

    /// @brief Sets the status indicator cell for a topic row.
    /// @param[in] topic  Topic name (must match an entry from `Begin`).
    /// @param[in] status Formatted status string (e.g. `"12 sps"`).
    void SetTopicStatus(const std::string& topic, const std::string& status) noexcept;

    /// @brief Updates the TX state row with a formatted channel summary.
    /// @param[in] text Pre-formatted state string produced by `AppRunner`'s
    ///            `TableTxStateListener` (e.g. `"axis:left_x=0.500 ..."`).
    void SetTxStateLine(const std::string& text) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace console
