// [COOP] The server's host has no window nor console: when it aborts, write the call stack next to it
// (host-crash.txt) so the server's admin can see why. Installed only in host mode (HostMode_ParseArgs).
#include "HostMode.h"

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

namespace {

void WriteStack(const char* why) {
    FILE* f = std::fopen("host-crash.txt", "a");
    if (f == nullptr) {
        return;
    }
    std::fprintf(f, "--- %s ---\n", why);
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);
    void* frames[64];
    USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);
    char buffer[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
    for (USHORT i = 0; i < count; i++) {
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 500;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line = { sizeof(IMAGEHLP_LINE64) };
        DWORD lineDisp = 0;
        bool hasName = SymFromAddr(process, (DWORD64)frames[i], &displacement, symbol);
        bool hasLine = SymGetLineFromAddr64(process, (DWORD64)frames[i], &lineDisp, &line);
        std::fprintf(f, "%2u %s", i, hasName ? symbol->Name : "?");
        if (hasLine) {
            std::fprintf(f, "  (%s:%lu)", line.FileName, line.LineNumber);
        }
        std::fprintf(f, "\n");
    }
    std::fclose(f);
}

void OnAbort(int) {
    WriteStack("abort");
}

} // namespace

void coop::client::HostCrash_Install() {
    std::signal(SIGABRT, OnAbort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}
#else
void coop::client::HostCrash_Install() {
}
#endif
