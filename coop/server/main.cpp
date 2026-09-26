// 2ship-coop-server: dedicated co-op server for 2 Ship 2 Harkinian.
// Files live in the working directory: server.json (settings), bans.json, ops.json, logs/server.log.
// Usage: 2ship-coop-server [--port N]
#include "server/AccessLists.h"
#include "server/Logger.h"
#include "server/Server.h"
#include "server/ServerConfig.h"

#include <atomic>
#include <csignal>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace coop::server;

namespace {

std::mutex gConsoleMutex;
std::deque<std::string> gConsoleLines;
std::atomic<bool> gStopRequested{ false };

void ConsoleThread() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lock(gConsoleMutex);
        gConsoleLines.push_back(line);
    }
}

#ifdef _WIN32
BOOL WINAPI OnConsoleEvent(DWORD type) {
    gStopRequested = true;
    if (type == CTRL_CLOSE_EVENT) {
        Sleep(1500); // window closing: give the main loop time to say goodbye to everyone
    }
    return TRUE;
}
#else
void OnSignal(int) {
    gStopRequested = true;
}
#endif

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleCtrlHandler(OnConsoleEvent, TRUE);
#else
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
#endif

    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    Logger log("logs/server.log", true);

    ServerConfig config;
    std::string err;
    if (!LoadOrCreateConfig("server.json", config, &err)) {
        log.Error(err);
        return 1;
    }
    for (int i = 1; i + 1 < argc; i++) {
        if (std::string(argv[i]) == "--port") {
            config.port = (uint16_t)std::stoi(argv[i + 1]);
        }
    }

    AccessLists access("bans.json", "ops.json");
    access.Load();

    Server server(config, access, log);
    if (!server.Start(&err)) {
        log.Error(err);
        return 1;
    }
    log.Info("Escribe 'help' para ver los comandos de la consola.");

    std::thread(ConsoleThread).detach();
    while (server.IsRunning()) {
        server.Tick(5);
        std::deque<std::string> lines;
        {
            std::lock_guard<std::mutex> lock(gConsoleMutex);
            lines.swap(gConsoleLines);
        }
        for (const std::string& line : lines) {
            if (!line.empty()) {
                server.ExecuteConsoleLine(line);
            }
        }
        if (gStopRequested.exchange(false)) {
            server.Stop("servidor cerrado por el administrador");
        }
    }
    return 0;
}
