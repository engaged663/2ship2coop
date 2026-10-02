#include "CommandRegistry.h"

#include "Mods/ModHost.h"
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

static std::map<std::string, Perm>& PermOverrides() {
    static std::map<std::string, Perm> table;
    return table;
}

std::string CommandContext::SenderName() const {
    return sender != nullptr ? sender->nick : Tr(Msg::ConsoleName);
}

void CommandContext::Reply(const std::string& text, const char* level) {
    if (capture != nullptr) {
        *capture += (capture->empty() ? "" : "\n") + text;
        return;
    }
    server.SendSystem(sender, text, level);
}

void RegisterCommand(const CommandDef& def) {
    CommandTable()[ToLower(def.name)] = def;
}

void RegisterAlias(const std::string& alias, const std::string& target) {
    AliasTable()[ToLower(alias)] = ToLower(target);
}

void UnregisterCommand(const std::string& name) {
    std::string key = ToLower(name);
    CommandTable().erase(key);
    for (auto it = AliasTable().begin(); it != AliasTable().end();) {
        it = it->second == key ? AliasTable().erase(it) : std::next(it);
    }
}

std::string UsageOf(const CommandDef& def) {
    return def.ownText ? def.usageText : Tr(def.usage);
}

std::string HelpOf(const CommandDef& def) {
    return def.ownText ? def.helpText : Tr(def.help);
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

void SetCommandPermissions(const std::map<std::string, Perm>& overrides) {
    PermOverrides() = overrides;
}

Perm EffectivePerm(const CommandDef& def) {
    auto it = PermOverrides().find(def.name);
    return it != PermOverrides().end() ? it->second : def.perm;
}

bool HasPermission(Server& server, RemoteClient* sender, const CommandDef& def) {
    return HasPermission(server, sender, EffectivePerm(def));
}

void ExecuteCommandLine(Server& server, RemoteClient* sender, const std::string& line, std::string* capture) {
    std::vector<std::string> tokens = SplitCommandLine(line);
    if (tokens.empty()) {
        return;
    }
    std::string name = tokens[0];
    if (!name.empty() && name[0] == '/') {
        name.erase(0, 1);
    }
    CommandContext ctx{ server, sender, capture };
    const CommandDef* def = FindCommand(name);
    if (def == nullptr) {
        ctx.Reply(Tr(Msg::UnknownCommand, { name }), level::kError);
        return;
    }
    if (!HasPermission(server, sender, *def)) {
        ctx.Reply(Tr(Msg::NoPermission, { def->name }), level::kError);
        return;
    }
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    if ((int)args.size() < def->minArgs) {
        ctx.Reply(Tr(Msg::UsageLine, { UsageOf(*def) }), level::kError);
        return;
    }
    if (sender != nullptr) {
        server.Log().Info(Tr(Msg::LogCommandUsed, { sender->nick, SanitizeChat(line, 120) }));
    }
    if (server.Mods().Wants(ModEvent::Command)) { // a mod may keep it from running
        json e = { { "player", sender != nullptr ? sender->id : 0 },
                   { "nick", ctx.SenderName() },
                   { "name", def->name },
                   { "args", args },
                   { "line", SanitizeChat(line, kCommandMaxChars) } };
        if (!server.Mods().Fire(ModEvent::Command, e)) {
            return;
        }
        def = FindCommand(name); // a handler may have removed it
        if (def == nullptr) {
            return;
        }
    }
    CommandFn fn = def->fn; // a mod's command may remove itself (or its mod) while it runs
    fn(ctx, args);
}

} // namespace coop::server
