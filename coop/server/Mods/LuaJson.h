#pragma once
// JSON <-> Lua values: the mod API speaks JSON, scripts see plain Lua tables (LuaMod.cpp).
#include "common/Events.h"

#include <string>

struct lua_State;

namespace coop::server {

constexpr int kLuaJsonMaxDepth = 16;

// Pushes one Lua value: null -> nil, lists -> tables 1..n, objects -> tables with string keys.
void LuaPushJson(lua_State* L, const json& value);
// The Lua value at idx as JSON. A table whose keys are exactly 1..n is a list, an empty table an empty object, any
// other table an object (number keys become texts). Texts that are not UTF-8 are cleaned. False (with err) for what
// JSON cannot hold: functions, userdata, threads, NaN or infinity, and tables nested deeper than kLuaJsonMaxDepth
// (a table that contains itself).
bool LuaToJson(lua_State* L, int idx, json& out, std::string* err);

} // namespace coop::server
