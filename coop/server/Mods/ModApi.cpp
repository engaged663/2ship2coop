#include "ModApi.h"

#include "server/Server.h"

#include "common/Text.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace coop::server {

namespace {

// Function-local static: safe to fill from the static registrars of other files.
std::map<std::string, ApiDef>& Table() {
    static std::map<std::string, ApiDef> table;
    return table;
}

std::string Index(size_t i) {
    return std::to_string(i + 1);
}

bool InWorld(const RemoteClient& c) {
    return c.welcomed && !c.closing && !c.host && c.inWorld;
}

} // namespace

void RegisterApi(const ApiDef& def) {
    Table()[def.name] = def;
}

const ApiDef* FindApi(const std::string& name) {
    auto it = Table().find(name);
    return it != Table().end() ? &it->second : nullptr;
}

std::vector<const ApiDef*> AllApis() {
    std::vector<const ApiDef*> out;
    for (auto& entry : Table()) {
        out.push_back(&entry.second);
    }
    return out; // std::map keeps them sorted by name
}

size_t ApiCall::Count() const {
    return args.size();
}

bool ApiCall::Has(size_t i) const {
    return i < args.size() && !args[i].is_null();
}

const json& ApiCall::At(size_t i) const {
    static const json kNull;
    return i < args.size() ? args[i] : kNull;
}

void ApiCall::Fail(const std::string& why) const {
    throw ApiError(std::string(function) + ": " + why);
}

int64_t ApiCall::Int(size_t i, const char* name, int64_t min, int64_t max) const {
    if (!Has(i)) {
        Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    const json& v = args[i];
    int64_t value = 0;
    if (v.is_number_integer()) {
        value = v.get<int64_t>();
    } else if (v.is_number_float() && std::floor(v.get<double>()) == v.get<double>() &&
               std::fabs(v.get<double>()) < 9.0e15) {
        value = (int64_t)v.get<double>(); // 10 / 2 is 5.0 in Lua
    } else {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypeInt) }));
    }
    if (value < min || value > max) {
        Fail(Tr(Msg::ApiArgRange, { Index(i), name, std::to_string(min), std::to_string(max) }));
    }
    return value;
}

int64_t ApiCall::IntOr(size_t i, const char* name, int64_t def, int64_t min, int64_t max) const {
    return Has(i) ? Int(i, name, min, max) : def;
}

double ApiCall::Number(size_t i, const char* name, double min, double max) const {
    if (!Has(i)) {
        Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    if (!args[i].is_number()) {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypeNumber) }));
    }
    double value = args[i].get<double>();
    if (!(value >= min && value <= max)) {
        auto shown = [](double d) {
            std::string s = std::to_string(d);
            s.erase(s.find_last_not_of('0') + 1);
            return s.back() == '.' ? s.substr(0, s.size() - 1) : s;
        };
        Fail(Tr(Msg::ApiArgRange, { Index(i), name, shown(min), shown(max) }));
    }
    return value;
}

double ApiCall::NumberOr(size_t i, const char* name, double def, double min, double max) const {
    return Has(i) ? Number(i, name, min, max) : def;
}

bool ApiCall::Bool(size_t i, const char* name) const {
    if (!Has(i)) {
        Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    if (!args[i].is_boolean()) {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypeBool) }));
    }
    return args[i].get<bool>();
}

bool ApiCall::BoolOr(size_t i, const char* name, bool def) const {
    return Has(i) ? Bool(i, name) : def;
}

std::string ApiCall::Str(size_t i, const char* name, size_t maxBytes) const {
    if (!Has(i)) {
        Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    if (!args[i].is_string()) {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypeText) }));
    }
    const std::string& text = args[i].get_ref<const std::string&>();
    if (text.size() > maxBytes) {
        Fail(Tr(Msg::ApiArgTooLong, { Index(i), name, std::to_string(maxBytes) }));
    }
    return text;
}

std::string ApiCall::StrOr(size_t i, const char* name, const std::string& def, size_t maxBytes) const {
    return Has(i) ? Str(i, name, maxBytes) : def;
}

json ApiCall::ObjectOr(size_t i, const char* name) const {
    if (!Has(i) || (args[i].is_array() && args[i].empty())) {
        return json::object(); // an empty Lua table arrives as an empty object, a plugin may send []
    }
    if (!args[i].is_object()) {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypeTable) }));
    }
    return args[i];
}

const char* ApiCall::Level(size_t i) const {
    std::string text = StrOr(i, "level", level::kInfo, 16);
    for (const char* known : { level::kInfo, level::kOk, level::kWarn, level::kError }) {
        if (text == known) {
            return known;
        }
    }
    Fail(Tr(Msg::ApiArgValue, { "level", "info, ok, warn, error" }));
}

RemoteClient* ApiCall::FindPlayer(size_t i, const char* name) const {
    if (!Has(i)) {
        Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    const json& v = args[i];
    RemoteClient* client = nullptr;
    if (v.is_number()) {
        client = server.Players().ById((uint8_t)Int(i, name, 0, 255));
    } else if (v.is_string()) {
        client = server.Players().ByNick(v.get<std::string>());
    } else {
        Fail(Tr(Msg::ApiArgType, { Index(i), name, Tr(Msg::ApiTypePlayer) }));
    }
    return (client != nullptr && !client->host) ? client : nullptr;
}

RemoteClient& ApiCall::Player(size_t i, const char* name) const {
    RemoteClient* client = FindPlayer(i, name);
    if (client == nullptr) {
        const json& v = args[i];
        Fail(Tr(Msg::ApiNoPlayer, { v.is_string() ? SanitizeChat(v.get<std::string>(), 40) : v.dump() }));
    }
    return *client;
}

namespace {

// The players an argument names: one, a list of them or "*" (everyone connected).
std::vector<RemoteClient*> Named(const ApiCall& call, size_t i, const char* name, bool onlyInWorld) {
    if (!call.Has(i)) {
        call.Fail(Tr(Msg::ApiArgMissing, { Index(i), name }));
    }
    std::vector<RemoteClient*> out;
    auto add = [&out, onlyInWorld](RemoteClient* c) {
        if ((!onlyInWorld || InWorld(*c)) && !c->closing && std::find(out.begin(), out.end(), c) == out.end()) {
            out.push_back(c);
        }
    };
    const json& v = call.args[i];
    if (v.is_string() && v.get<std::string>() == "*") {
        for (RemoteClient* c : call.server.Players().Welcomed()) {
            add(c);
        }
        return out;
    }
    if (!v.is_array()) {
        add(&call.Player(i, name));
        return out;
    }
    for (const json& one : v) { // each entry read as if it were the argument itself
        json single = json::array();
        single.get_ref<json::array_t&>().resize(i);
        single.push_back(one);
        ApiCall item{ call.server, call.host, call.mod, call.function, single };
        add(&item.Player(i, name));
    }
    return out;
}

} // namespace

std::vector<RemoteClient*> ApiCall::Targets(size_t i, const char* name) const {
    return Named(*this, i, name, true);
}

std::vector<RemoteClient*> ApiCall::Connected(size_t i, const char* name) const {
    return Named(*this, i, name, false);
}

double ApiCall::Field(const json& table, const char* key, double min, double max, const double* def) const {
    auto it = table.find(key);
    if (it == table.end() || it->is_null()) {
        if (def != nullptr) {
            return *def;
        }
        Fail(Tr(Msg::ApiFieldMissing, { key }));
    }
    if (!it->is_number() || !(it->get<double>() >= min && it->get<double>() <= max)) {
        Fail(Tr(Msg::ApiFieldRange, { key, std::to_string((int64_t)min), std::to_string((int64_t)max) }));
    }
    return it->get<double>();
}

} // namespace coop::server
