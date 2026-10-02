#pragma once
// Slash commands, shared by the server console and the in-game chat.
// Add a command = new file in server/Commands/ with:
//   static void Run(CommandContext& ctx, const std::vector<std::string>& args) { ... }
//   COOP_COMMAND(myCmd, "name", Msg::MyCmdUsage, Msg::MyCmdHelp, Perm::Player, /*minArgs*/ 1, Run);
// and the two texts (usage and help) in common/I18nMessages.inc, where they get their translations.
// Optional other name for the same command: COOP_ALIAS(myCmdAlias, "othername", "name");
// Nothing else needs to change (the game client just forwards the text).
// Mods add and remove commands while the server runs (Mods/ModHost.h: AddCommand), with their own texts.
#include "common/I18n.h"
#include "common/Protocol.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace coop::server {

class Server;
struct RemoteClient;

enum class Perm {
    Player,  // everyone
    Op,      // ops (ops.json) and the console
    Console, // console only
};

struct CommandContext {
    Server& server;
    RemoteClient* sender;           // nullptr = server console
    std::string* capture = nullptr; // set: the replies are gathered here instead of being sent (mods: server.exec)

    bool IsConsole() const {
        return sender == nullptr;
    }
    std::string SenderName() const; // nick, or the translated "Server" for the console
    void Reply(const std::string& text, const char* level = level::kInfo);
};

using CommandFn = std::function<void(CommandContext& ctx, const std::vector<std::string>& args)>;

struct CommandDef {
    std::string name; // lowercase, without '/'
    Msg usage{};      // translated when shown
    Msg help{};
    Perm perm = Perm::Player;
    int minArgs = 0;
    CommandFn fn;
    // A mod's command says its own texts, as they are (never translated).
    bool ownText = false;
    std::string usageText;
    std::string helpText;
};

void RegisterCommand(const CommandDef& def);
void RegisterAlias(const std::string& alias, const std::string& target);
void UnregisterCommand(const std::string& name);        // and the aliases that point to it
const CommandDef* FindCommand(const std::string& name); // also finds aliases
std::vector<const CommandDef*> AllCommands();           // sorted by name, without aliases
std::string UsageOf(const CommandDef& def);             // in the server's language, or the mod's own text
std::string HelpOf(const CommandDef& def);
bool HasPermission(Server& server, RemoteClient* sender, Perm perm);
// server.json "commandPermissions": who may use a command instead of what its file says. Replaces the previous ones
// (every server sets its own when it starts).
void SetCommandPermissions(const std::map<std::string, Perm>& overrides);
Perm EffectivePerm(const CommandDef& def);
bool HasPermission(Server& server, RemoteClient* sender, const CommandDef& def); // with EffectivePerm
// Parses "/name args..." (the '/' is optional) and runs it with permission and argument checks.
// With capture, what the command answers is appended to it (one line per reply) instead of being sent or logged.
void ExecuteCommandLine(Server& server, RemoteClient* sender, const std::string& line,
                        std::string* capture = nullptr);

struct CommandRegistrar {
    explicit CommandRegistrar(const CommandDef& def) {
        RegisterCommand(def);
    }
    CommandRegistrar(const char* alias, const char* target) {
        RegisterAlias(alias, target);
    }
};

} // namespace coop::server

#define COOP_COMMAND(id, name, usage, help, perm, minArgs, fn) \
    static coop::server::CommandRegistrar id##_command(coop::server::CommandDef{ name, usage, help, perm, minArgs, fn })

#define COOP_ALIAS(id, alias, target) static coop::server::CommandRegistrar id##_alias(alias, target)
