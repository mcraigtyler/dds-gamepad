#pragma once

#include <string>

#include "app/StopToken.h"

namespace app
{
struct AppRunnerOptions
{
    std::string configDir;
    int domainId = 0;
    bool logRxRaw = false;
    bool tableMode = false;
};

class AppRunner
{
public:
    int Run(const AppRunnerOptions& options, const StopToken& stopToken);
};
} // namespace app
