#include "CommandRegistry.h"

#include "Server.h"

#include "common/Text.h"

#include <algorithm>
#include <map>

namespace coop::server {

static std::map<std::string, CommandDef>& CommandTable() {
    static std::map<std::string, CommandDef> table;
    return table;
}

static std::map<std::string, std::string>& AliasTable() {
    static std::map<std::string, std::string> table;
    return table;
}

std::string CommandContext::SenderName() const {
    return sender != nullptr ? sender->nick : Tr(Msg::ConsoleName);
}

void CommandContext::Reply(const std::string& text, const char* level) {
    server.SendSystem(sender, text, level);
}

void RegisterCommand(const CommandDef& def) {
    CommandTable()[ToLower(def.name)] = def;
}

void RegisterAlias(const std::string& alias, const std::string& target) {
    AliasTable()[ToLower(alias)] = ToLower(target);
}

const CommandDef* FindCommand(const std::string& name) {
    std::string key = ToLower(name);
    auto alias = AliasTable().find(key);
    if (alias != AliasTable().end()) {
        key = alias->second;
    }
    auto it = CommandTable().find(key);
    return it != CommandTable().end() ? &it->second : nullptr;
}

std::vector<const CommandDef*> AllCommands() {
    std::vector<const CommandDef*> out;
    for (auto& entry : CommandTable()) {
        out.push_back(&entry.second);
    }
    return out; // std::map keeps them sorted by name
}

bool HasPermission(Server& server, RemoteClient* sender, Perm perm) {
    if (sender == nullptr) {
        return true;
    }
    switch (perm) {
        case Perm::Player:
            return true;
        case Perm::Op:
            return server.IsOp(*sender);
        case Perm::Console:
        default:
            return false;
    }
}

void ExecuteCommandLine(Server& server, RemoteClient* sender, const std::string& line) {
    std::vector<std::string> tokens = SplitCommandLine(line);
    if (tokens.empty()) {
        return;
    }
    std::string name = tokens[0];
    if (!name.empty() && name[0] == '/') {
        name.erase(0, 1);
    }
    CommandContext ctx{ server, sender };
    const CommandDef* def = FindCommand(name);
    if (def == nullptr) {
        ctx.Reply(Tr(Msg::UnknownCommand, { name }), level::kError);
        return;
    }
    if (!HasPermission(server, sender, def->perm)) {
        ctx.Reply(Tr(Msg::NoPermission, { def->name }), level::kError);
        return;
    }
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    if ((int)args.size() < def->minArgs) {
        ctx.Reply(Tr(Msg::UsageLine, { Tr(def->usage) }), level::kError);
        return;
    }
    if (sender != nullptr) {
        server.Log().Info(Tr(Msg::LogCommandUsed, { sender->nick, SanitizeChat(line, 120) }));
    }
    def->fn(ctx, args);
}

} // namespace coop::server
