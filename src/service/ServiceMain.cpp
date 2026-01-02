#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "app/AppRunner.h"

namespace
{
constexpr const wchar_t* SERVICE_NAME = L"dds-gamepad-service";

SERVICE_STATUS_HANDLE gStatusHandle = nullptr;
SERVICE_STATUS gStatus{};

std::thread gWorker;
std::optional<int> gWorkerExitCode;

app::StopSource gStopSource;

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
            gStopSource.RequestStop();
            return NO_ERROR;
        default:
            return NO_ERROR;
    }
}

void WINAPI ServiceMain(DWORD argc, wchar_t** argv)
{
    gStatusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceCtrlHandlerEx, nullptr);
    if (gStatusHandle == nullptr) {
        return;
    }

    ZeroMemory(&gStatus, sizeof(gStatus));
    gStatus.dwCheckPoint = 1;

    SetServiceState(SERVICE_START_PENDING, NO_ERROR, 0, 3000);

    const auto domainId = ParseDomainIdFromArgs(static_cast<int>(argc), argv);
    if (!domainId.has_value()) {
        SetServiceState(SERVICE_STOPPED, ERROR_INVALID_PARAMETER, 1);
        return;
    }

    const std::filesystem::path exeDir = GetExecutableDirectory();
    const std::filesystem::path configDir = exeDir / "config";

    app::AppRunnerOptions options;
    options.configDir = configDir.string();
    options.domainId = *domainId;
    options.logRxRaw = false;
    options.tableMode = false;

    app::AppRunner runner;

    try {
        gWorker = std::thread([&]() {
            gWorkerExitCode = runner.Run(options, gStopSource.Token());
        });
    } catch (...) {
        SetServiceState(SERVICE_STOPPED, ERROR_NOT_ENOUGH_MEMORY, 2);
        return;
    }

    SetServiceState(SERVICE_RUNNING);

    if (gWorker.joinable()) {
        gWorker.join();
    }

    const int exitCode = gWorkerExitCode.value_or(EXIT_FAILURE);
    if (exitCode == EXIT_SUCCESS) {
        SetServiceState(SERVICE_STOPPED, NO_ERROR);
    } else {
        // Report a service-specific error code (1) so SCM treats it as failure.
        SetServiceState(SERVICE_STOPPED, NO_ERROR, 1);
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
