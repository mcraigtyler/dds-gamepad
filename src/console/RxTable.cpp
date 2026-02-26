#include "console/RxTable.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

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
} // namespace

struct RxTable::Impl
{
    struct RowData
    {
        std::string topic;
        std::string id;
        std::string value;
        SHORT row = 0;
    };

    HANDLE out = INVALID_HANDLE_VALUE;
    bool active = false;
    SHORT width = 0;
    SHORT nextRow = 2;
    CONSOLE_CURSOR_INFO originalCursorInfo{};

    SHORT statusRowCount = 0;
    bool hasTxLine = false;
    SHORT txRow = 0;
    std::string txText;
    SHORT tableHeaderRow = 0;
    SHORT tableUnderlineRow = 1;

    size_t topicWidth = std::string("topic").size();
    size_t idWidth = std::string("id").size();

    std::unordered_map<std::string, SHORT> topicStatusRows;
    std::unordered_map<std::string, std::string> topicStatusText;
    std::vector<std::string> topicStatusOrder;

    std::unordered_map<std::string, RowData> rowsByKey;
    std::vector<std::string> rowOrder;

    Impl() noexcept {
        originalCursorInfo.dwSize = 25;
        originalCursorInfo.bVisible = TRUE;
    }

    void ClearScreen(const CONSOLE_SCREEN_BUFFER_INFO& info) noexcept;
    void EnsureBufferHeight(SHORT minHeight) noexcept;
    SHORT AllocateRow(const std::string& key) noexcept;
    void WriteLineAtRow(SHORT row, const std::string& line) noexcept;
    void RedrawAll() noexcept;
    void WriteTopicStatusLine(const std::string& topic) noexcept;
};

void RxTable::Impl::ClearScreen(const CONSOLE_SCREEN_BUFFER_INFO& info) noexcept
{
    const DWORD cellCount = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    const COORD home{0, 0};

    FillConsoleOutputCharacterA(out, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(out, info.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(out, home);
}

void RxTable::Impl::EnsureBufferHeight(SHORT minHeight) noexcept
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(out, &info)) {
        return;
    }

    if (info.dwSize.Y >= minHeight) {
        return;
    }

    COORD newSize = info.dwSize;
    newSize.Y = static_cast<SHORT>(minHeight + 50);
    if (SetConsoleScreenBufferSize(out, newSize)) {
        width = newSize.X;
    }
}

SHORT RxTable::Impl::AllocateRow(const std::string& key) noexcept
{
    const SHORT row = nextRow;
    nextRow = static_cast<SHORT>(nextRow + 1);

    EnsureBufferHeight(static_cast<SHORT>(nextRow + 2));
    WriteLineAtRow(row, "");
    return row;
}

void RxTable::Impl::WriteLineAtRow(SHORT row, const std::string& line) noexcept
{
    EnsureBufferHeight(static_cast<SHORT>(row + 2));

    std::string padded = line;
    if (width > 0) {
        const size_t w = static_cast<size_t>(width);
        if (padded.size() > w) {
            padded.resize(w);
        } else if (padded.size() < w) {
            padded.append(w - padded.size(), ' ');
        }
    }

    const COORD pos{0, row};
    SetConsoleCursorPosition(out, pos);

    DWORD written = 0;
    WriteConsoleA(out, padded.data(), static_cast<DWORD>(padded.size()), &written, nullptr);
}

void RxTable::Impl::RedrawAll() noexcept
{
    if (hasTxLine) {
        const std::string line = PadRight("tx", topicWidth) + " | " + txText;
        WriteLineAtRow(txRow, line);
    }

    const std::string header =
        PadRight("topic", topicWidth) + " | " +
        PadRight("id", idWidth) + " | value";
    const std::string underline =
        std::string(topicWidth, '-') + " | " +
        std::string(idWidth, '-') + " | " +
        std::string(5, '-');

    WriteLineAtRow(tableHeaderRow, header);
    WriteLineAtRow(tableUnderlineRow, underline);

    for (const auto& key : rowOrder) {
        const auto it = rowsByKey.find(key);
        if (it == rowsByKey.end()) {
            continue;
        }
        const std::string line =
            PadRight(it->second.topic, topicWidth) + " | " +
            PadRight(it->second.id, idWidth) + " | " +
            it->second.value;
        WriteLineAtRow(it->second.row, line);
    }
}

void RxTable::Impl::WriteTopicStatusLine(const std::string& topic) noexcept
{
    const auto rowIt = topicStatusRows.find(topic);
    if (rowIt == topicStatusRows.end()) {
        return;
    }

    const auto textIt = topicStatusText.find(topic);
    const std::string& status = (textIt != topicStatusText.end()) ? textIt->second : std::string();

    std::string line = PadRight(topic, topicWidth) + " | " + status;
    WriteLineAtRow(rowIt->second, line);
}

// ---------------------------------------------------------------------------
// RxTable public interface — delegates to Impl
// ---------------------------------------------------------------------------

RxTable::RxTable() noexcept
    : impl_(std::make_unique<Impl>())
{
}

RxTable::~RxTable() noexcept
{
    End();
}

RxTable::RxTable(RxTable&&) noexcept = default;
RxTable& RxTable::operator=(RxTable&&) noexcept = default;

bool RxTable::Begin() noexcept
{
    const std::vector<std::string> noTopics;
    return Begin(noTopics);
}

bool RxTable::Begin(const std::vector<std::string>& topics) noexcept
{
    return Begin(topics, false);
}

bool RxTable::Begin(const std::vector<std::string>& topics, bool includeTxLine) noexcept
{
    impl_->out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (impl_->out == INVALID_HANDLE_VALUE || impl_->out == nullptr) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(impl_->out, &info)) {
        return false;
    }
    impl_->width = info.dwSize.X;

    if (!GetConsoleCursorInfo(impl_->out, &impl_->originalCursorInfo)) {
        impl_->originalCursorInfo.dwSize = 25;
        impl_->originalCursorInfo.bVisible = TRUE;
    }

    CONSOLE_CURSOR_INFO hidden = impl_->originalCursorInfo;
    hidden.bVisible = FALSE;
    SetConsoleCursorInfo(impl_->out, &hidden);

    impl_->ClearScreen(info);

    impl_->topicWidth = std::string("topic").size();
    impl_->idWidth = std::string("id").size();
    for (const auto& t : topics) {
        if (t.size() > impl_->topicWidth) {
            impl_->topicWidth = t.size();
        }
    }

    impl_->topicStatusRows.clear();
    impl_->topicStatusText.clear();
    impl_->topicStatusOrder.clear();
    impl_->topicStatusOrder.reserve(topics.size());
    impl_->statusRowCount = static_cast<SHORT>(topics.size());
    impl_->hasTxLine = includeTxLine;
    impl_->txText.clear();
    if (impl_->hasTxLine) {
        impl_->txRow = impl_->statusRowCount;
        impl_->tableHeaderRow = static_cast<SHORT>(impl_->statusRowCount + 1);
        impl_->tableUnderlineRow = static_cast<SHORT>(impl_->statusRowCount + 2);
    } else {
        impl_->txRow = 0;
        impl_->tableHeaderRow = impl_->statusRowCount;
        impl_->tableUnderlineRow = static_cast<SHORT>(impl_->statusRowCount + 1);
    }

    for (SHORT i = 0; i < impl_->statusRowCount; ++i) {
        const auto& topic = topics[static_cast<size_t>(i)];
        impl_->topicStatusRows.emplace(topic, i);
        impl_->topicStatusText.emplace(topic, std::string());
        impl_->topicStatusOrder.push_back(topic);
    }

    impl_->rowsByKey.clear();
    impl_->rowOrder.clear();
    impl_->nextRow = static_cast<SHORT>(impl_->tableUnderlineRow + 1);

    impl_->RedrawAll();

    impl_->active = true;
    return true;
}

void RxTable::End() noexcept
{
    if (!impl_ || !impl_->active) {
        return;
    }

    SetConsoleCursorInfo(impl_->out, &impl_->originalCursorInfo);

    const COORD pos{0, static_cast<SHORT>(impl_->nextRow + 1)};
    SetConsoleCursorPosition(impl_->out, pos);

    impl_->active = false;
}

void RxTable::Update(const std::string& topic, const std::string& id, const std::string& value) noexcept
{
    if (!impl_ || !impl_->active) {
        return;
    }

    std::string key;
    key.reserve(topic.size() + 1 + id.size());
    key.append(topic);
    key.push_back('|');
    key.append(id);

    bool widthChanged = false;
    if (topic.size() > impl_->topicWidth) {
        impl_->topicWidth = topic.size();
        widthChanged = true;
    }
    if (id.size() > impl_->idWidth) {
        impl_->idWidth = id.size();
        widthChanged = true;
    }

    auto it = impl_->rowsByKey.find(key);
    if (it == impl_->rowsByKey.end()) {
        Impl::RowData data;
        data.topic = topic;
        data.id = id;
        data.value = value;
        data.row = impl_->AllocateRow(key);
        impl_->rowOrder.push_back(key);
        it = impl_->rowsByKey.emplace(key, std::move(data)).first;
        // New rows should also pick up the latest column widths.
        widthChanged = true;
    } else {
        it->second.topic = topic;
        it->second.id = id;
        it->second.value = value;
    }

    if (widthChanged) {
        impl_->RedrawAll();
        return;
    }

    const std::string line =
        PadRight(it->second.topic, impl_->topicWidth) + " | " +
        PadRight(it->second.id, impl_->idWidth) + " | " +
        it->second.value;
    impl_->WriteLineAtRow(it->second.row, line);
}

void RxTable::SetTopicStatus(const std::string& topic, const std::string& status) noexcept
{
    if (!impl_ || !impl_->active) {
        return;
    }

    auto rowIt = impl_->topicStatusRows.find(topic);
    if (rowIt == impl_->topicStatusRows.end()) {
        return;
    }

    bool widthChanged = false;
    if (topic.size() > impl_->topicWidth) {
        impl_->topicWidth = topic.size();
        widthChanged = true;
    }

    impl_->topicStatusText[topic] = status;

    if (widthChanged) {
        impl_->RedrawAll();
        return;
    }

    impl_->WriteTopicStatusLine(topic);
}

void RxTable::SetTxStateLine(const std::string& text) noexcept
{
    if (!impl_ || !impl_->active || !impl_->hasTxLine) {
        return;
    }

    impl_->txText = text;
    const std::string line = PadRight("tx", impl_->topicWidth) + " | " + impl_->txText;
    impl_->WriteLineAtRow(impl_->txRow, line);
}

} // namespace console
