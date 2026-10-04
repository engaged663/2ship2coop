#pragma once
// SHA-256 (FIPS 180-4): which exact .o2r file a server shares and a game has (common/O2r.h). Incremental, so a
// download is hashed as its chunks arrive.
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace coop {

class Sha256 {
  public:
    Sha256();
    void Update(const void* data, size_t size);
    std::string FinalHex(); // 64 lowercase hex digits; call it once

  private:
    void Block(const uint8_t* block);

    uint32_t mState[8];
    uint8_t mBuffer[64];
    size_t mBuffered = 0;
    uint64_t mBytes = 0;
};

std::string Sha256Hex(const void* data, size_t size);
// Of a whole file; "" if it cannot be read. size (optional) gets its length.
std::string Sha256File(const std::filesystem::path& path, uint64_t* size = nullptr);

} // namespace coop
