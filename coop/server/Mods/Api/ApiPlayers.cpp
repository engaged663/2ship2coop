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

COOP_MOD_API(playersList, "players.list", "", "list", "The ids of the connected players.", PlayersList);
COOP_MOD_API(playersCount, "players.count", "", "number", "How many players are connected.", PlayersCount);
COOP_MOD_API(playersGet, "players.get", "player", "table or nil",
             "A player's data (by id or nick), or `nil` if they are not connected: `id`, `nick`, `ip`, `op`, "
             "`scene`, `sceneKey`, `sceneName`, `room`, `entrance`, `x`, `y`, `z`, `rot`, `inWorld` (playing in the "
             "server's game), `busy` (in a cutscene), `group` (0: no group), `activity` (minigame key), "
             "`form` (0 Fierce Deity, 1 Goron, 2 Zora, 3 Deku, 4 human), `mask`, `connectedMs` and, once their game "
             "has reported them, `health`, `maxHealth`, `magic` and `rupees`.",
             PlayersGet);
COOP_MOD_API(playersFind, "players.find", "nick", "number or nil",
             "The id of the player with that nick (case-insensitive), or `nil`.", PlayersFind);
COOP_MOD_API(playersInScene, "players.inScene", "scene", "list",
             "The ids of the players who are in that scene (its id or its name, such as `\"SOUTH_CLOCK_TOWN\"`).",
             PlayersInScene);
COOP_MOD_API(playersInWorld, "players.inWorld", "", "list",
             "The ids of the players who are playing in the server's game.", PlayersInWorld);
COOP_MOD_API(playersNear, "players.near", "player, radius", "list",
             "The ids of the other players in the same scene who are within `radius` units (Link is about 60 tall).",
             PlayersNear);
COOP_MOD_API(playersKick, "players.kick", "player, reason?", "nothing", "Kicks a player from the server.",
             PlayersKick);
COOP_MOD_API(playersBan, "players.ban", "player, reason?", "nothing",
             "Bans the nick and IP of a connected player and kicks them (like `/ban`).", PlayersBan);
COOP_MOD_API(playersIsOp, "players.isOp", "player", "boolean", "Whether that player is an admin.", PlayersIsOp);
COOP_MOD_API(playersSetOp, "players.setOp", "player, op", "nothing",
             "Grants (`true`) or removes (`false`) admin permissions for a connected player. They are tied to "
             "their nick and to the IP they have now.",
             PlayersSetOp);
COOP_MOD_API(playersTeleport, "players.teleport", "player, dest", "nothing",
             "Takes a player next to another one (`dest` = id or nick, like `/tp`) or to an exact point: `dest` = "
             "`{ scene = \"SOUTH_CLOCK_TOWN\", x = 0, y = 0, z = 0, room = 0, rot = 0 }` (`scene` by name or id; "
             "`entrance` also works instead). If Link is in a dialogue or a cutscene, their game does not move them.",
             PlayersTeleport);

} // namespace coop::server
