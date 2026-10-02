// Mod API: game.* (orders for the games of the players in the server's world, the game settings the server forces on
// them, and the game's things by name). The orders travel as "mod" events; the game runs them in
// mm/2s2h/Coop/Mods/GameOps.cpp, and only while it plays in the server's world.
#include "server/Mods/GameIds.h"
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/ModRules.h"
#include "common/PlayerState.h"
#include "common/Text.h"

namespace coop::server {

namespace {

using ids::Kind;

constexpr size_t kMaxTextBytes = 65536; // what a text may bring before it is cut to kMaxModText characters

json Order(const char* op) {
    return json{ { "op", op } };
}

// To the games of the targets (the first argument); how many got it.
json Send(ApiCall& call, const json& op) {
    return call.host.SendOp(call.Targets(0), op);
}

std::string Position(size_t i) {
    return std::to_string(i + 1);
}

std::string Shown(const json& value) {
    return value.is_string() ? SanitizeChat(ToValidUtf8(value.get<std::string>()), 60)
                             : value.dump(-1, ' ', false, json::error_handler_t::replace);
}

// An item, an actor or a scene, by name or by id.
int IdArg(ApiCall& call, size_t i, const char* name, Kind kind) {
    if (!call.Has(i)) {
        call.Fail(Tr(Msg::ApiArgMissing, { Position(i), name }));
    }
    int id = ids::Find(kind, call.At(i));
    if (id < 0) {
        Msg what = kind == Kind::Item ? Msg::ApiBadItem : kind == Kind::Actor ? Msg::ApiBadActor : Msg::ApiBadScene;
        call.Fail(Tr(what, { Shown(call.At(i)) }));
    }
    return id;
}

Kind KindArg(ApiCall& call, size_t i) {
    Kind kind = Kind::Item;
    if (!ids::ParseKind(call.Str(i, "kind", 16), kind)) {
        call.Fail(Tr(Msg::ApiArgValue, { "kind", "item, actor, scene" }));
    }
    return kind;
}

// A text for a game's screen: valid UTF-8, no control characters but the line break, kMaxModText characters.
std::string TextArg(ApiCall& call, size_t i) {
    std::string text = SanitizeText(call.Str(i, "text", kMaxTextBytes), kMaxModText);
    if (text.empty()) {
        call.Fail(Tr(Msg::ApiArgEmpty, { Position(i), "text" }));
    }
    return text;
}

// Something the game gives or takes: the map's points, the ship's own entries and the ids its Item_Give has no case
// for are not (common/ModRules.h).
int ItemArg(ApiCall& call, size_t i) {
    int item = IdArg(call, i, "item", Kind::Item);
    if (!mods::ItemGivable(item)) {
        call.Fail(Tr(Msg::ApiNotGivable, { ids::NameOf(Kind::Item, item) }));
    }
    return item;
}

json GameNotify(ApiCall& call) {
    json op = Order("notify");
    op["text"] = TextArg(call, 1);
    op["seconds"] = call.IntOr(2, "seconds", 6, 1, 30);
    return Send(call, op);
}

json GameMessage(ApiCall& call) {
    json op = Order("message");
    op["text"] = TextArg(call, 1);
    return Send(call, op);
}

json GameSound(ApiCall& call) {
    json op = Order("sfx");
    op["id"] = call.Int(1, "sfx", 0, 0xFFFF);
    return Send(call, op);
}

json GameGiveItem(ApiCall& call) {
    json op = Order("item");
    op["id"] = ItemArg(call, 1);
    return Send(call, op);
}

json GameTakeItem(ApiCall& call) {
    json op = Order("take");
    op["id"] = ItemArg(call, 1);
    return Send(call, op);
}

json GameGiveRupees(ApiCall& call) {
    int64_t amount = call.Int(1, "amount", -kMaxModRupees, kMaxModRupees);
    if (amount == 0) {
        call.Fail(Tr(Msg::ApiArgNotZero, { Position(1), "amount" }));
    }
    json op = Order("rupees");
    op["amount"] = amount;
    return Send(call, op);
}

json GameHeal(ApiCall& call) {
    json op = Order("heal");
    op["amount"] = call.IntOr(1, "amount", 0, 0, kMaxModHealth); // 0: all of it
    return Send(call, op);
}

json GameDamage(ApiCall& call) {
    json op = Order("damage");
    op["amount"] = call.Int(1, "amount", 1, kMaxModHealth);
    return Send(call, op);
}

json GameKill(ApiCall& call) {
    return Send(call, Order("kill"));
}

json GameMagic(ApiCall& call) {
    json op = Order("magic");
    op["amount"] = call.IntOr(1, "amount", 0, -96, 96); // 0: fill it
    return Send(call, op);
}

json GameSpawn(ApiCall& call) {
    int actor = IdArg(call, 1, "actor", Kind::Actor);
    if (actor == 0) { // ACTOR_PLAYER: other Links are the co-op's own business
        call.Fail(Tr(Msg::ApiNoSpawn, { ids::NameOf(Kind::Actor, actor) }));
    }
    json opts = call.ObjectOr(2, "opts");
    const double zero = 0.0;
    const double ahead = 100.0;
    json op = Order("spawn");
    op["actor"] = actor;
    op["params"] = (int64_t)call.Field(opts, "params", -32768, 65535, &zero);
    op["rotY"] = (int64_t)call.Field(opts, "rotY", -32768, 65535, &zero);
    op["dist"] = (int64_t)call.Field(opts, "distance", 0, kMaxModSpawnDistance, &ahead);
    if (opts.contains("x") || opts.contains("y") || opts.contains("z")) { // an exact point: the three of them
        op["pos"] = { call.Field(opts, "x", -pose_limits::kWorldLimit, pose_limits::kWorldLimit),
                      call.Field(opts, "y", -pose_limits::kWorldLimit, pose_limits::kWorldLimit),
                      call.Field(opts, "z", -pose_limits::kWorldLimit, pose_limits::kWorldLimit) };
    }
    return Send(call, op);
}

json GameWarp(ApiCall& call) {
    int scene = IdArg(call, 1, "scene", Kind::Scene);
    int64_t spawn = call.IntOr(2, "spawn", 0, 0, 31);
    json op = Order("warp");
    op["entrance"] = ((int64_t)ids::EntranceSceneOf(scene) << 9) | (spawn << 4); // the game's Entrance_Create
    return Send(call, op);
}

json GameUnlockAll(ApiCall& call) {
    std::vector<RemoteClient*> targets = call.Targets(0);
    for (RemoteClient* target : targets) {
        json ev = MakeEvent(ev::kUnlockAll); // what /unlockall sends
        ev["nick"] = target->nick;
        call.server.SendEvent(*target, ev);
    }
    return (int)targets.size();
}

// Who a forced setting is for: 0 (everyone, "*") or the ids of the players named, wherever they play now.
std::vector<uint8_t> SettingOwners(ApiCall& call) {
    const json& target = call.At(0);
    if (target.is_string() && target.get<std::string>() == "*") {
        return { 0 };
    }
    std::vector<uint8_t> out;
    for (RemoteClient* player : call.Connected(0)) {
        out.push_back(player->id);
    }
    return out;
}

json GameSetSetting(ApiCall& call) {
    std::vector<uint8_t> owners = SettingOwners(call);
    std::string name = call.Str(1, "name", 256);
    if (!call.Has(2)) {
        call.Fail(Tr(Msg::ApiArgMissing, { Position(2), "value" }));
    }
    for (uint8_t owner : owners) {
        std::string err;
        if (!call.host.SetSetting(owner, name, call.At(2), &err)) {
            call.Fail(err);
        }
    }
    return nullptr;
}

json GameClearSetting(ApiCall& call) {
    std::vector<uint8_t> owners = SettingOwners(call);
    std::string name = call.Str(1, "name", 256);
    for (uint8_t owner : owners) {
        call.host.ClearSetting(owner, name);
    }
    return nullptr;
}

json GameSettingsOf(ApiCall& call) {
    return call.host.Settings().For(call.Has(0) ? call.Player(0).id : 0);
}

json GameIds(ApiCall& call) {
    return ids::Table(KindArg(call, 0));
}

json GameIdOf(ApiCall& call) {
    Kind kind = KindArg(call, 0);
    if (!call.Has(1)) {
        call.Fail(Tr(Msg::ApiArgMissing, { Position(1), "name" }));
    }
    int id = ids::Find(kind, call.At(1));
    return id >= 0 ? json(id) : json(nullptr);
}

json GameNameOf(ApiCall& call) {
    Kind kind = KindArg(call, 0);
    std::string name = ids::NameOf(kind, (int)call.Int(1, "id", -0x7FFFFFFF, 0x7FFFFFFF));
    return !name.empty() ? json(name) : json(nullptr);
}

} // namespace

// Every order: "target" is a player (id or nick), a list of players or "*"; only the ones playing in the server's
// world get it, and the function returns how many.
COOP_MOD_API(gameNotify, "game.notify", "target, text, seconds?", "número",
             "Muestra un aviso emergente (la notificación de 2 Ship, en una esquina de la pantalla) durante `seconds` "
             "segundos (de 1 a 30; 6 si no se indica). El texto se corta a 400 caracteres.",
             GameNotify);
COOP_MOD_API(gameMessage, "game.message", "target, text", "número",
             "Abre un cuadro de texto del juego con ese mensaje, como el de un cartel (admite saltos de línea; cada 4 "
             "líneas, un cuadro nuevo). La letra del juego no tiene tildes: se quitan (`á` → `a`, `ñ` → `n`, sin `¡ ¿`); "
             "`%r` `%g` `%b` `%y` `%p` `%w` cambian el color (rojo, verde, azul, amarillo, rosa, blanco). Si Link "
             "está ocupado (otro diálogo, una cinemática, el menú, a caballo), espera hasta 10 segundos a que quede "
             "libre; si no, el mensaje le llega como línea de chat.",
             GameMessage);
COOP_MOD_API(gameSound, "game.sound", "target, sfx", "número",
             "Reproduce un efecto de sonido del juego por su id (los `NA_SE_*` del juego, de 0 a 65535; por ejemplo "
             "`0x4802` es la melodía de acierto, `0x4803` una rupia y `0x4806` el de error).",
             GameSound);
COOP_MOD_API(gameGiveItem, "game.giveItem", "target, item", "número",
             "Da un objeto (por nombre, como `\"MASK_BUNNY\"`, o por id) como lo da el juego pero sin la animación del "
             "cofre: objetos, máscaras, contenidos de botella (llenan una vacía), equipo, munición, mejoras, canciones, "
             "corazones y rupias. Los pocos ids que el juego no sabe dar (`SWORD_DEITY`, `WALLET_DEFAULT`, "
             "`FISHING_ROD`, `STRAY_FAIRIES`, `INVALID_*`) dan error. Los nombres están en `IDS.md`.",
             GameGiveItem);
COOP_MOD_API(gameTakeItem, "game.takeItem", "target, item", "número",
             "Quita un objeto de la pantalla de objetos o de máscaras si lo tiene (un contenido de botella se lleva su "
             "botella). Con el resto (equipo, canciones, munición) no hace nada, y nunca quita la máscara que Link "
             "lleva puesta ni la de su forma.",
             GameTakeItem);
COOP_MOD_API(gameGiveRupees, "game.giveRupees", "target, amount", "número",
             "Da rupias, o las quita con un número negativo (de -9999 a 9999). Lo que no cabe en la cartera se pierde.",
             GameGiveRupees);
COOP_MOD_API(gameHeal, "game.heal", "target, amount?", "número",
             "Cura `amount` de vida (16 = un corazón). Sin `amount`, o con 0, cura toda.", GameHeal);
COOP_MOD_API(gameDamage, "game.damage", "target, amount", "número",
             "Quita `amount` de vida (16 = un corazón; la doble defensa lo reduce a la mitad), sin retroceso. Si se "
             "queda sin vida, Link muere como en el juego (un hada embotellada lo revive).",
             GameDamage);
COOP_MOD_API(gameKill, "game.kill", "target", "número", "Deja sin vida a esos jugadores.", GameKill);
COOP_MOD_API(gameMagic, "game.magic", "target, amount?", "número",
             "Da magia, o la quita con un número negativo (48 es la barra normal, 96 la doble). Sin `amount`, o con 0, "
             "la llena.",
             GameMagic);
COOP_MOD_API(gameSpawn, "game.spawn", "target, actor, opts?", "número",
             "Crea un actor (por nombre, como `\"EN_DODONGO\"`, o por id) en el juego de esos jugadores. `opts` = "
             "`{ params = 0, distance = 100, rotY = 0, x = ..., y = ..., z = ... }`: sin `x, y, z` aparece `distance` "
             "unidades delante de Link (de 0 a 2000). Cada juego crea el suyo: no se comparte con los demás jugadores. "
             "Se crea aunque la escena no use ese actor, pero un enemigo no aparece en una sala ya despejada y algunos "
             "actores solo funcionan en su propia escena (jefes, mecanismos).",
             GameSpawn);
COOP_MOD_API(gameWarp, "game.warp", "target, scene, spawn?", "número",
             "Lleva a esos jugadores a una escena (por nombre, como `\"SOUTH_CLOCK_TOWN\"`, o por id) por su entrada "
             "número `spawn` (de 0 a 31; 0 si no se indica). Si Link está en un diálogo o una cinemática, espera hasta "
             "10 segundos.",
             GameWarp);
COOP_MOD_API(gameUnlockAll, "game.unlockAll", "target", "número",
             "Da a esos jugadores todos los objetos, máscaras, canciones y corazones (lo que hace `/unlockall`).",
             GameUnlockAll);
COOP_MOD_API(gameSetSetting, "game.setSetting", "target, name, value", "nada",
             "Fuerza una opción de 2 Ship (un CVar de juego: `gEnhancements.*`, `gCheats.*`, `gModes.*`, `gFixes.*`) "
             "mientras se juegue en la partida del servidor; al salir, cada juego recupera la suya. Con `target` = "
             "`\"*\"` vale para todos, también para quien entre después; con un jugador o una lista, solo para ellos "
             "y hasta que se desconecten. `value`: un número entero para las opciones enteras (casillas: 1 o 0; listas), "
             "uno con decimales para los deslizadores (`2.0`, no `2`); `true` y `false` valen 1 y 0. Como mucho 64 "
             "opciones por jugador.",
             GameSetSetting);
COOP_MOD_API(gameClearSetting, "game.clearSetting", "target, name", "nada",
             "Deja de forzar esa opción: la de todos con `\"*\"`, o la propia de esos jugadores.", GameClearSetting);
COOP_MOD_API(gameSettings, "game.settings", "player?", "tabla",
             "Las opciones forzadas, `{ nombre = valor }`: sin argumentos, las de todos (incluidas las de "
             "`gameSettings` de `server.json`); con un jugador, todas las que tiene él.",
             GameSettingsOf);
COOP_MOD_API(gameIds, "game.ids", "kind", "tabla",
             "Todos los nombres de un tipo (`\"item\"`, `\"actor\"` o `\"scene\"`) con su id: `{ MASK_BUNNY = 57, ... }`.",
             GameIds);
COOP_MOD_API(gameIdOf, "game.idOf", "kind, name", "número o nil",
             "El id de un nombre, o `nil` si el juego no tiene nada con ese nombre. No distingue mayúsculas y admite el "
             "prefijo del juego (`ITEM_`, `ACTOR_`, `SCENE_`).",
             GameIdOf);
COOP_MOD_API(gameNameOf, "game.nameOf", "kind, id", "texto o nil", "El nombre de un id, o `nil` si no existe.",
             GameNameOf);

} // namespace coop::server
