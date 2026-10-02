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
COOP_MOD_API(gameNotify, "game.notify", "target, text, seconds?", "number",
             "Shows a pop-up notice (2 Ship's notification, in a corner of the screen) for `seconds` seconds (1 to "
             "30; 6 if omitted). The text is cut to 400 characters.",
             GameNotify);
COOP_MOD_API(gameMessage, "game.message", "target, text", "number",
             "Opens a game text box with that message, like a sign's (line breaks allowed; a new box every 4 "
             "lines). The game's font has no accents: they are removed (`á` → `a`, `ñ` → `n`, no `¡ ¿`); "
             "`%r` `%g` `%b` `%y` `%p` `%w` change the color (red, green, blue, yellow, pink, white). If Link is "
             "busy (another dialogue, a cutscene, the menu, on horseback), it waits up to 10 seconds for him to be "
             "free; otherwise the message reaches them as a chat line.",
             GameMessage);
COOP_MOD_API(gameSound, "game.sound", "target, sfx", "number",
             "Plays a game sound effect by its id (the game's `NA_SE_*`, from 0 to 65535; for example "
             "`0x4802` is the success jingle, `0x4803` a rupee and `0x4806` the error one).",
             GameSound);
COOP_MOD_API(gameGiveItem, "game.giveItem", "target, item", "number",
             "Gives an item (by name, such as `\"MASK_BUNNY\"`, or by id) the way the game does but without the chest "
             "animation: items, masks, bottle contents (they fill an empty one), equipment, ammo, upgrades, songs, "
             "hearts and rupees. The few ids the game cannot give (`SWORD_DEITY`, `WALLET_DEFAULT`, "
             "`FISHING_ROD`, `STRAY_FAIRIES`, `INVALID_*`) raise an error. The names are in `IDS.md`.",
             GameGiveItem);
COOP_MOD_API(gameTakeItem, "game.takeItem", "target, item", "number",
             "Takes an item from the items or masks screen if they have it (a bottle content takes its bottle "
             "with it). With the rest (equipment, songs, ammo) it does nothing, and it never takes the mask Link "
             "is wearing nor the one of his form.",
             GameTakeItem);
COOP_MOD_API(gameGiveRupees, "game.giveRupees", "target, amount", "number",
             "Gives rupees, or takes them with a negative number (-9999 to 9999). What does not fit in the wallet is "
             "lost.",
             GameGiveRupees);
COOP_MOD_API(gameHeal, "game.heal", "target, amount?", "number",
             "Heals `amount` of health (16 = one heart). Without `amount`, or with 0, heals fully.", GameHeal);
COOP_MOD_API(gameDamage, "game.damage", "target, amount", "number",
             "Takes `amount` of health (16 = one heart; double defense halves it), without knockback. If they run "
             "out of health, Link dies as in the game (a bottled fairy revives him).",
             GameDamage);
COOP_MOD_API(gameKill, "game.kill", "target", "number", "Takes all the health of those players.", GameKill);
COOP_MOD_API(gameMagic, "game.magic", "target, amount?", "number",
             "Gives magic, or takes it with a negative number (48 is the normal bar, 96 the double one). Without "
             "`amount`, or with 0, fills it.",
             GameMagic);
COOP_MOD_API(gameSpawn, "game.spawn", "target, actor, opts?", "number",
             "Creates an actor (by name, such as `\"EN_DODONGO\"`, or by id) in those players' games. `opts` = "
             "`{ params = 0, distance = 100, rotY = 0, x = ..., y = ..., z = ... }`: without `x, y, z` it appears "
             "`distance` units in front of Link (0 to 2000). Each game creates its own: it is not shared with the "
             "other players. It is created even if the scene does not use that actor, but an enemy does not appear "
             "in a room that is already cleared and some actors only work in their own scene (bosses, mechanisms).",
             GameSpawn);
COOP_MOD_API(gameWarp, "game.warp", "target, scene, spawn?", "number",
             "Takes those players to a scene (by name, such as `\"SOUTH_CLOCK_TOWN\"`, or by id) through its entrance "
             "number `spawn` (0 to 31; 0 if omitted). If Link is in a dialogue or a cutscene, it waits up to "
             "10 seconds.",
             GameWarp);
COOP_MOD_API(gameUnlockAll, "game.unlockAll", "target", "number",
             "Gives those players every item, mask, song and heart (what `/unlockall` does).",
             GameUnlockAll);
COOP_MOD_API(gameSetSetting, "game.setSetting", "target, name, value", "nothing",
             "Forces a 2 Ship option (a game CVar: `gEnhancements.*`, `gCheats.*`, `gModes.*`, `gFixes.*`) "
             "while playing in the server's game; on leaving, each game gets its own back. With `target` = "
             "`\"*\"` it applies to everyone, also to whoever enters later; with a player or a list, only to them "
             "and until they disconnect. `value`: an integer for integer options (checkboxes: 1 or 0; lists), "
             "one with decimals for sliders (`2.0`, not `2`); `true` and `false` count as 1 and 0. At most 64 "
             "options per player.",
             GameSetSetting);
COOP_MOD_API(gameClearSetting, "game.clearSetting", "target, name", "nothing",
             "Stops forcing that option: everyone's with `\"*\"`, or those players' own.", GameClearSetting);
COOP_MOD_API(gameSettings, "game.settings", "player?", "table",
             "The forced options, `{ name = value }`: without arguments, everyone's (including those from "
             "`gameSettings` in `server.json`); with a player, all the ones they have.",
             GameSettingsOf);
COOP_MOD_API(gameIds, "game.ids", "kind", "table",
             "All the names of a kind (`\"item\"`, `\"actor\"` or `\"scene\"`) with their id: `{ MASK_BUNNY = 57, ... }`.",
             GameIds);
COOP_MOD_API(gameIdOf, "game.idOf", "kind, name", "number or nil",
             "The id of a name, or `nil` if the game has nothing with that name. Case-insensitive, and it accepts the "
             "game prefix (`ITEM_`, `ACTOR_`, `SCENE_`).",
             GameIdOf);
COOP_MOD_API(gameNameOf, "game.nameOf", "kind, id", "text or nil", "The name of an id, or `nil` if it does not exist.",
             GameNameOf);

} // namespace coop::server
