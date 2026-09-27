#include "BuildId.h"

#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace coop {

uint64_t Fnv1a64(const uint8_t* data, size_t size, uint64_t seed) {
    uint64_t h = seed;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

namespace {

std::string ExePath() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
    if (n == 0) {
        return "";
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, buffer, (int)n, nullptr, 0, nullptr, nullptr);
    std::string out((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, (int)n, out.data(), len, nullptr, nullptr);
    return out;
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    return _NSGetExecutablePath(buffer, &size) == 0 ? std::string(buffer) : std::string();
#else
    char buffer[4096];
    ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    return n > 0 ? std::string(buffer, (size_t)n) : std::string();
#endif
}

FILE* OpenUtf8(const std::string& path) {
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), nullptr, 0);
    std::wstring wide((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), wide.data(), len);
    return _wfopen(wide.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

} // namespace

std::string FileBuildId(const std::string& path) {
    FILE* f = path.empty() ? nullptr : OpenUtf8(path);
    if (f == nullptr) {
        return "";
    }
    uint64_t h = 0xCBF29CE484222325ull;
    std::vector<uint8_t> chunk(1 << 20);
    size_t n = 0;
    while ((n = std::fread(chunk.data(), 1, chunk.size(), f)) > 0) {
        h = Fnv1a64(chunk.data(), n, h);
    }
    std::fclose(f);
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", (unsigned long long)h);
    return text;
}

const std::string& BuildId() {
    static const std::string sId = FileBuildId(ExePath());
    return sId;
}

} // namespace coop
