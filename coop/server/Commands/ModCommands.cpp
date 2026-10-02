// /mods: the mods the server runs. /mod reload [name] (admins), /mod load <file> and /mod unload <name> (console).
#include "server/CommandRegistry.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void ListMods(CommandContext& ctx, const std::vector<std::string>&) {
    std::vector<Mod*> mods = ctx.server.Mods().All();
    if (mods.empty()) {
        ctx.Reply(Tr(Msg::ModsNone));
        return;
    }
    std::string text = Tr(Msg::ModsTitle, { std::to_string(mods.size()) });
    for (Mod* mod : mods) {
        const ModInfo& info = mod->Info();
        text += "\n" + info.name + " (" + info.kind + ")";
        if (!info.version.empty()) {
            text += " v" + info.version;
        }
        if (!info.title.empty() || !info.description.empty()) { // "Title: description", or whichever there is
            text += " - " + info.title + (!info.title.empty() && !info.description.empty() ? ": " : "") +
                    info.description;
        }
    }
    ctx.Reply(text);
}

void Reload(CommandContext& ctx, const std::vector<std::string>& args) {
    ModHost& mods = ctx.server.Mods();
    std::string err;
    if (args.size() >= 2) {
        if (mods.Reload(args[1], &err)) {
            ctx.Reply(Tr(Msg::ModReloaded, { ToLower(args[1]) }), level::kOk);
        } else {
            ctx.Reply(err, level::kError);
        }
        return;
    }
    std::vector<std::string> names; // every mod (their list changes while they reload)
    for (Mod* mod : mods.All()) {
        names.push_back(mod->Info().name);
    }
    int done = 0;
    for (const std::string& name : names) {
        if (mods.Reload(name, &err)) {
            done++;
        } else {
            ctx.Reply(Tr(Msg::ModLoadFailed, { name, err }), level::kError);
        }
    }
    ctx.Reply(Tr(Msg::ModReloadedAll, { std::to_string(done) }), level::kOk);
}

// What /mod load names: a file of the scripts' or the plugins' folder (its extension may be left out, as in the
// lists of server.json) or a path.
std::string FindModFile(Server& server, const std::string& entry) {
    if (entry.empty() || entry[0] == '*' || entry[0] == '!') {
        return entry; // the lists' own words mean nothing here
    }
    const ModsConfig& cfg = server.Config().mods;
    std::string ignored;
    std::vector<std::string> found = ResolveModFiles(cfg.scriptsDir, { entry }, { ".lua" }, &ignored);
    if (found.empty()) {
        found = ResolveModFiles(cfg.pluginsDir, { entry }, { ".dll", ".so" }, &ignored);
    }
    return found.empty() ? entry : found.front();
}

void Manage(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string action = args.empty() ? "" : ToLower(args[0]);
    if (action == "reload") {
        Reload(ctx, args);
        return;
    }
    if ((action != "load" && action != "unload") || args.size() < 2) {
        ctx.Reply(Tr(Msg::UsageLine, { Tr(Msg::ModUsage) }), level::kError);
        return;
    }
    if (!ctx.IsConsole()) { // which files run on the server's machine is its owner's call
        ctx.Reply(Tr(Msg::ModOnlyConsole), level::kError);
        return;
    }
    std::string err;
    if (action == "load") {
        std::string file = FindModFile(ctx.server, JoinFrom(args, 1)); // a path may have spaces
        if (!ctx.server.Mods().LoadFile(file, &err)) {
            ctx.Reply(Tr(Msg::ModLoadFailed, { file, err }), level::kError);
        }
    } else if (!ctx.server.Mods().Unload(args[1], &err)) {
        ctx.Reply(err, level::kError);
    }
}

} // namespace

COOP_COMMAND(mods, "mods", Msg::ModsUsage, Msg::ModsHelp, Perm::Player, 0, ListMods);
COOP_COMMAND(mod, "mod", Msg::ModUsage, Msg::ModHelp, Perm::Op, 0, Manage);

} // namespace coop::server
