#pragma once

#ifdef _WIN32

#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace console
{
class RxTable
{
public:
    RxTable() noexcept;
    ~RxTable() noexcept;

    RxTable(const RxTable&) = delete;
    RxTable& operator=(const RxTable&) = delete;

    bool Begin() noexcept;
    void End() noexcept;

    void Update(const std::string& topic, const std::string& id, const std::string& value) noexcept;

private:
    struct RowData
    {
        std::string topic;
        std::string id;
        std::string value;
        SHORT row = 0;
    };

    void ClearScreen(const CONSOLE_SCREEN_BUFFER_INFO& info) noexcept;
    void EnsureBufferHeight(SHORT minHeight) noexcept;
    SHORT AllocateRow(const std::string& key) noexcept;
    void WriteLineAtRow(SHORT row, const std::string& line) noexcept;
    void RedrawAll() noexcept;

private:
    HANDLE _out;
    bool _active;
    SHORT _width;
    SHORT _nextRow;
    CONSOLE_CURSOR_INFO _originalCursorInfo;

    size_t _topicWidth;
    size_t _idWidth;

    std::unordered_map<std::string, RowData> _rowsByKey;
    std::vector<std::string> _rowOrder;
};
} // namespace console

#endif // _WIN32
