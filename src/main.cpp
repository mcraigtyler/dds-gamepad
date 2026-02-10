#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "app/AppRunner.h"

void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <config_file> <domain_id> <yoke_id> [--debug | --table]" << std::endl;
    std::cerr << "       " << exe << " --help" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Args:" << std::endl;
    std::cerr << "  <config_file>     YAML role config file (for example: config/driver.yaml)." << std::endl;
    std::cerr << "  <domain_id>       DDS domain id (integer)." << std::endl;
    std::cerr << "  <yoke_id>         Yoke sub_role id used to filter incoming DDS messages." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --debug            Enable verbose raw input logging (rx_raw...)." << std::endl;
    std::cerr << "  --table            Live table + a tx state line (no scrolling)." << std::endl;
    std::cerr << "  -h, --help         Show this help text." << std::endl;
}

namespace
{
app::StopSource* gStopSource = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (gStopSource != nullptr) {
                gStopSource->RequestStop();
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    bool log_rx_raw = false;
    bool table_mode = false;
    std::string config_path;
    std::optional<int> domain_id;
    std::optional<int> yoke_id;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (arg == "--debug") {
            log_rx_raw = true;
            continue;
        }
        if (arg == "--table") {
            table_mode = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        if (config_path.empty()) {
            config_path = arg;
            continue;
        }
        if (!domain_id.has_value()) {
            try {
                domain_id = std::stoi(arg);
            } catch (const std::exception&) {
                std::cerr << "Invalid domain_id: " << arg << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }
        if (!yoke_id.has_value()) {
            try {
                yoke_id = std::stoi(arg);
            } catch (const std::exception&) {
                std::cerr << "Invalid yoke_id: " << arg << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }

        std::cerr << "Unexpected argument: " << arg << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (config_path.empty()) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!domain_id.has_value()) {
        std::cerr << "Missing required domain_id." << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!yoke_id.has_value()) {
        std::cerr << "Missing required yoke_id." << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (table_mode && log_rx_raw) {
        std::cerr << "Options --debug and --table are mutually exclusive." << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    app::AppRunnerOptions options;
    options.configFile = config_path;
    options.domainId = *domain_id;
    options.yokeId = *yoke_id;
    options.logRxRaw = log_rx_raw;
    options.tableMode = table_mode;

    app::StopSource stopSource;
    gStopSource = &stopSource;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    app::AppRunner runner;
    const int exitCode = runner.Run(options, stopSource.Token());

    SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
    gStopSource = nullptr;
    return exitCode;
}
