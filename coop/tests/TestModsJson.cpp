// JSON <-> Lua (server/Mods/LuaJson.h) and Lua itself inside the server build.
#include "TestMain.h"

#include "common/Text.h"
#include "server/Mods/LuaJson.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

using coop::json;
using namespace coop::server;

namespace {

struct LuaState {
    lua_State* L = luaL_newstate();
    LuaState() {
        luaL_openlibs(L);
    }
    ~LuaState() {
        lua_close(L);
    }
    // Runs "return <expr>" and converts what it returns.
    bool Eval(const std::string& expr, json& out, std::string* err = nullptr) {
        if (luaL_dostring(L, ("return " + expr).c_str()) != LUA_OK) {
            coop_test::Fail(__FILE__, __LINE__, std::string("lua: ") + lua_tostring(L, -1));
        }
        bool ok = LuaToJson(L, -1, out, err);
        lua_pop(L, 1);
        return ok;
    }
};

} // namespace

TEST_CASE(LuaRunsInsideTheServerBuild) {
    LuaState lua;
    json v;
    CHECK(lua.Eval("1 + 1", v));
    CHECK_EQ(v.get<int>(), 2);
}

TEST_CASE(LuaJsonRoundTripKeepsTypes) {
    LuaState lua;
    json in = { { "n", 5 },      { "f", 1.5 },    { "s", "hola" },
                { "b", true },   { "list", { 1, 2, 3 } }, { "obj", { { "k", "v" } } },
                { "big", 9007199254740993LL } };
    LuaPushJson(lua.L, in);
    json out;
    std::string err;
    CHECK(LuaToJson(lua.L, -1, out, &err));
    CHECK(out == in);
    CHECK(out["n"].is_number_integer());
    CHECK(out["f"].is_number_float());
}

TEST_CASE(LuaTablesBecomeListsOrObjects) {
    LuaState lua;
    json v;
    CHECK(lua.Eval("{10, 20, 30}", v));
    CHECK(v.is_array());
    CHECK_EQ(v.size(), (size_t)3);
    CHECK(lua.Eval("{}", v));
    CHECK(v.is_object());
    CHECK(v.empty());
    CHECK(lua.Eval("{a = 1, [2] = 'x'}", v));
    CHECK(v.is_object());
    CHECK_EQ(v["2"].get<std::string>(), std::string("x"));
    CHECK(lua.Eval("{1, 2, nil, 4}", v)); // a list with a hole is an object, never a shorter list
    CHECK(v.is_object());
    CHECK_EQ(v["4"].get<int>(), 4);
}

TEST_CASE(LuaJsonRefusesWhatJsonCannotHold) {
    LuaState lua;
    json v;
    std::string err;
    CHECK(!lua.Eval("print", v, &err));
    CHECK(!err.empty());
    CHECK(!lua.Eval("0/0", v, &err));
    CHECK(!lua.Eval("(function() local t = {} t.me = t return t end)()", v, &err));
    CHECK(lua.Eval("'caf\\xC3\\xA9 \\xFF'", v)); // bytes that are not UTF-8 never reach the JSON
    CHECK(coop::IsValidUtf8(v.get<std::string>()));
    CHECK(v.get<std::string>().find("caf\xC3\xA9") == 0);
}

TEST_CASE(ValidUtf8KeepsGoodTextAndReplacesBadBytes) {
    CHECK(coop::IsValidUtf8("ni\xC3\xB1o \xE4\xB8\xAD"));
    CHECK(!coop::IsValidUtf8("\xC3"));
    CHECK(!coop::IsValidUtf8("\xC0\x80")); // overlong
    CHECK_EQ(coop::ToValidUtf8("a\xFF" "b"), std::string("a?b"));
    CHECK_EQ(coop::ToValidUtf8("ni\xC3\xB1o"), std::string("ni\xC3\xB1o"));
}
