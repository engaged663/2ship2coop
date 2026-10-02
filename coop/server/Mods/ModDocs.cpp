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
    { "on", "event, fn", "número",
      "Escucha un evento: uno de la lista de abajo o uno propio entre mods (con `:` en el nombre). `fn(e)` recibe la "
      "tabla del evento; en uno que se puede cancelar, `return false` (o `e.cancel = true`) lo cancela, y asignar un "
      "campo que se puede cambiar (`e.text = \"...\"`) lo cambia. Devuelve el id de la suscripción. Un nombre sin `:` "
      "que no está en la lista es un error (así no pasa desapercibido uno mal escrito)." },
    { "off", "id", "booleano", "Deja de escuchar: el id que devolvió `coop.on`." },
    { "emit", "name, payload?", "tabla, booleano",
      "Lanza un evento propio para otros mods (el nombre lleva `:`, como `\"economia:pago\"`) con la tabla `payload`. "
      "Devuelve la tabla tal como la dejaron sus manejadores (pueden cambiar cualquier campo) y si alguno lo canceló. "
      "Los plugins pueden escuchar estos eventos, pero solo los scripts los lanzan." },
    { "timer.after", "ms, fn", "número",
      "Llama a `fn()` una vez, dentro de `ms` milisegundos (como poco 10; como mucho 30 días). Devuelve el id del "
      "temporizador." },
    { "timer.every", "ms, fn", "número",
      "Llama a `fn()` cada `ms` milisegundos (como poco 10) hasta que se cancele o se descargue el mod. Devuelve su "
      "id." },
    { "timer.cancel", "id", "booleano", "Cancela un temporizador de este mod." },
    { "commands.register", "name, opts?, fn", "nada",
      "Crea el comando `/name`. `opts` = `{ usage = \"/name <x>\", help = \"...\", perm = \"player\" | \"op\" | "
      "\"console\", minArgs = 0, aliases = { \"otro\" } }` (todo opcional; `perm` dice quién puede usarlo y "
      "`commandPermissions` de `server.json` puede cambiarlo). `fn(ctx, args)` recibe quién lo escribe (`ctx.player`, "
      "que falta si es la consola; `ctx.nick`, `ctx.isConsole`, `ctx.isOp`) y sus argumentos (textos); lo que "
      "devuelve es la respuesta: un texto y, si quieres, su nivel (`\"ok\"`, `\"warn\"`, `\"error\"`). `/help` lo "
      "muestra con su ayuda. Un nombre que ya existe es un error." },
    { "commands.unregister", "name", "booleano", "Quita un comando de este mod." },
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
    md += "Devuelve: " + returns + "\n\n";
    md += doc + "\n\n";
}

void Functions(std::string& md) {
    md += "## Funciones\n\n";
    md += "En un script se llaman `coop.<espacio>.<función>(...)`; en un plugin, `api.Call(\"<espacio>.<función>\", "
          "{ ... })` con los mismos argumentos en una lista. Un argumento con `?` se puede omitir (o pasar `nil`). Si "
          "uno está mal, la función lanza un error que empieza por su nombre (en Lua, con el archivo y la línea; "
          "`pcall` lo atrapa).\n\n";
    md += "Argumentos que se repiten:\n\n";
    md += "- **player**: un jugador conectado, por su id (número) o su nick.\n";
    md += "- **target**: un jugador, una lista de jugadores o `\"*\"` (todos). Las órdenes al juego (`game.*`) solo "
          "llegan a los que juegan en la partida del servidor y devuelven a cuántos llegaron.\n";
    md += "- **item**, **actor**, **scene**: un id del juego o su nombre de [IDS.md](IDS.md) (`\"MASK_BUNNY\"`, "
          "`\"EN_DODONGO\"`, `\"SOUTH_CLOCK_TOWN\"`).\n";
    md += "- **level**: `\"info\"` (si se omite), `\"ok\"`, `\"warn\"` o `\"error\"`: el color de la línea en el "
          "chat.\n\n";

    md += "### Funciones que reciben funciones\n\n";
    md += "Las da cada motor: en un plugin son métodos de `coop::Plugin` (`On`, `Off`, `After`, `Every`, "
          "`CancelTimer`, `Command`; mira [PLUGINS.md](PLUGINS.md)).\n\n";
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
    md += "## Eventos\n\n";
    md += "Un script escucha un evento con `coop.on(\"nombre\", function(e) ... end)` y un plugin con "
          "`api.On(\"nombre\", ...)`. El manejador recibe los campos del evento (`e.nick`, `e.player`...); `player` es "
          "el id del jugador y vale para las funciones mientras siga conectado. En los eventos que se pueden cancelar, "
          "`return false` (o `e.cancel = true`) impide lo que anuncian, y los campos que se pueden cambiar se cambian "
          "asignándolos. Los manejadores se llaman en el orden en que se suscribieron; si uno cancela, los siguientes "
          "no lo reciben.\n\n";
    for (const ModEventDef& ev : ModEventDefs()) {
        md += "### `" + std::string(ev.name) + "`" + (ev.cancelable ? " (se puede cancelar)" : "") + "\n\n";
        md += std::string(ev.doc) + "\n\n";
        if (ev.fields.empty()) {
            md += "Sin campos.\n\n";
            continue;
        }
        md += "| Campo | Tipo | Se puede cambiar | Qué es |\n";
        md += "|---|---|---|---|\n";
        for (const ModFieldDef& f : ev.fields) {
            md += "| `" + f.name + "` | " + f.type + " | " + (f.writable ? "sí" : "") + " | " + f.doc + " |\n";
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
    md += "| Nombre | Id | Descripción |\n";
    md += "|---|---|---|\n";
    for (const auto& [id, name] : rows) {
        std::string about = ids::TitleOf(kind, id);
        if (kind == ids::Kind::Item && !mods::ItemGivable(id)) {
            about = "no se puede dar ni quitar";
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
    std::string md = "# Referencia de la API de mods\n\n";
    md += "> Generada por `2ship-coop-server --mod-docs <carpeta>` a partir del código. No la edites a mano.\n\n";
    md += "Guía para empezar: [README.md](README.md). Nombres de objetos, actores y escenas: [IDS.md](IDS.md). "
          "Plugins DLL: [PLUGINS.md](PLUGINS.md).\n\n";
    Functions(md);
    Events(md);
    return md;
}

std::string ModIdsMarkdown() {
    std::string md = "# Nombres del juego: objetos, actores y escenas\n\n";
    md += "> Generada por `2ship-coop-server --mod-docs <carpeta>` a partir de `coop/common/GameIds.inc`. No la "
          "edites a mano.\n\n";
    md += "Las funciones `game.*` aceptan el nombre (sin distinguir mayúsculas, con o sin el prefijo del juego: "
          "`ITEM_`, `ACTOR_`, `SCENE_`) o el número. `coop.game.ids(\"item\")` (o `\"actor\"`, `\"scene\"`) da la "
          "tabla entera y `coop.game.idOf` / `coop.game.nameOf` traducen de uno a otro. Los sonidos no tienen nombre: "
          "son los `NA_SE_*` de `mm/include/sfx.h`.\n\n";
    IdTable(md, ids::Kind::Item, "Objetos");
    IdTable(md, ids::Kind::Actor, "Actores");
    IdTable(md, ids::Kind::Scene, "Escenas");
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
