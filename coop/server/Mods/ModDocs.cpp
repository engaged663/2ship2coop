#include "ModDocs.h"

#include "GameIds.h"
#include "ModApi.h"
#include "ModEvents.h"

#include "common/ModRules.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace coop::server {

namespace {

// The functions each engine gives itself (they take functions as arguments, so they are not lines of the API table):
// coop.* in Lua; a plugin has the same ones as methods of coop::Plugin (PLUGINS.md).
struct CallbackApi {
    const char* name;
    const char* signature;
    const char* returns;
    const char* doc;
};

const CallbackApi kCallbackApis[] = {
    { "on", "event, fn", "number",
      "Listens to an event: one from the list below or a custom one between mods (with `:` in the name). `fn(e)` "
      "receives the event table; in a cancelable one, `return false` (or `e.cancel = true`) cancels it, and "
      "assigning a writable field (`e.text = \"...\"`) changes it. Returns the subscription id. A name without `:` "
      "that is not in the list is an error (so a misspelled one does not go unnoticed)." },
    { "off", "id", "boolean", "Stops listening: the id that `coop.on` returned." },
    { "emit", "name, payload?", "table, boolean",
      "Fires a custom event for other mods (the name contains `:`, like `\"economy:payment\"`) with the `payload` "
      "table. Returns the table as its handlers left it (they may change any field) and whether any of them "
      "cancelled it. Plugins can listen to these events, but only scripts fire them." },
    { "timer.after", "ms, fn", "number",
      "Calls `fn()` once, `ms` milliseconds from now (at least 10; at most 30 days). Returns the timer id." },
    { "timer.every", "ms, fn", "number",
      "Calls `fn()` every `ms` milliseconds (at least 10) until it is cancelled or the mod is unloaded. Returns its "
      "id." },
    { "timer.cancel", "id", "boolean", "Cancels a timer of this mod." },
    { "commands.register", "name, opts?, fn", "nothing",
      "Creates the command `/name`. `opts` = `{ usage = \"/name <x>\", help = \"...\", perm = \"player\" | \"op\" | "
      "\"console\", minArgs = 0, aliases = { \"other\" } }` (all optional; `perm` says who can use it and "
      "`commandPermissions` in `server.json` can change it). `fn(ctx, args)` receives who typed it (`ctx.player`, "
      "missing if it is the console; `ctx.nick`, `ctx.isConsole`, `ctx.isOp`) and its arguments (strings); what it "
      "returns is the reply: a text and, optionally, its level (`\"ok\"`, `\"warn\"`, `\"error\"`). `/help` shows "
      "it with its help. A name that already exists is an error." },
    { "commands.unregister", "name", "boolean", "Removes a command of this mod." },
};

// Where each namespace goes in the reference; one that is not here goes after these, by name.
const char* const kNamespaceOrder[] = { "server", "players", "chat", "world", "game", "storage", "mod" };

std::string NamespaceOf(const std::string& function) {
    return function.substr(0, function.find('.'));
}

size_t NamespaceRank(const std::string& space) {
    for (size_t i = 0; i < std::size(kNamespaceOrder); i++) {
        if (space == kNamespaceOrder[i]) {
            return i;
        }
    }
    return std::size(kNamespaceOrder);
}

void Function(std::string& md, const std::string& name, const std::string& signature, const std::string& returns,
              const std::string& doc) {
    md += "#### `coop." + name + "(" + signature + ")`\n\n";
    md += "Returns: " + returns + "\n\n";
    md += doc + "\n\n";
}

void Functions(std::string& md) {
    md += "## Functions\n\n";
    md += "In a script they are called `coop.<namespace>.<function>(...)`; in a plugin, "
          "`api.Call(\"<namespace>.<function>\", { ... })` with the same arguments in a list. An argument with `?` "
          "can be omitted (or passed as `nil`). If one is wrong, the function raises an error that starts with its "
          "name (in Lua, with the file and line; `pcall` catches it).\n\n";
    md += "Arguments that repeat:\n\n";
    md += "- **player**: a connected player, by id (number) or nick.\n";
    md += "- **target**: a player, a list of players or `\"*\"` (everyone). Commands to the game (`game.*`) only "
          "reach those playing in the server's game and return how many they reached.\n";
    md += "- **item**, **actor**, **scene**: a game id or its name from [IDS.md](IDS.md) (`\"MASK_BUNNY\"`, "
          "`\"EN_DODONGO\"`, `\"SOUTH_CLOCK_TOWN\"`).\n";
    md += "- **level**: `\"info\"` (if omitted), `\"ok\"`, `\"warn\"` or `\"error\"`: the color of the line in the "
          "chat.\n\n";

    md += "### Functions that take functions\n\n";
    md += "Each engine provides them: in a plugin they are methods of `coop::Plugin` (`On`, `Off`, `After`, "
          "`Every`, `CancelTimer`, `Command`; see [PLUGINS.md](PLUGINS.md)).\n\n";
    for (const CallbackApi& api : kCallbackApis) {
        Function(md, api.name, api.signature, api.returns, api.doc);
    }

    std::vector<const ApiDef*> apis = AllApis(); // by name
    std::stable_sort(apis.begin(), apis.end(), [](const ApiDef* a, const ApiDef* b) {
        std::string sa = NamespaceOf(a->name);
        std::string sb = NamespaceOf(b->name);
        return std::make_pair(NamespaceRank(sa), sa) < std::make_pair(NamespaceRank(sb), sb);
    });
    std::string space;
    for (const ApiDef* def : apis) {
        if (NamespaceOf(def->name) != space) {
            space = NamespaceOf(def->name);
            md += "### " + space + "\n\n";
        }
        Function(md, def->name, def->signature, def->returns, def->doc);
    }
}

void Events(std::string& md) {
    md += "## Events\n\n";
    md += "A script listens to an event with `coop.on(\"name\", function(e) ... end)` and a plugin with "
          "`api.On(\"name\", ...)`. The handler receives the event's fields (`e.nick`, `e.player`...); `player` is "
          "the player's id and is valid for the functions as long as they stay connected. In cancelable events, "
          "`return false` (or `e.cancel = true`) prevents what they announce, and writable fields are changed by "
          "assigning them. Handlers are called in the order they subscribed; if one cancels, the following ones do "
          "not receive it.\n\n";
    for (const ModEventDef& ev : ModEventDefs()) {
        md += "### `" + std::string(ev.name) + "`" + (ev.cancelable ? " (cancelable)" : "") + "\n\n";
        md += std::string(ev.doc) + "\n\n";
        if (ev.fields.empty()) {
            md += "No fields.\n\n";
            continue;
        }
        md += "| Field | Type | Writable | What it is |\n";
        md += "|---|---|---|---|\n";
        for (const ModFieldDef& f : ev.fields) {
            md += "| `" + f.name + "` | " + f.type + " | " + (f.writable ? "yes" : "") + " | " + f.doc + " |\n";
        }
        md += "\n";
    }
}

std::string Hex(int id) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%02X", id);
    return text;
}

void IdTable(std::string& md, ids::Kind kind, const char* title) {
    json table = ids::Table(kind); // {name: id}, by name
    std::vector<std::pair<int, std::string>> rows;
    for (const auto& entry : table.items()) {
        rows.push_back({ entry.value().get<int>(), entry.key() });
    }
    std::sort(rows.begin(), rows.end());
    md += std::string("## ") + title + "\n\n";
    md += "| Name | Id | Description |\n";
    md += "|---|---|---|\n";
    for (const auto& [id, name] : rows) {
        std::string about = ids::TitleOf(kind, id);
        if (kind == ids::Kind::Item && !mods::ItemGivable(id)) {
            about = "cannot be given or taken";
        }
        md += "| `" + name + "` | " + Hex(id) + " (" + std::to_string(id) + ") | " + about + " |\n";
    }
    md += "\n";
}

bool WriteFile(const std::filesystem::path& path, const std::string& text, std::string* err) {
    std::ofstream f(path, std::ios::binary); // "\n" as it is: the test compares the copies byte for byte
    f << text;
    if (!f) {
        *err = path.string();
        return false;
    }
    return true;
}

} // namespace

std::string ModApiMarkdown() {
    std::string md = "# Mod API reference\n\n";
    md += "> Generated by `2ship-coop-server --mod-docs <folder>` from the code. Do not edit by hand.\n\n";
    md += "Getting started guide: [README.md](README.md). Item, actor and scene names: [IDS.md](IDS.md). "
          "DLL plugins: [PLUGINS.md](PLUGINS.md).\n\n";
    Functions(md);
    Events(md);
    return md;
}

std::string ModIdsMarkdown() {
    std::string md = "# Game names: items, actors and scenes\n\n";
    md += "> Generated by `2ship-coop-server --mod-docs <folder>` from `coop/common/GameIds.inc`. Do not edit by "
          "hand.\n\n";
    md += "The `game.*` functions accept the name (case-insensitive, with or without the game prefix: "
          "`ITEM_`, `ACTOR_`, `SCENE_`) or the number. `coop.game.ids(\"item\")` (or `\"actor\"`, `\"scene\"`) "
          "returns the whole table and `coop.game.idOf` / `coop.game.nameOf` convert from one to the other. Sounds "
          "have no name: they are the `NA_SE_*` in `mm/include/sfx.h`.\n\n";
    IdTable(md, ids::Kind::Item, "Items");
    IdTable(md, ids::Kind::Actor, "Actors");
    IdTable(md, ids::Kind::Scene, "Scenes");
    return md;
}

bool WriteModDocs(const std::string& dir, std::string* err) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        *err = ec.message();
        return false;
    }
    std::filesystem::path base(dir);
    return WriteFile(base / "API.md", ModApiMarkdown(), err) && WriteFile(base / "IDS.md", ModIdsMarkdown(), err);
}

} // namespace coop::server
