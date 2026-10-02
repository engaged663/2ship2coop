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

COOP_MOD_API(modName, "mod.name", "", "texto",
             "El nombre de este mod: el de su archivo, en minúsculas y sin extensión (`mods/Mi Mod.lua` es `mi_mod`).",
             ModName);
COOP_MOD_API(modSetting, "mod.setting", "key, default?", "valor",
             "Un ajuste de este mod escrito por el dueño del servidor en `server.json`, dentro de "
             "`mods.settings.<nombre del mod>`. Si no está, devuelve `default`.",
             ModSetting);
COOP_MOD_API(modDescribe, "mod.describe", "info", "nada",
             "Dice qué es este mod para el comando `/mods`: `{ title = \"...\", version = \"1.0\", author = \"...\", "
             "description = \"...\" }` (todos opcionales).",
             ModDescribe);
COOP_MOD_API(modList, "mod.list", "", "lista",
             "Los mods cargados, en orden de carga: `{ name, kind (\"lua\" o \"plugin\"), title, version, author, "
             "description }`.",
             ModList);

} // namespace coop::server
