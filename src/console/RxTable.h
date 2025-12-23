#pragma once

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
    bool Begin(const std::vector<std::string>& topics) noexcept;
    bool Begin(const std::vector<std::string>& topics, bool includeTxLine) noexcept;
    void End() noexcept;

    void Update(const std::string& topic, const std::string& id, const std::string& value) noexcept;
    void SetTopicStatus(const std::string& topic, const std::string& status) noexcept;
    void SetTxStateLine(const std::string& text) noexcept;

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

    void WriteTopicStatusLine(const std::string& topic) noexcept;

private:
    HANDLE _out;
    bool _active;
    SHORT _width;
    SHORT _nextRow;
    CONSOLE_CURSOR_INFO _originalCursorInfo;

    SHORT _statusRowCount;
    bool _hasTxLine;
    SHORT _txRow;
    std::string _txText;
    SHORT _tableHeaderRow;
    SHORT _tableUnderlineRow;

    size_t _topicWidth;
    size_t _idWidth;

    std::unordered_map<std::string, SHORT> _topicStatusRows;
    std::unordered_map<std::string, std::string> _topicStatusText;
    std::vector<std::string> _topicStatusOrder;

    std::unordered_map<std::string, RowData> _rowsByKey;
    std::vector<std::string> _rowOrder;
};
} // namespace console
