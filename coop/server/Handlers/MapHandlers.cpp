// "map_pos": where a player is on their minimap (the map's texture id and a point on it), relayed to everyone else, so
// a game in another scene that shows the same map draws them (Clock Town's five scenes share one: game
// Features/CoopMinimap.cpp). Rate limited like "loc": a change over the budget waits, and only the latest one goes out.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/Text.h"

#include <cmath>
#include <cstdint>

namespace coop::server {

namespace {

bool ValidCoord(double v) {
    return std::isfinite(v) && v >= -kMaxMapCoord && v <= kMaxMapCoord;
}

void FlushMapPos(Server& server, RemoteClient& client) {
    if (!client.mapDirty || client.closing || client.host || !client.mapBudget.Take(server.NowMs())) {
        return;
    }
    client.mapDirty = false;
    json out = MakeEvent(ev::kMapPos);
    out["id"] = client.id;
    out["map"] = client.mapId;
    if (client.mapId >= 0) {
        out["x"] = client.mapX;
        out["y"] = client.mapY;
        out["yaw"] = client.mapYaw;
    }
    server.Broadcast(out, &client);
}

void OnMapPos(Server& server, RemoteClient& client, const json& ev) {
    if (client.host) {
        return; // the server's own games are nowhere anyone looks
    }
    int64_t map = GetInt(ev, "map", -1);
    double x = GetNumber(ev, "x");
    double y = GetNumber(ev, "y");
    int64_t yaw = GetInt(ev, "yaw");
    if (map < -1 || map > kMaxMapId || !ValidCoord(x) || !ValidCoord(y) || yaw < INT16_MIN || yaw > INT16_MAX) {
        server.NoteInvalid(client, Tr(Msg::InvMapPos));
        return;
    }
    client.mapId = (int32_t)map;
    client.mapX = map >= 0 ? x : 0.0;
    client.mapY = map >= 0 ? y : 0.0;
    client.mapYaw = map >= 0 ? (int16_t)yaw : 0;
    client.mapDirty = true; // also when nothing changed: the games' heartbeat tells newcomers where it is
    FlushMapPos(server, client);
}

void FlushPendingMapPos(Server& server) {
    for (RemoteClient* client : server.Players().Welcomed()) {
        FlushMapPos(server, *client);
    }
}

} // namespace

COOP_SERVER_EVENT(mapPos, ev::kMapPos, true, OnMapPos);
COOP_SERVER_ON_TICK(mapPosFlush, FlushPendingMapPos);

} // namespace coop::server
