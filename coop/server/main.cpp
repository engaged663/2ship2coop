// 2ship-coop-server: dedicated co-op server for 2 Ship 2 Harkinian.
// Files live in the working directory: server.json (settings), bans.json, ops.json, world.json + players/
// (the shared world, see World/SharedWorld.h), logs/server.log, the mods' folders (mods/, plugins/: see
// coop/docs/mods/README.md) and o2r/ (the game mods the players download: O2rStore.h).
// Usage: 2ship-coop-server [--port N] [--lang es|en|zh|ru] [--mods-dir D] [--plugins-dir D] [--script F]...
//                          [--plugin F]... [--no-mods] [--mod-docs D] [--o2r-dir D] [--no-o2r]
//                          (ServerConfig.h: ApplyCommandLine)
#include "server/AccessLists.h"
#include "server/Logger.h"
#include "server/Mods/ModDocs.h"
#include "server/Server.h"
#include "server/ServerConfig.h"

#include "common/Text.h"

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

    // --mod-docs <dir>: write the mod reference (API.md, IDS.md) and exit, without server.json, logs or the port.
    {
        ServerConfig scratch;
        CommandLine line;
        ApplyCommandLine(scratch, argc, argv, &line);
        if (!line.modDocsDir.empty()) {
            std::string why;
            if (!WriteModDocs(line.modDocsDir, &why)) {
                std::cerr << coop::Tr(coop::Msg::ModDocsFailed, { line.modDocsDir, why }) << std::endl;
                return 1;
            }
            std::cout << coop::Tr(coop::Msg::ModDocsWritten, { line.modDocsDir }) << std::endl;
            return 0;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    Logger log("logs/server.log", true);

    ServerConfig config;
    std::string err;
    if (!LoadOrCreateConfig("server.json", config, &err)) {
        log.Error(err);
        return 1;
    }
    if (!err.empty()) {
        log.Warn(err);
    }
    CommandLine commandLine;
    err = ApplyCommandLine(config, argc, argv, &commandLine);
    if (!err.empty()) {
        log.Warn(err);
    }

    AccessLists access("bans.json", "ops.json");
    std::string accessWarnings;
    access.Load(&accessWarnings);
    if (!accessWarnings.empty()) {
        log.Warn(accessWarnings);
    }

    config.configPath = "server.json";
    config.worldPath = "world.json";
    config.playersDir = "players";
    Server server(config, access, log);
    if (!server.Start(&err)) {
        log.Error(err);
        return 1;
    }
    log.Info(coop::Tr(coop::Msg::ConsoleHint));

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
            server.Stop(coop::Tr(coop::Msg::StopByAdmin));
        }
    }
    return 0;
}
