#pragma once
// Timestamped log to the console and (optionally) a file.
#include <fstream>
#include <mutex>
#include <string>

namespace coop::server {

class Logger {
  public:
    // filePath "" = no file; echo = also print to stdout.
    explicit Logger(const std::string& filePath = "", bool echo = true);

    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);

  private:
    void Write(const char* level, const std::string& message);

    bool mEcho;
    std::ofstream mFile;
    std::mutex mMutex;
};

} // namespace coop::server
