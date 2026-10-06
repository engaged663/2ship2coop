#include "ServerLock.h"

#include "common/Text.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace coop::server {

namespace {

bool ReadPid(const std::string& path, uint32_t& pid) {
    std::ifstream in(path);
    std::string text;
    int value = 0;
    if (!in.is_open() || !std::getline(in, text) || !ParseInt(text, value) || value <= 0) {
        return false;
    }
    pid = (uint32_t)value;
    return true;
}

bool ProcessAlive(uint32_t pid) {
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return GetLastError() == ERROR_ACCESS_DENIED; // it exists, it is just not ours to open
    }
    DWORD code = 0;
    bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
#else
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}

} // namespace

uint32_t CurrentProcessId() {
#ifdef _WIN32
    return (uint32_t)GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

bool WriteServerLock(const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    out << CurrentProcessId() << '\n';
    return out.good();
}

void RemoveServerLock(const std::string& path) {
    uint32_t pid = 0;
    if (ReadPid(path, pid) && pid == CurrentProcessId()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}

bool ServerLockHeld(const std::string& path, uint32_t* pid) {
    uint32_t owner = 0;
    if (!ReadPid(path, owner) || owner == CurrentProcessId() || !ProcessAlive(owner)) {
        return false;
    }
    if (pid != nullptr) {
        *pid = owner;
    }
    return true;
}

} // namespace coop::server
