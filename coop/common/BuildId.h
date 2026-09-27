#pragma once
// [COOP] Sub-project D: which exact game executable this is. The server lets in only games with the same build as
// the first one it accepted (D3 copies pointers into the game's code, which only line up in the same exe).
#include <cstdint>
#include <string>
#include <string_view>

namespace coop {

// FNV-1a 64 of some bytes (also used by the tests).
uint64_t Fnv1a64(const uint8_t* data, size_t size, uint64_t seed = 0xCBF29CE484222325ull);

// Hash of the running executable's file as 16 hex digits; "" if it cannot be read. Computed once.
const std::string& BuildId();

// Hash of a file as 16 hex digits; "" if it cannot be read (the server hashes the host exe it starts, D2).
std::string FileBuildId(const std::string& path);

} // namespace coop
