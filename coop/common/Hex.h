#pragma once
// Bytes <-> lowercase hexadecimal text: how the shared world's fields travel in JSON.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop {

std::string ToHex(const uint8_t* data, size_t size);
inline std::string ToHex(const std::vector<uint8_t>& bytes) {
    return ToHex(bytes.data(), bytes.size());
}
// False for an odd length or a non-hex character (out is left unchanged).
bool FromHex(const std::string& text, std::vector<uint8_t>& out);

} // namespace coop
