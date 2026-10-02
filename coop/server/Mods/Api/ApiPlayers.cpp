// Mod API: players.* (who is connected, where, and what an admin would do to them).
#include "server/Mods/GameIds.h"
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/PlayerState.h"
#include "common/Text.h"

#include <cmath>

namespace coop::server {

namespace {

// What a script sees of a player (players.get).
json Snapshot(Server& server, const RemoteClient& c) {
    const Group* group = server.Groups().GroupOf(c.id);
    json out = { { "id", c.id },
             { "nick", c.nick },
             { "ip", c.ip },
             { "op", server.IsOp(c) },
             { "scene", c.scene },
             { "sceneKey", ids::NameOf(ids::Kind::Scene, c.scene) },
             { "sceneName", c.sceneName },
             { "room", c.room },
             { "entrance", c.entrance },
             { "x", c.pos[0] },
             { "y", c.pos[1] },
             { "z", c.pos[2] },
             { "rot", c.rotY },
             { "inWorld", c.inWorld },
             { "busy", c.busy },
             { "group", group != nullptr ? group->id : 0 },
             { "activity", c.activityKey },
             { "form", c.form },
             { "mask", c.mask },
             { "connectedMs", server.NowMs() - c.connectedAtMs } };
    if (c.hasStats) { // what its game last reported (Handlers/ModHandlers.cpp)
        out["health"] = c.health;
        out["maxHealth"] = c.maxHealth;
        out["magic"] = c.magic;
        out["rupees"] = c.rupees;
    }
    return out;
}

// A scene by its name ("SOUTH_CLOCK_TOWN", "clocktower") or its id.
int SceneOf(ApiCall& call, const json& value) {
    int scene = ids::Find(ids::Kind::Scene, value);
    if (scene < 0) {
        call.Fail(Tr(Msg::ApiBadScene, { value.is_string() ? SanitizeChat(value.get<std::string>(), 60)
                                                           : value.dump(-1, ' ', false, json::error_handler_t::replace) }));
    }
    return scene;
}

json Ids(const std::vector<RemoteClient*>& players) {
    json out = json::array();
    for (const RemoteClient* p : players) {
        out.push_back(p->id);
    }
    return out;
}

json PlayersList(ApiCall& call) {
    return Ids(call.server.Players().Welcomed());
}

json PlayersCount(ApiCall& call) {
    return call.server.Players().WelcomedCount();
}

json PlayersGet(ApiCall& call) {
    RemoteClient* c = call.FindPlayer(0);
    return c != nullptr ? Snapshot(call.server, *c) : json(nullptr);
}

json PlayersFind(ApiCall& call) {
    RemoteClient* c = call.server.Players().ByNick(call.Str(0, "nick", 64));
    return (c != nullptr && !c->host) ? json(c->id) : json(nullptr);
}

json PlayersInScene(ApiCall& call) {
    // A name must be one of the game's scenes; a number is taken as it comes (nobody is in a scene that is not).
    int64_t scene = call.At(0).is_string() ? SceneOf(call, call.At(0)) : call.Int(0, "scene", 0, 0x7FFF);
    std::vector<RemoteClient*> in;
    for (RemoteClient* p : call.server.Players().Welcomed()) {
        if (p->scene == scene) {
            in.push_back(p);
        }
    }
    return Ids(in);
}

json PlayersInWorld(ApiCall& call) {
    std::vector<RemoteClient*> in;
    for (RemoteClient* p : call.server.Players().Welcomed()) {
        if (p->inWorld && !p->closing) {
            in.push_back(p);
        }
    }
    return Ids(in);
}

json PlayersNear(ApiCall& call) {
    RemoteClient& center = call.Player(0);
    double radius = call.Number(1, "radius", 0.0, 1.0e6);
    std::vector<RemoteClient*> near;
    for (RemoteClient* p : call.server.Players().Welcomed()) {
        if (p == &center || !p->hasState || !center.hasState || p->scene != center.scene || center.scene < 0) {
            continue;
        }
        double dx = p->pos[0] - center.pos[0];
        double dy = p->pos[1] - center.pos[1];
        double dz = p->pos[2] - center.pos[2];
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= radius) {
            near.push_back(p);
        }
    }
    return Ids(near);
}

json PlayersKick(ApiCall& call) {
    RemoteClient& target = call.Player(0);
    call.server.Kick(target, SanitizeChat(call.StrOr(1, "reason", ""), kChatMaxChars));
    return nullptr;
}

json PlayersBan(ApiCall& call) {
    RemoteClient& target = call.Player(0);
    std::string reason = SanitizeChat(call.StrOr(1, "reason", ""), kChatMaxChars);
    call.server.Access().Ban(target.nick, target.ip, reason);
    call.server.Kick(target, reason.empty() ? Tr(Msg::BanKickReason) : Tr(Msg::BanKickReasonText, { reason }));
    return nullptr;
}

json PlayersIsOp(ApiCall& call) {
    return call.server.IsOp(call.Player(0));
}

json PlayersSetOp(ApiCall& call) {
    RemoteClient& target = call.Player(0);
    if (call.Bool(1, "op")) {
        if (call.server.Access().AddOp(target.nick, target.ip)) {
            call.server.SendSystem(&target, Tr(Msg::OpNotice), level::kOk);
        }
    } else if (call.server.Access().RemoveOp(target.nick)) {
        call.server.SendSystem(&target, Tr(Msg::DeopNotice), level::kWarn);
    }
    return nullptr;
}

json PlayersTeleport(ApiCall& call) {
    RemoteClient& mover = call.Player(0);
    json tp = MakeEvent(ev::kTp);
    const json& dest = call.At(1);
    if (dest.is_object()) { // an exact place: {scene | entrance, room?, x, y, z, rot?}
        const double zero = 0.0;
        auto named = dest.find("scene");
        if (named != dest.end() && !named->is_null()) {
            int scene = SceneOf(call, *named);
            tp["scene"] = scene; // already there: its game moves Link without loading anything
            tp["entrance"] = ids::EntranceSceneOf(scene) << 9;
        } else {
            int64_t entrance = (int64_t)call.Field(dest, "entrance", 0, 0xFFFF);
            if ((entrance >> 9) >= pose_limits::kEntranceScenes) {
                call.Fail(Tr(Msg::ApiFieldRange,
                             { "entrance", "0", std::to_string((pose_limits::kEntranceScenes << 9) - 1) }));
            }
            tp["scene"] = -1; // the game loads the scene of that entrance
            tp["entrance"] = entrance;
        }
        tp["room"] = (int64_t)call.Field(dest, "room", 0, 127, &zero);
        tp["pos"] = { call.Field(dest, "x", -pose_limits::kWorldLimit, pose_limits::kWorldLimit),
                      call.Field(dest, "y", -pose_limits::kWorldLimit, pose_limits::kWorldLimit),
                      call.Field(dest, "z", -pose_limits::kWorldLimit, pose_limits::kWorldLimit) };
        tp["rot"] = (int64_t)call.Field(dest, "rot", -32768, 65535, &zero);
        tp["target"] = "";
    } else { // next to another player, as /tp does
        RemoteClient& target = call.Player(1, "dest");
        if (&target == &mover) {
            call.Fail(Tr(Msg::TpSelf));
        }
        if (!target.hasState || target.scene < 0) {
            call.Fail(Tr(Msg::TpNotInGame, { target.nick }));
        }
        tp["scene"] = target.scene;
        tp["entrance"] = target.entrance;
        tp["room"] = target.room;
        tp["pos"] = { target.pos[0], target.pos[1], target.pos[2] };
        tp["rot"] = target.rotY;
        tp["target"] = target.nick;
    }
    call.server.SendEvent(mover, tp);
    return nullptr;
}

} // namespace

COOP_MOD_API(playersList, "players.list", "", "lista", "Los ids de los jugadores conectados.", PlayersList);
COOP_MOD_API(playersCount, "players.count", "", "número", "Cuántos jugadores hay conectados.", PlayersCount);
COOP_MOD_API(playersGet, "players.get", "player", "tabla o nil",
             "Los datos de un jugador (por id o por nick), o `nil` si no está conectado: `id`, `nick`, `ip`, `op`, "
             "`scene`, `sceneKey`, `sceneName`, `room`, `entrance`, `x`, `y`, `z`, `rot`, `inWorld` (juega en la "
             "partida del servidor), `busy` (en una cinemática), `group` (0: sin grupo), `activity` (clave del "
             "minijuego), `form` (0 Deidad Fiera, 1 Goron, 2 Zora, 3 Deku, 4 humano), `mask`, `connectedMs` y, cuando "
             "su juego los ha comunicado, `health`, `maxHealth`, `magic` y `rupees`.",
             PlayersGet);
COOP_MOD_API(playersFind, "players.find", "nick", "número o nil",
             "El id del jugador con ese nick (sin distinguir mayúsculas), o `nil`.", PlayersFind);
COOP_MOD_API(playersInScene, "players.inScene", "scene", "lista",
             "Los ids de los jugadores que están en esa escena (su id o su nombre, como `\"SOUTH_CLOCK_TOWN\"`).",
             PlayersInScene);
COOP_MOD_API(playersInWorld, "players.inWorld", "", "lista",
             "Los ids de los jugadores que están jugando en la partida del servidor.", PlayersInWorld);
COOP_MOD_API(playersNear, "players.near", "player, radius", "lista",
             "Los ids de los otros jugadores de su misma escena que están a `radius` unidades o menos (un Link mide "
             "unas 60 de alto).",
             PlayersNear);
COOP_MOD_API(playersKick, "players.kick", "player, reason?", "nada", "Expulsa a un jugador del servidor.", PlayersKick);
COOP_MOD_API(playersBan, "players.ban", "player, reason?", "nada",
             "Banea el nick y la IP de un jugador conectado y lo expulsa (como `/ban`).", PlayersBan);
COOP_MOD_API(playersIsOp, "players.isOp", "player", "booleano", "Si ese jugador es administrador.", PlayersIsOp);
COOP_MOD_API(playersSetOp, "players.setOp", "player, op", "nada",
             "Da (`true`) o quita (`false`) los permisos de administrador a un jugador conectado. Quedan ligados a "
             "su nick y a la IP que tiene ahora.",
             PlayersSetOp);
COOP_MOD_API(playersTeleport, "players.teleport", "player, dest", "nada",
             "Lleva a un jugador junto a otro (`dest` = id o nick, como `/tp`) o a un punto exacto: `dest` = "
             "`{ scene = \"SOUTH_CLOCK_TOWN\", x = 0, y = 0, z = 0, room = 0, rot = 0 }` (`scene` por nombre o id; "
             "también vale `entrance` en su lugar). Si Link está en un diálogo o una cinemática, su juego no lo mueve.",
             PlayersTeleport);

} // namespace coop::server
