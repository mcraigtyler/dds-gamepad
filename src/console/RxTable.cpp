#include "console/RxTable.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace console
{
namespace {
std::string PadRight(const std::string& text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    std::string out = text;
    out.append(width - text.size(), ' ');
    return out;
}
}

RxTable::RxTable() noexcept
    : _out(INVALID_HANDLE_VALUE),
      _active(false),
      _width(0),
            _nextRow(2),
      _topicWidth(std::string("topic").size()),
            _idWidth(std::string("id").size()),
            _statusRowCount(0),
            _tableHeaderRow(0),
            _tableUnderlineRow(1)
{
    _originalCursorInfo.dwSize = 25;
    _originalCursorInfo.bVisible = TRUE;
}

RxTable::~RxTable() noexcept
{
    End();
}

bool RxTable::Begin() noexcept
{
    const std::vector<std::string> noTopics;
    return Begin(noTopics);
}

bool RxTable::Begin(const std::vector<std::string>& topics) noexcept
{
    _out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (_out == INVALID_HANDLE_VALUE || _out == nullptr) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(_out, &info)) {
        return false;
    }
    _width = info.dwSize.X;

    if (!GetConsoleCursorInfo(_out, &_originalCursorInfo)) {
        _originalCursorInfo.dwSize = 25;
        _originalCursorInfo.bVisible = TRUE;
    }

    CONSOLE_CURSOR_INFO hidden = _originalCursorInfo;
    hidden.bVisible = FALSE;
    SetConsoleCursorInfo(_out, &hidden);

    ClearScreen(info);

    _topicWidth = std::string("topic").size();
    _idWidth = std::string("id").size();
    for (const auto& t : topics) {
        if (t.size() > _topicWidth) {
            _topicWidth = t.size();
        }
    }

    _topicStatusRows.clear();
    _topicStatusText.clear();
    _topicStatusOrder.clear();
    _topicStatusOrder.reserve(topics.size());
    _statusRowCount = static_cast<SHORT>(topics.size());
    _tableHeaderRow = _statusRowCount;
    _tableUnderlineRow = static_cast<SHORT>(_statusRowCount + 1);

    for (SHORT i = 0; i < _statusRowCount; ++i) {
        const auto& topic = topics[static_cast<size_t>(i)];
        _topicStatusRows.emplace(topic, i);
        _topicStatusText.emplace(topic, std::string());
        _topicStatusOrder.push_back(topic);
    }

    _rowsByKey.clear();
    _rowOrder.clear();
    _nextRow = static_cast<SHORT>(_tableUnderlineRow + 1);

    RedrawAll();

    _active = true;
    return true;
}

void RxTable::End() noexcept
{
    if (!_active) {
        return;
    }

    SetConsoleCursorInfo(_out, &_originalCursorInfo);

    const COORD pos{0, static_cast<SHORT>(_nextRow + 1)};
    SetConsoleCursorPosition(_out, pos);

    _active = false;
}

void RxTable::Update(const std::string& topic, const std::string& id, const std::string& value) noexcept
{
    if (!_active) {
        return;
    }

    std::string key;
    key.reserve(topic.size() + 1 + id.size());
    key.append(topic);
    key.push_back('|');
    key.append(id);

    bool widthChanged = false;
    if (topic.size() > _topicWidth) {
        _topicWidth = topic.size();
        widthChanged = true;
    }
    if (id.size() > _idWidth) {
        _idWidth = id.size();
        widthChanged = true;
    }

    auto it = _rowsByKey.find(key);
    if (it == _rowsByKey.end()) {
        RowData data;
        data.topic = topic;
        data.id = id;
        data.value = value;
        data.row = AllocateRow(key);
        _rowOrder.push_back(key);
        it = _rowsByKey.emplace(key, std::move(data)).first;
        // New rows should also pick up the latest column widths.
        widthChanged = true;
    } else {
        it->second.topic = topic;
        it->second.id = id;
        it->second.value = value;
    }

    if (widthChanged) {
        RedrawAll();
        return;
    }

    const std::string line =
        PadRight(it->second.topic, _topicWidth) + " | " +
        PadRight(it->second.id, _idWidth) + " | " +
        it->second.value;
    WriteLineAtRow(it->second.row, line);
}

void RxTable::SetTopicStatus(const std::string& topic, const std::string& status) noexcept
{
    if (!_active) {
        return;
    }

    auto rowIt = _topicStatusRows.find(topic);
    if (rowIt == _topicStatusRows.end()) {
        return;
    }

    bool widthChanged = false;
    if (topic.size() > _topicWidth) {
        _topicWidth = topic.size();
        widthChanged = true;
    }

    _topicStatusText[topic] = status;

    if (widthChanged) {
        RedrawAll();
        return;
    }

    WriteTopicStatusLine(topic);
}

void RxTable::ClearScreen(const CONSOLE_SCREEN_BUFFER_INFO& info) noexcept
{
    const DWORD cellCount = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    const COORD home{0, 0};

    FillConsoleOutputCharacterA(_out, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(_out, info.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(_out, home);
}

void RxTable::EnsureBufferHeight(SHORT minHeight) noexcept
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(_out, &info)) {
        return;
    }

    if (info.dwSize.Y >= minHeight) {
        return;
    }

    COORD newSize = info.dwSize;
    newSize.Y = static_cast<SHORT>(minHeight + 50);
    if (SetConsoleScreenBufferSize(_out, newSize)) {
        _width = newSize.X;
    }
}

SHORT RxTable::AllocateRow(const std::string& key) noexcept
{
    const SHORT row = _nextRow;
    _nextRow = static_cast<SHORT>(_nextRow + 1);

    EnsureBufferHeight(static_cast<SHORT>(_nextRow + 2));
    WriteLineAtRow(row, "");
    return row;
}

void RxTable::WriteLineAtRow(SHORT row, const std::string& line) noexcept
{
    EnsureBufferHeight(static_cast<SHORT>(row + 2));

    std::string padded = line;
    if (_width > 0) {
        const size_t width = static_cast<size_t>(_width);
        if (padded.size() > width) {
            padded.resize(width);
        } else if (padded.size() < width) {
            padded.append(width - padded.size(), ' ');
        }
    }

    const COORD pos{0, row};
    SetConsoleCursorPosition(_out, pos);

    DWORD written = 0;
    WriteConsoleA(_out, padded.data(), static_cast<DWORD>(padded.size()), &written, nullptr);
}

void RxTable::RedrawAll() noexcept
{
    const std::string header =
        PadRight("topic", _topicWidth) + " | " +
        PadRight("id", _idWidth) + " | value";
    const std::string underline =
        std::string(_topicWidth, '-') + " | " +
        std::string(_idWidth, '-') + " | " +
        std::string(5, '-');

    WriteLineAtRow(_tableHeaderRow, header);
    WriteLineAtRow(_tableUnderlineRow, underline);

    for (const auto& key : _rowOrder) {
        const auto it = _rowsByKey.find(key);
        if (it == _rowsByKey.end()) {
            continue;
        }
        const std::string line =
            PadRight(it->second.topic, _topicWidth) + " | " +
            PadRight(it->second.id, _idWidth) + " | " +
            it->second.value;
        WriteLineAtRow(it->second.row, line);
    }
}

void RxTable::WriteTopicStatusLine(const std::string& topic) noexcept
{
    const auto rowIt = _topicStatusRows.find(topic);
    if (rowIt == _topicStatusRows.end()) {
        return;
    }

    const auto textIt = _topicStatusText.find(topic);
    const std::string& status = (textIt != _topicStatusText.end()) ? textIt->second : std::string();

    std::string line = PadRight(topic, _topicWidth) + " | " + status;
    WriteLineAtRow(rowIt->second, line);
}
} // namespace console
