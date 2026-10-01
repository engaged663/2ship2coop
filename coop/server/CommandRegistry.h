#pragma once
// Slash commands, shared by the server console and the in-game chat.
// Add a command = new file in server/Commands/ with:
//   static void Run(CommandContext& ctx, const std::vector<std::string>& args) { ... }
//   COOP_COMMAND(myCmd, "name", Msg::MyCmdUsage, Msg::MyCmdHelp, Perm::Player, /*minArgs*/ 1, Run);
// and the two texts (usage and help) in common/I18nMessages.inc, where they get their translations.
// Optional other name for the same command: COOP_ALIAS(myCmdAlias, "othername", "name");
// Nothing else needs to change (the game client just forwards the text).
#include "common/I18n.h"
#include "common/Protocol.h"

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
    RemoteClient* sender; // nullptr = server console

    bool IsConsole() const {
        return sender == nullptr;
    }
    std::string SenderName() const; // nick, or the translated "Server" for the console
    void Reply(const std::string& text, const char* level = level::kInfo);
};

using CommandFn = void (*)(CommandContext& ctx, const std::vector<std::string>& args);

struct CommandDef {
    std::string name; // lowercase, without '/'
    Msg usage; // translated when shown
    Msg help;
    Perm perm = Perm::Player;
    int minArgs = 0;
    CommandFn fn = nullptr;
};

void RegisterCommand(const CommandDef& def);
void RegisterAlias(const std::string& alias, const std::string& target);
const CommandDef* FindCommand(const std::string& name); // also finds aliases
std::vector<const CommandDef*> AllCommands();           // sorted by name, without aliases
bool HasPermission(Server& server, RemoteClient* sender, Perm perm);
// Parses "/name args..." (the '/' is optional) and runs it with permission and argument checks.
void ExecuteCommandLine(Server& server, RemoteClient* sender, const std::string& line);

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
