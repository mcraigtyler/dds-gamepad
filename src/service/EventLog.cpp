#include "service/EventLog.h"

namespace service
{
EventLog::EventLog(const wchar_t* sourceName) noexcept
    : _handle(RegisterEventSourceW(nullptr, sourceName))
{
}

EventLog::~EventLog() noexcept
{
    if (_handle != nullptr) {
        DeregisterEventSource(_handle);
        _handle = nullptr;
    }
}

void EventLog::Info(const std::wstring& message) noexcept
{
    Write(EVENTLOG_INFORMATION_TYPE, message);
}

void EventLog::Warning(const std::wstring& message) noexcept
{
    Write(EVENTLOG_WARNING_TYPE, message);
}

void EventLog::Error(const std::wstring& message) noexcept
{
    Write(EVENTLOG_ERROR_TYPE, message);
}

void EventLog::Write(WORD type, const std::wstring& message) noexcept
{
    if (_handle == nullptr) {
        return;
    }

    const wchar_t* strings[1] = {message.c_str()};
    ReportEventW(
        _handle,
        type,
        0,
        0,
        nullptr,
        1,
        0,
        strings,
        nullptr);
}
} // namespace service
