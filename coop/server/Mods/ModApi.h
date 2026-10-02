#pragma once
// The functions of the mod API: what a script calls as coop.<namespace>.<function>(...) and a plugin as
// call("namespace.function", [...]). Arguments and results are JSON, so both kinds of mod share every function.
//
// New function = one function and one COOP_MOD_API line in a file of Mods/Api/ (the reference in
// coop/docs/mods/API.md is generated from these lines: 2ship-coop-server --mod-docs coop/docs/mods):
//   static json ChatTell(ApiCall& call) {
//       RemoteClient& to = call.Player(0);
//       call.server.SendSystem(&to, call.Str(1, "text"), call.Level(2));
//       return nullptr;
//   }
//   COOP_MOD_API(chatTell, "chat.tell", "player, text, level?", "nada", "Envía una línea...", ChatTell);
#include "Mod.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace coop::server {

class ModHost;
class Server;
struct RemoteClient;

// What a function throws for a call it cannot serve; its text already starts with the function's name.
struct ApiError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// One call: who calls, with what, and readers that throw ApiError for a missing or wrong argument.
// Arguments are counted from 0 here and from 1 in the error texts; `name` is how the reference calls them.
struct ApiCall {
    Server& server;
    ModHost& host;
    Mod& mod;             // who calls
    const char* function; // "chat.tell"
    const json& args;     // a list

    size_t Count() const;
    bool Has(size_t i) const;       // given and not null (nil)
    const json& At(size_t i) const; // null when missing
    int64_t Int(size_t i, const char* name, int64_t min, int64_t max) const; // 5.0 counts as 5
    int64_t IntOr(size_t i, const char* name, int64_t def, int64_t min, int64_t max) const;
    double Number(size_t i, const char* name, double min, double max) const;
    double NumberOr(size_t i, const char* name, double def, double min, double max) const;
    bool Bool(size_t i, const char* name) const;
    bool BoolOr(size_t i, const char* name, bool def) const;
    std::string Str(size_t i, const char* name, size_t maxBytes = 4096) const;
    std::string StrOr(size_t i, const char* name, const std::string& def, size_t maxBytes = 4096) const;
    json ObjectOr(size_t i, const char* name) const; // {} when missing; an empty list counts as {}
    const char* Level(size_t i) const;               // "info" (when missing), "ok", "warn" or "error"
    // The id or the nick of a connected player (never one of the server's own hosts).
    RemoteClient& Player(size_t i, const char* name = "player") const;
    RemoteClient* FindPlayer(size_t i, const char* name = "player") const; // nullptr when nobody is that player
    // A player, a list of players or "*" (everyone): the ones of them playing in the server's world.
    std::vector<RemoteClient*> Targets(size_t i, const char* name = "target") const;
    // The same, wherever they play (a game on its own save too).
    std::vector<RemoteClient*> Connected(size_t i, const char* name = "target") const;
    // A number of a table (an argument that is one), with its range. def = nullptr: it must be there.
    double Field(const json& table, const char* key, double min, double max, const double* def = nullptr) const;
    [[noreturn]] void Fail(const std::string& why) const; // ApiError("<function>: <why>")
};

using ApiFn = json (*)(ApiCall& call);

struct ApiDef {
    const char* name;      // "namespace.function"
    const char* signature; // "player, text, level?" ("?" = may be left out)
    const char* returns;   // for the reference (Spanish)
    const char* doc;       // for the reference (Spanish)
    ApiFn fn;
};

void RegisterApi(const ApiDef& def);
const ApiDef* FindApi(const std::string& name);
std::vector<const ApiDef*> AllApis(); // sorted by name

struct ApiRegistrar {
    explicit ApiRegistrar(const ApiDef& def) {
        RegisterApi(def);
    }
};

} // namespace coop::server

#define COOP_MOD_API(id, name, signature, returns, doc, fn) \
    static coop::server::ApiRegistrar id##_modapi(coop::server::ApiDef{ name, signature, returns, doc, fn })
