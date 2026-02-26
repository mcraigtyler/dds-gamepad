#pragma once

#include <string>

#include "app/StopToken.h"

namespace emulator { class IOutputDevice; }

namespace app
{
struct AppRunnerOptions
{
    std::string configFile;
    int domainId = 0;
    int yokeId = 0;
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
    // Convenience overload: constructs a VigemClient internally.
    int Run(const AppRunnerOptions& options, const StopToken& stopToken);
    // Core overload: caller supplies the output device (enables testing).
    int Run(const AppRunnerOptions& options,
            emulator::IOutputDevice& client,
            const StopToken& stopToken);
    const std::string& LastError() const noexcept;

private:
    void SetLastError(const std::string& error);

private:
    std::string _lastError;
};
} // namespace app
