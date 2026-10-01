#pragma once
// [COOP] Sub-project D3: what the sender asks about its own process to tell a pointer from plain data. Kept apart
// from the game's headers (it needs the system's).
#include <cstdint>

namespace coop::client {

// The loaded executable: its base address and size. False where unknown (then code pointers are not translated).
bool ProcessMemory_ExeRange(uint64_t& base, uint64_t& size);
// True if the address is memory this process can read (heap, stacks, the exe, libraries).
bool ProcessMemory_IsMapped(uint64_t address);

} // namespace coop::client
