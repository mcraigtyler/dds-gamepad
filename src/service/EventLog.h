#pragma once

#include <Windows.h>

#include <string>

namespace service
{
class EventLog
{
public:
    explicit EventLog(const wchar_t* sourceName) noexcept;
    ~EventLog() noexcept;

    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;

    void Info(const std::wstring& message) noexcept;
    void Warning(const std::wstring& message) noexcept;
    void Error(const std::wstring& message) noexcept;

private:
    void Write(WORD type, const std::wstring& message) noexcept;

private:
    HANDLE _handle;
};
} // namespace service
