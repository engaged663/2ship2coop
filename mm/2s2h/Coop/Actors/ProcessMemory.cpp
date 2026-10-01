#include "ProcessMemory.h"

#include <chrono>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace coop::client {

namespace {

// Granules (64 KiB) known to be mapped. Only positive answers are kept: a pointer never looks unmapped by mistake.
std::unordered_set<uint64_t> sMapped;
std::chrono::steady_clock::time_point sClearedAt = std::chrono::steady_clock::now();

bool CanonicalUser(uint64_t a) {
    return a >= 0x10000 && a < 0x0000800000000000ull;
}

} // namespace

bool ProcessMemory_ExeRange(uint64_t& base, uint64_t& size) {
#ifdef _WIN32
    static uint64_t sBase = 0;
    static uint64_t sSize = 0;
    if (sBase == 0) {
        HMODULE module = GetModuleHandleW(nullptr);
        auto* dos = (const IMAGE_DOS_HEADER*)module;
        auto* nt = (const IMAGE_NT_HEADERS*)((const uint8_t*)module + dos->e_lfanew);
        sBase = (uint64_t)module;
        sSize = nt->OptionalHeader.SizeOfImage;
    }
    base = sBase;
    size = sSize;
    return true;
#else
    base = 0;
    size = 0;
    return false; // other systems: code pointers travel as "keep" (the copy keeps its own)
#endif
}

bool ProcessMemory_IsMapped(uint64_t address) {
    if (!CanonicalUser(address)) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    if (now - sClearedAt > std::chrono::seconds(2)) {
        sMapped.clear(); // memory freed since then would otherwise still look mapped
        sClearedAt = now;
    }
    uint64_t granule = address >> 16;
    if (sMapped.count(granule) != 0) {
        return true;
    }
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery((LPCVOID)address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    sMapped.insert(granule);
    return true;
#else
    return true; // no cheap query here: anything that looks like a pointer is kept (safe, a little less exact)
#endif
}

} // namespace coop::client
