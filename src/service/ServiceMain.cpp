#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "app/AppRunner.h"
#include "service/EventLog.h"

namespace
{
constexpr const wchar_t* SERVICE_NAME = L"dds-gamepad-service";

SERVICE_STATUS_HANDLE gStatusHandle = nullptr;
SERVICE_STATUS gStatus{};

std::thread gWorker;
std::optional<int> gWorkerExitCode;

app::StopSource gStopSource;

std::unique_ptr<service::EventLog> gEventLog;

std::wstring ToWString(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }
    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return std::wstring();
    }
    std::wstring out;
    out.resize(static_cast<size_t>(required - 1));
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), required);
    return out;
}

struct WorkerContext
{
    app::AppRunner* runner = nullptr;
    app::AppRunnerOptions options;
    app::StopToken stopToken;
};

void WorkerEntry(WorkerContext* ctx)
{
    if (ctx == nullptr || ctx->runner == nullptr) {
        gWorkerExitCode = EXIT_FAILURE;
        return;
    }

    gWorkerExitCode = ctx->runner->Run(ctx->options, ctx->stopToken);
}

void SetServiceState(DWORD currentState,
                     DWORD win32ExitCode = NO_ERROR,
                     DWORD serviceSpecificExitCode = 0,
                     DWORD waitHintMs = 0)
{
    gStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gStatus.dwCurrentState = currentState;

    gStatus.dwControlsAccepted = 0;
    if (currentState == SERVICE_RUNNING) {
        gStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (serviceSpecificExitCode != 0) {
        gStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        gStatus.dwServiceSpecificExitCode = serviceSpecificExitCode;
    } else {
        gStatus.dwWin32ExitCode = win32ExitCode;
        gStatus.dwServiceSpecificExitCode = 0;
    }

    gStatus.dwWaitHint = waitHintMs;

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        gStatus.dwCheckPoint = 0;
    } else {
        gStatus.dwCheckPoint++;
    }

    if (gStatusHandle != nullptr) {
        SetServiceStatus(gStatusHandle, &gStatus);
    }
}

std::optional<int> ParseDomainIdFromArgs(int argc, wchar_t** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        // --domain-id 0
        if (arg == L"--domain-id" || arg == L"--domainid") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            try {
                return std::stoi(argv[i + 1]);
            } catch (...) {
                return std::nullopt;
            }
        }

        // --domain-id=0
        constexpr std::wstring_view prefix = L"--domain-id=";
        if (arg.rfind(prefix.data(), 0) == 0) {
            try {
                return std::stoi(arg.substr(prefix.size()));
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}


std::optional<std::string> ParseConfigFileFromArgs(int argc, wchar_t** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        if (arg == L"--config-file" || arg == L"--config") {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            return std::filesystem::path(argv[i + 1]).string();
        }

        constexpr std::wstring_view prefix = L"--config-file=";
        if (arg.rfind(prefix.data(), 0) == 0) {
            return std::filesystem::path(arg.substr(prefix.size())).string();
        }
    }

    return std::nullopt;
}

std::optional<int> ParseDomainIdFromCommandLine()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc <= 0) {
        return std::nullopt;
    }

    const auto domainId = ParseDomainIdFromArgs(argc, argv);
    LocalFree(argv);
    return domainId;
}

std::optional<std::string> ParseConfigFileFromCommandLine()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc <= 0) {
        return std::nullopt;
    }

    const auto configFile = ParseConfigFileFromArgs(argc, argv);
    LocalFree(argv);
    return configFile;
}

std::filesystem::path GetExecutableDirectory()
{
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (len == 0 || len >= std::size(path)) {
        return std::filesystem::current_path();
    }

    std::filesystem::path exePath(path);
    return exePath.parent_path();
}

DWORD WINAPI ServiceCtrlHandlerEx(DWORD control, DWORD /*eventType*/, void* /*eventData*/, void* /*context*/)
{
    switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            SetServiceState(SERVICE_STOP_PENDING, NO_ERROR, 0, 3000);
            if (gEventLog) {
                gEventLog->Info(L"Stop requested.");
            }
            gStopSource.RequestStop();
            return NO_ERROR;
        default:
            return NO_ERROR;
    }
}

void WINAPI ServiceMain(DWORD argc, wchar_t** argv)
{
    gEventLog = std::make_unique<service::EventLog>(SERVICE_NAME);

    gStatusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceCtrlHandlerEx, nullptr);
    if (gStatusHandle == nullptr) {
        if (gEventLog) {
            gEventLog->Error(L"RegisterServiceCtrlHandlerExW failed.");
        }
        return;
    }

    ZeroMemory(&gStatus, sizeof(gStatus));
    gStatus.dwCheckPoint = 1;

    SetServiceState(SERVICE_START_PENDING, NO_ERROR, 0, 3000);

    if (gEventLog) {
        gEventLog->Info(L"Start requested.");
    }

    (void)argc;
    (void)argv;

    const auto domainId = ParseDomainIdFromCommandLine();
    if (!domainId.has_value()) {
        if (gEventLog) {
            gEventLog->Error(L"Startup failed: missing or invalid --domain-id.");
        }
        SetServiceState(SERVICE_STOPPED, ERROR_INVALID_PARAMETER, 1);
        return;
    }

    const std::filesystem::path exeDir = GetExecutableDirectory();
    const std::filesystem::path defaultConfigFile = exeDir / "config" / "driver.yaml";
    const std::string configFile = ParseConfigFileFromCommandLine().value_or(defaultConfigFile.string());

    app::AppRunnerOptions options;
    options.configFile = configFile;
    options.domainId = *domainId;
    options.logRxRaw = false;
    options.tableMode = false;
    options.logStartup = false;
    options.logRx = false;
    options.logTxState = false;

    app::AppRunner runner;

    if (gEventLog) {
        std::wstring msg = L"Starting runner. domainId=" + std::to_wstring(*domainId) +
                           L" configFile=" + ToWString(configFile);
        gEventLog->Info(msg);
    }

    try {
        WorkerContext ctx;
        ctx.runner = &runner;
        ctx.options = options;
        ctx.stopToken = gStopSource.Token();
        gWorker = std::thread(WorkerEntry, &ctx);

        // Transition to RUNNING only after worker thread started successfully.
        SetServiceState(SERVICE_RUNNING);

        if (gEventLog) {
            gEventLog->Info(L"Service RUNNING.");
        }

        if (gWorker.joinable()) {
            gWorker.join();
        }

        const int exitCode = gWorkerExitCode.value_or(EXIT_FAILURE);
        if (exitCode == EXIT_SUCCESS) {
            if (gEventLog) {
                gEventLog->Info(L"Service STOPPED.");
            }
            SetServiceState(SERVICE_STOPPED, NO_ERROR);
        } else {
            if (gEventLog) {
                std::wstring err = L"Service STOPPED due to error. " + ToWString(runner.LastError());
                gEventLog->Error(err);
            }
            SetServiceState(SERVICE_STOPPED, NO_ERROR, 1);
        }
        return;
    } catch (...) {
        if (gEventLog) {
            gEventLog->Error(L"Failed to start worker thread.");
        }
        SetServiceState(SERVICE_STOPPED, ERROR_NOT_ENOUGH_MEMORY, 2);
        return;
    }
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    SERVICE_TABLE_ENTRYW dispatchTable[] = {
        {const_cast<LPWSTR>(SERVICE_NAME), ServiceMain},
        {nullptr, nullptr}};

    if (!StartServiceCtrlDispatcherW(dispatchTable)) {
        const DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            std::cerr << "Not started by the Service Control Manager. "
                         "Install/run as a Windows Service." << std::endl;
            return EXIT_FAILURE;
        }

        std::cerr << "StartServiceCtrlDispatcher failed: " << err << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
