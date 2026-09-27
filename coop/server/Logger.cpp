#include "Logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace coop::server {

Logger::Logger(const std::string& filePath, bool echo) : mEcho(echo) {
    if (!filePath.empty()) {
        mFile.open(filePath, std::ios::app);
    }
}

void Logger::Info(const std::string& message) {
    Write("INFO", message);
}

void Logger::Warn(const std::string& message) {
    Write("WARN", message);
}

void Logger::Error(const std::string& message) {
    Write("ERROR", message);
}

std::vector<std::string> Logger::Lines() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return { mRecent.begin(), mRecent.end() };
}

void Logger::Write(const char* level, const std::string& message) {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    std::string line = std::string("[") + stamp + "] [" + level + "] " + message;

    std::lock_guard<std::mutex> lock(mMutex);
    mRecent.push_back(line);
    if (mRecent.size() > 200) {
        mRecent.pop_front();
    }
    if (mEcho) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    }
    if (mFile.is_open()) {
        mFile << line << '\n';
        mFile.flush();
    }
}

} // namespace coop::server
