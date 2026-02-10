#pragma once

#include <string>

#include "app/StopToken.h"

namespace app
{
struct AppRunnerOptions
{
    std::string configFile;
    int domainId = 0;
    bool logRxRaw = false;
    bool tableMode = false;
    // Console defaults preserve previous behavior.
    // Service mode should typically set these to false.
    bool logStartup = true;
    bool logRx = true;
    bool logTxState = true;
};

class AppRunner
{
public:
    int Run(const AppRunnerOptions& options, const StopToken& stopToken);
    const std::string& LastError() const noexcept;

private:
    void SetLastError(const std::string& error);

private:
    std::string _lastError;
};
} // namespace app
