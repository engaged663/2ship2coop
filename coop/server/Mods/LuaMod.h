#pragma once
// A Lua script as a mod: its own Lua state, the `coop` table scripts use (coop/docs/mods/README.md) and the limits
// that keep one script from hurting the server (a safe set of libraries, a time limit per call, a memory limit).
#include "Mod.h"

#include <chrono>
#include <cstddef>
#include <string>

struct lua_State;
struct lua_Debug;

namespace coop::server {

class ModHost;

class LuaMod : public Mod {
  public:
    struct Limits {
        bool unsafe = false;    // the whole Lua (io, os, package) instead of the safe set
        int timeoutMs = 2000;   // a call into the script that runs longer is aborted
        int memoryMb = 64;      // what the script may allocate
        std::string scriptsDir; // where require() looks ("" = next to the script)
    };

    LuaMod(ModHost& host, const std::string& name, const std::string& path, const Limits& limits);
    ~LuaMod() override;

    bool Open(std::string* err);    // creates the Lua state and the coop table
    bool RunFile(std::string* err); // runs the script; the host must already own this mod (it subscribes, etc.)

    bool InvokeEvent(int handler, json& payload, bool* cancel, std::string* err) override;
    bool InvokeTimer(int handler, std::string* err) override;
    bool InvokeCommand(int handler, const json& ctx, const std::vector<std::string>& args, CommandReply* reply,
                       std::string* err) override;
    void ReleaseHandler(int handler) override;

  private:
    // Runs the function below its nargs arguments on the stack. False + err when it fails (nothing is left on the
    // stack); the first call from outside starts the clock of the time limit.
    bool Protected(int nargs, int nresults, std::string* err);
    void OpenLibraries();
    void BuildCoopTable();
    std::string RequireRoot() const;

    static void* Alloc(void* ud, void* ptr, size_t osize, size_t nsize);
    static void Hook(lua_State* L, lua_Debug* ar);
    static int Panic(lua_State* L);
    static int Traceback(lua_State* L);
    static int Print(lua_State* L);
    static int Require(lua_State* L);
    static int On(lua_State* L);
    static int Off(lua_State* L);
    static int Emit(lua_State* L);
    static int TimerAfter(lua_State* L);
    static int TimerEvery(lua_State* L);
    static int TimerCancel(lua_State* L);
    static int CommandRegister(lua_State* L);
    static int CommandUnregister(lua_State* L);
    static int ApiTrampoline(lua_State* L);
    static int AddTimer(lua_State* L, bool repeat);

    ModHost& mHost;
    Limits mLimits;
    lua_State* mL = nullptr;
    size_t mUsed = 0;       // bytes the state holds
    size_t mLimitBytes = 0;
    int mEntered = 0;       // calls from outside in progress (a script may run inside itself through an event)
    std::chrono::steady_clock::time_point mDeadline;
};

} // namespace coop::server
