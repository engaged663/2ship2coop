// Mod API: mod.* (the mod that calls: its name, its settings in server.json, what /mods says of it).
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

json ModName(ApiCall& call) {
    return call.mod.Info().name;
}

json ModSetting(ApiCall& call) {
    json settings = call.host.SettingsOf(call.mod);
    auto it = settings.find(call.Str(0, "key", 128));
    return it != settings.end() ? *it : call.At(1);
}

json ModDescribe(ApiCall& call) {
    json info = call.ObjectOr(0, "info");
    ModInfo& mine = call.mod.EditInfo();
    auto text = [&info](const char* key, std::string& out) {
        auto it = info.find(key);
        if (it != info.end() && (it->is_string() || it->is_number())) {
            out = SanitizeChat(it->is_string() ? it->get<std::string>() : it->dump(), 200);
        }
    };
    text("title", mine.title);
    text("version", mine.version);
    text("author", mine.author);
    text("description", mine.description);
    return nullptr;
}

json ModList(ApiCall& call) {
    json out = json::array();
    for (Mod* mod : call.host.All()) {
        const ModInfo& info = mod->Info();
        out.push_back({ { "name", info.name },
                        { "kind", info.kind },
                        { "title", info.title },
                        { "version", info.version },
                        { "author", info.author },
                        { "description", info.description } });
    }
    return out;
}

} // namespace

COOP_MOD_API(modName, "mod.name", "", "text",
             "This mod's name: its file name, in lowercase and without extension (`mods/My Mod.lua` is `my_mod`).",
             ModName);
COOP_MOD_API(modSetting, "mod.setting", "key, default?", "value",
             "A setting of this mod written by the server owner in `server.json`, under "
             "`mods.settings.<mod name>`. If it is not there, returns `default`.",
             ModSetting);
COOP_MOD_API(modDescribe, "mod.describe", "info", "nothing",
             "Says what this mod is, for the `/mods` command: `{ title = \"...\", version = \"1.0\", author = \"...\", "
             "description = \"...\" }` (all optional).",
             ModDescribe);
COOP_MOD_API(modList, "mod.list", "", "list",
             "The loaded mods, in load order: `{ name, kind (\"lua\" or \"plugin\"), title, version, author, "
             "description }`.",
             ModList);

} // namespace coop::server
