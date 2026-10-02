#include "LuaJson.h"

#include "common/Text.h"

#include <lauxlib.h>
#include <lua.h>

#include <cmath>

namespace coop::server {

namespace {

bool Fail(std::string* err, const std::string& why) {
    if (err != nullptr) {
        *err = why;
    }
    return false;
}

bool ToJsonAt(lua_State* L, int idx, json& out, std::string* err, int depth) {
    idx = lua_absindex(L, idx);
    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            out = nullptr;
            return true;
        case LUA_TBOOLEAN:
            out = lua_toboolean(L, idx) != 0;
            return true;
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx)) {
                out = (int64_t)lua_tointeger(L, idx);
                return true;
            }
            double d = lua_tonumber(L, idx);
            if (!std::isfinite(d)) {
                return Fail(err, "number is not finite");
            }
            out = d;
            return true;
        }
        case LUA_TSTRING: {
            size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);
            out = ToValidUtf8(std::string(s, len));
            return true;
        }
        case LUA_TTABLE:
            break;
        default:
            return Fail(err, std::string("cannot convert a ") + luaL_typename(L, idx));
    }
    if (depth >= kLuaJsonMaxDepth) {
        return Fail(err, "table nested too deep (or it contains itself)");
    }
    if (!lua_checkstack(L, 4)) {
        return Fail(err, "out of Lua stack");
    }
    lua_Integer n = (lua_Integer)lua_rawlen(L, idx);
    size_t keys = 0;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        keys++;
        lua_pop(L, 1);
    }
    if (n > 0 && keys == (size_t)n) { // exactly 1..n: a list
        out = json::array();
        for (lua_Integer i = 1; i <= n; i++) {
            lua_rawgeti(L, idx, i);
            json item;
            bool ok = ToJsonAt(L, -1, item, err, depth + 1);
            lua_pop(L, 1);
            if (!ok) {
                return false;
            }
            out.push_back(std::move(item));
        }
        return true;
    }
    out = json::object();
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        std::string key;
        if (lua_type(L, -2) == LUA_TSTRING) { // never lua_tolstring on a number key: it would break lua_next
            size_t len = 0;
            const char* s = lua_tolstring(L, -2, &len);
            key = ToValidUtf8(std::string(s, len));
        } else if (lua_isinteger(L, -2)) {
            key = std::to_string(lua_tointeger(L, -2));
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            key = std::to_string(lua_tonumber(L, -2));
        } else {
            lua_pop(L, 2);
            return Fail(err, "table keys must be texts or numbers");
        }
        json item;
        bool ok = ToJsonAt(L, -1, item, err, depth + 1);
        lua_pop(L, 1);
        if (!ok) {
            lua_pop(L, 1);
            return false;
        }
        out[key] = std::move(item);
    }
    return true;
}

} // namespace

void LuaPushJson(lua_State* L, const json& value) {
    luaL_checkstack(L, 4, "json");
    switch (value.type()) {
        case json::value_t::boolean:
            lua_pushboolean(L, value.get<bool>());
            break;
        case json::value_t::number_integer:
            lua_pushinteger(L, (lua_Integer)value.get<int64_t>());
            break;
        case json::value_t::number_unsigned:
            lua_pushinteger(L, (lua_Integer)value.get<uint64_t>());
            break;
        case json::value_t::number_float:
            lua_pushnumber(L, value.get<double>());
            break;
        case json::value_t::string: {
            const std::string& s = value.get_ref<const std::string&>();
            lua_pushlstring(L, s.data(), s.size());
            break;
        }
        case json::value_t::array: {
            lua_createtable(L, (int)value.size(), 0);
            lua_Integer i = 1;
            for (const json& item : value) {
                LuaPushJson(L, item);
                lua_rawseti(L, -2, i++);
            }
            break;
        }
        case json::value_t::object:
            lua_createtable(L, 0, (int)value.size());
            for (auto it = value.begin(); it != value.end(); ++it) {
                lua_pushlstring(L, it.key().data(), it.key().size());
                LuaPushJson(L, it.value());
                lua_rawset(L, -3);
            }
            break;
        default: // null (and what JSON files never hold: binary, discarded)
            lua_pushnil(L);
            break;
    }
}

bool LuaToJson(lua_State* L, int idx, json& out, std::string* err) {
    return ToJsonAt(L, idx, out, err, 0);
}

} // namespace coop::server
