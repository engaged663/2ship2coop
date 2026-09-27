#pragma once
// Timestamped log to the console and (optionally) a file.
#include <fstream>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

namespace coop::server {

class Logger {
  public:
    // filePath "" = no file; echo = also print to stdout.
    explicit Logger(const std::string& filePath = "", bool echo = true);

    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    std::vector<std::string> Lines() const; // the most recent lines (tests, diagnostics)

  private:
    void Write(const char* level, const std::string& message);

    bool mEcho;
    std::ofstream mFile;
    mutable std::mutex mMutex;
    std::deque<std::string> mRecent;
};

} // namespace coop::server
