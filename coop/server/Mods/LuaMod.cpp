#include "LuaMod.h"

#include "LuaJson.h"
#include "ModApi.h"
#include "ModHost.h"

#include "common/I18n.h"
#include "common/Text.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Lua is built as C++ (coop/CMakeLists.txt): a Lua error is a C++ exception, so the C++ objects of the functions it
// passes through are destroyed as usual. Outside a protected call an error reaches Panic, which turns it into a
// std::runtime_error for Open/RunFile/Invoke* to report. Never catch (...) here: that would swallow Lua's own.

namespace coop::server {

namespace {

const char* const kLoadedKey = "coop.loaded"; // registry: what require() already loaded (safe mode)
constexpr int kHookEvery = 10000;            // instructions between two looks at the clock
constexpr double kMaxTimerMs = 30.0 * 86400000.0;

// Any state of a script (also a coroutine's) leads back to its mod.
LuaMod* Self(lua_State* L) {
    return *(LuaMod**)lua_getextraspace(L);
}

bool ReadFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    out = buffer.str();
    if (out.rfind("\xEF\xBB\xBF", 0) == 0) {
        out.erase(0, 3); // a UTF-8 mark some editors write
    }
    return true;
}

std::string ErrorText(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    return msg != nullptr ? msg : "error";
}

} // namespace

LuaMod::LuaMod(ModHost& host, const std::string& name, const std::string& path, const Limits& limits)
    : mHost(host), mLimits(limits) {
    mInfo.name = name;
    mInfo.kind = "lua";
    mInfo.path = path;
    mLimitBytes = (size_t)std::max(limits.memoryMb, 1) * 1024 * 1024;
}

LuaMod::~LuaMod() {
    if (mL != nullptr) {
        mEntered = 1; // finalizers written in Lua run under the time limit too
        mDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(mLimits.timeoutMs);
        lua_close(mL);
    }
}

// The allocator refuses to grow past the limit; Lua turns that into a "not enough memory" error.
void* LuaMod::Alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    LuaMod* self = (LuaMod*)ud;
    size_t old = ptr != nullptr ? osize : 0; // for a new block osize is its kind, not a size
    if (nsize == 0) {
        self->mUsed -= old;
        std::free(ptr);
        return nullptr;
    }
    if (nsize > old && self->mUsed + (nsize - old) > self->mLimitBytes) {
        return nullptr;
    }
    void* out = std::realloc(ptr, nsize);
    if (out != nullptr) {
        self->mUsed = self->mUsed - old + nsize;
    }
    return out;
}

// Every kHookEvery instructions: past the deadline of the outermost call the script is aborted.
void LuaMod::Hook(lua_State* L, lua_Debug*) {
    LuaMod* self = Self(L);
    if (self->mEntered > 0 && std::chrono::steady_clock::now() > self->mDeadline) {
        luaL_error(L, "script timeout (%d ms)", self->mLimits.timeoutMs);
    }
}

int LuaMod::Panic(lua_State* L) {
    throw std::runtime_error(ErrorText(L));
}

int LuaMod::Traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg == nullptr) {
        msg = luaL_tolstring(L, 1, nullptr); // error({...}): whatever tostring says of it
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

bool LuaMod::Open(std::string* err) {
    try {
        mL = lua_newstate(Alloc, this);
        if (mL == nullptr) {
            *err = "not enough memory";
            return false;
        }
        *(LuaMod**)lua_getextraspace(mL) = this;
        lua_atpanic(mL, Panic);
        lua_sethook(mL, Hook, LUA_MASKCOUNT, kHookEvery);
        OpenLibraries();
        BuildCoopTable();
        return true;
    } catch (const std::exception& e) {
        *err = e.what();
        return false;
    }
}

std::string LuaMod::RequireRoot() const {
    return mLimits.scriptsDir.empty() ? std::filesystem::path(mInfo.path).parent_path().string() : mLimits.scriptsDir;
}

void LuaMod::OpenLibraries() {
    lua_State* L = mL;
    if (mLimits.unsafe) {
        luaL_openlibs(L);
        // The standard require also looks in the scripts folder.
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");
        std::string root = RequireRoot();
        std::string path = root + "/?.lua;" + root + "/?/init.lua;" + ErrorText(L);
        lua_pop(L, 1);
        lua_pushlstring(L, path.data(), path.size());
        lua_setfield(L, -2, "path");
        lua_pop(L, 1);
    } else {
        // No files, no processes, no loading of code from text or from outside the scripts folder.
        static const luaL_Reg kSafe[] = { { LUA_GNAME, luaopen_base },         { LUA_TABLIBNAME, luaopen_table },
                                          { LUA_STRLIBNAME, luaopen_string },  { LUA_MATHLIBNAME, luaopen_math },
                                          { LUA_UTF8LIBNAME, luaopen_utf8 },   { LUA_COLIBNAME, luaopen_coroutine },
                                          { LUA_OSLIBNAME, luaopen_os } };
        for (const luaL_Reg& lib : kSafe) {
            luaL_requiref(L, lib.name, lib.func, 1);
            lua_pop(L, 1);
        }
        for (const char* gone : { "dofile", "loadfile", "load" }) {
            lua_pushnil(L);
            lua_setglobal(L, gone);
        }
        lua_getglobal(L, "os"); // of os only the clock stays
        lua_newtable(L);
        for (const char* keep : { "time", "date", "clock", "difftime" }) {
            lua_getfield(L, -2, keep);
            lua_setfield(L, -2, keep);
        }
        lua_setglobal(L, "os");
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setfield(L, LUA_REGISTRYINDEX, kLoadedKey);
        lua_pushcfunction(L, Require);
        lua_setglobal(L, "require");
    }
    lua_pushcfunction(L, Print);
    lua_setglobal(L, "print");
}

// The global `coop`: on/off/emit, timer.*, commands.* (they take functions, so they live here) and one function per
// line of the API table (ModApi.h), by namespace.
void LuaMod::BuildCoopTable() {
    lua_State* L = mL;
    lua_newtable(L);
    auto fn = [L](const char* name, lua_CFunction f) {
        lua_pushcfunction(L, f);
        lua_setfield(L, -2, name);
    };
    fn("on", On);
    fn("off", Off);
    fn("emit", Emit);
    lua_newtable(L);
    fn("after", TimerAfter);
    fn("every", TimerEvery);
    fn("cancel", TimerCancel);
    lua_setfield(L, -2, "timer");
    lua_newtable(L);
    fn("register", CommandRegister);
    fn("unregister", CommandUnregister);
    lua_setfield(L, -2, "commands");
    for (const ApiDef* def : AllApis()) {
        std::string full = def->name;
        size_t dot = full.find('.');
        std::string space = full.substr(0, dot);
        std::string name = full.substr(dot + 1);
        lua_getfield(L, -1, space.c_str());
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, space.c_str());
        }
        lua_pushlightuserdata(L, (void*)def);
        lua_pushcclosure(L, ApiTrampoline, 1);
        lua_setfield(L, -2, name.c_str());
        lua_pop(L, 1);
    }
    lua_setglobal(L, "coop");
}

bool LuaMod::RunFile(std::string* err) {
    std::string code;
    if (!ReadFile(mInfo.path, code)) {
        *err = Tr(Msg::ModFileMissing, { mInfo.path });
        return false;
    }
    try {
        // "@file.lua": errors say "file.lua:12: ..."
        std::string chunk = "@" + std::filesystem::path(mInfo.path).filename().string();
        if (luaL_loadbufferx(mL, code.data(), code.size(), chunk.c_str(), mLimits.unsafe ? nullptr : "t") != LUA_OK) {
            *err = ErrorText(mL);
            lua_pop(mL, 1);
            return false;
        }
        return Protected(0, 0, err);
    } catch (const std::exception& e) {
        *err = e.what();
        return false;
    }
}

bool LuaMod::Protected(int nargs, int nresults, std::string* err) {
    lua_State* L = mL;
    if (mEntered++ == 0) {
        mDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(mLimits.timeoutMs);
    }
    int base = lua_gettop(L) - nargs; // where the function is
    lua_pushcfunction(L, Traceback);
    lua_insert(L, base);
    int status = lua_pcall(L, nargs, nresults, base);
    lua_remove(L, base);
    mEntered--;
    if (status == LUA_OK) {
        return true;
    }
    *err = ErrorText(L);
    lua_pop(L, 1);
    if (status == LUA_ERRMEM) {
        lua_gc(L, LUA_GCCOLLECT, 0); // what the failed call left behind
    }
    return false;
}

bool LuaMod::InvokeEvent(int handler, json& payload, bool* cancel, std::string* err) {
    lua_State* L = mL;
    int top = lua_gettop(L);
    try {
        if (!lua_checkstack(L, 8)) {
            *err = "out of Lua stack";
            return false;
        }
        LuaPushJson(L, payload);                    // e (kept to read it back after the call)
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler); // e fn
        lua_pushvalue(L, -2);                       // e fn e
        if (!Protected(1, 1, err)) {
            lua_settop(L, top);
            return false;
        }
        if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) { // return false
            *cancel = true;
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "cancel"); // e.cancel = true
        if (lua_toboolean(L, -1)) {
            *cancel = true;
        }
        lua_pop(L, 1);
        json seen;
        bool ok = LuaToJson(L, -1, seen, err);
        lua_settop(L, top);
        if (!ok) {
            return false;
        }
        if (seen.is_object()) {
            seen.erase("cancel");
            payload = std::move(seen);
        }
        return true;
    } catch (const std::exception& e) {
        *err = e.what();
        return false;
    }
}

bool LuaMod::InvokeTimer(int handler, std::string* err) {
    try {
        if (!lua_checkstack(mL, 4)) {
            *err = "out of Lua stack";
            return false;
        }
        lua_rawgeti(mL, LUA_REGISTRYINDEX, handler);
        return Protected(0, 0, err);
    } catch (const std::exception& e) {
        *err = e.what();
        return false;
    }
}

bool LuaMod::InvokeCommand(int handler, const json& ctx, const std::vector<std::string>& args, CommandReply* reply,
                           std::string* err) {
    lua_State* L = mL;
    int top = lua_gettop(L);
    try {
        if (!lua_checkstack(L, 8)) {
            *err = "out of Lua stack";
            return false;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler);
        LuaPushJson(L, ctx);
        lua_createtable(L, (int)args.size(), 0);
        for (size_t i = 0; i < args.size(); i++) {
            lua_pushlstring(L, args[i].data(), args[i].size());
            lua_rawseti(L, -2, (lua_Integer)i + 1);
        }
        if (!Protected(2, 2, err)) {
            lua_settop(L, top);
            return false;
        }
        // What it returns is the answer: text, or text and level.
        if (lua_type(L, -2) == LUA_TSTRING || lua_type(L, -2) == LUA_TNUMBER) {
            size_t len = 0;
            const char* text = lua_tolstring(L, -2, &len);
            reply->text.assign(text, len);
        }
        if (lua_type(L, -1) == LUA_TSTRING) {
            reply->level = lua_tostring(L, -1);
        }
        lua_settop(L, top);
        return true;
    } catch (const std::exception& e) {
        *err = e.what();
        return false;
    }
}

void LuaMod::ReleaseHandler(int handler) {
    if (mL != nullptr) {
        luaL_unref(mL, LUA_REGISTRYINDEX, handler);
    }
}

// print(...) goes to the server's log, with the mod's name.
int LuaMod::Print(lua_State* L) {
    LuaMod* self = Self(L);
    int n = lua_gettop(L);
    std::string line;
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) {
            line.push_back('\t');
        }
        line.append(s, len);
        lua_pop(L, 1);
    }
    self->mHost.Log(*self, "info", ToValidUtf8(line));
    return 0;
}

// require("lib.util") loads <scripts folder>/lib/util.lua once (safe mode; the unsafe one has Lua's own require).
int LuaMod::Require(lua_State* L) {
    LuaMod* self = Self(L);
    size_t len = 0;
    const char* raw = luaL_checklstring(L, 1, &len);
    std::string name(raw, len);
    lua_settop(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, kLoadedKey); // 2: what is already loaded
    lua_getfield(L, 2, name.c_str());
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);
    bool valid = !name.empty() && name.front() != '.' && name.back() != '.' && name.find("..") == std::string::npos;
    for (unsigned char c : name) {
        valid = valid && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                          c == '.');
    }
    if (!valid) {
        return luaL_error(L, "require: invalid module name (letters, digits, _ and . only)");
    }
    std::string relative = name;
    std::replace(relative.begin(), relative.end(), '.', '/');
    relative += ".lua";
    std::string code;
    if (!ReadFile((std::filesystem::path(self->RequireRoot()) / relative).string(), code)) {
        return luaL_error(L, "require: module '%s' not found (%s in the scripts folder)", name.c_str(),
                          relative.c_str());
    }
    std::string chunk = "@" + relative;
    if (luaL_loadbufferx(L, code.data(), code.size(), chunk.c_str(), "t") != LUA_OK) {
        return lua_error(L);
    }
    lua_pushlstring(L, name.data(), name.size());
    lua_call(L, 1, 1);
    if (lua_isnil(L, -1)) { // a module that returns nothing
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, 2, name.c_str());
    return 1;
}

// coop.on(event, fn) -> id
int LuaMod::On(lua_State* L) {
    LuaMod* self = Self(L);
    std::string event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int handler = luaL_ref(L, LUA_REGISTRYINDEX);
    std::string err;
    uint32_t id = self->mHost.Subscribe(*self, event, handler, &err);
    if (id == 0) {
        luaL_unref(L, LUA_REGISTRYINDEX, handler);
        return luaL_error(L, "coop.on: %s", err.c_str());
    }
    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

// coop.off(id) -> boolean
int LuaMod::Off(lua_State* L) {
    LuaMod* self = Self(L);
    lua_pushboolean(L, self->mHost.Unsubscribe(*self, (uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}

// coop.emit(name, payload?) -> payload, cancelled
int LuaMod::Emit(lua_State* L) {
    LuaMod* self = Self(L);
    std::string name = luaL_checkstring(L, 1);
    if (name.find(':') == std::string::npos) {
        return luaL_error(L, "coop.emit: the name of an event between mods needs a ':' (\"mymod:%s\")", name.c_str());
    }
    json payload = json::object();
    if (!lua_isnoneornil(L, 2)) {
        std::string why;
        if (!LuaToJson(L, 2, payload, &why)) {
            return luaL_error(L, "coop.emit: %s", why.c_str());
        }
        if (!payload.is_object()) {
            return luaL_error(L, "coop.emit: the payload must be a table with named fields");
        }
    }
    bool delivered = self->mHost.FireCustom(name, payload);
    LuaPushJson(L, payload);
    lua_pushboolean(L, !delivered);
    return 2;
}

int LuaMod::AddTimer(lua_State* L, bool repeat) {
    LuaMod* self = Self(L);
    lua_Number ms = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!(ms >= 0 && ms <= kMaxTimerMs)) {
        return luaL_argerror(L, 1, "milliseconds out of range");
    }
    lua_pushvalue(L, 2);
    int handler = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, (lua_Integer)self->mHost.AddTimer(*self, handler, (int64_t)ms, repeat));
    return 1;
}

// coop.timer.after(ms, fn) -> id
int LuaMod::TimerAfter(lua_State* L) {
    return AddTimer(L, false);
}

// coop.timer.every(ms, fn) -> id
int LuaMod::TimerEvery(lua_State* L) {
    return AddTimer(L, true);
}

// coop.timer.cancel(id) -> boolean
int LuaMod::TimerCancel(lua_State* L) {
    LuaMod* self = Self(L);
    lua_pushboolean(L, self->mHost.CancelTimer(*self, (uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}

// coop.commands.register(name, opts, fn) or (name, fn)
int LuaMod::CommandRegister(lua_State* L) {
    LuaMod* self = Self(L);
    std::string name = luaL_checkstring(L, 1);
    int fnIndex = lua_isfunction(L, 2) ? 2 : 3;
    luaL_checktype(L, fnIndex, LUA_TFUNCTION);
    json opts = json::object();
    if (fnIndex == 3 && !lua_isnil(L, 2)) {
        std::string why;
        if (!LuaToJson(L, 2, opts, &why) || !opts.is_object()) {
            return luaL_error(L, "coop.commands.register: the options must be a table { usage, help, perm, ... }");
        }
    }
    lua_pushvalue(L, fnIndex);
    int handler = luaL_ref(L, LUA_REGISTRYINDEX);
    std::string err;
    if (!self->mHost.AddCommand(*self, name, opts, handler, &err)) {
        luaL_unref(L, LUA_REGISTRYINDEX, handler);
        return luaL_error(L, "coop.commands.register: %s", err.c_str());
    }
    return 0;
}

// coop.commands.unregister(name) -> boolean
int LuaMod::CommandUnregister(lua_State* L) {
    LuaMod* self = Self(L);
    lua_pushboolean(L, self->mHost.RemoveCommand(*self, luaL_checkstring(L, 1)));
    return 1;
}

// coop.<namespace>.<function>(...): its upvalue is the line of the API table.
int LuaMod::ApiTrampoline(lua_State* L) {
    const ApiDef* def = (const ApiDef*)lua_touserdata(L, lua_upvalueindex(1));
    LuaMod* self = Self(L);
    std::string error;
    {
        json args = json::array();
        int n = lua_gettop(L);
        for (int i = 1; i <= n && error.empty(); i++) {
            json value;
            std::string why;
            if (!LuaToJson(L, i, value, &why)) {
                error = std::string(def->name) + ": " + why;
            } else {
                args.push_back(std::move(value));
            }
        }
        if (error.empty()) {
            try {
                json result = self->mHost.Call(*self, def->name, args);
                LuaPushJson(L, result);
                return 1;
            } catch (const ApiError& e) {
                error = e.what();
            }
        }
    }
    return luaL_error(L, "%s", error.c_str());
}

} // namespace coop::server
