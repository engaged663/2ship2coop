// Players' colors (common/PlayerColor.h): "color" C->S changes the sender's color; the others get it as S->C
// {id, color}. The first one travels in "hello" and everyone learns it from "welcome" (players[].color) and "join".
// Rate limited like "loc": a change over the budget waits, and only the latest one goes out when it refills.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/PlayerColor.h"
#include "common/Text.h"

namespace coop::server {

namespace {

void FlushColor(Server& server, RemoteClient& client) {
    if (!client.colorDirty || client.closing || client.host || !client.colorBudget.Take(server.NowMs())) {
        return;
    }
    client.colorDirty = false;
    json out = MakeEvent(ev::kColor);
    out["id"] = client.id;
    out["color"] = client.color;
    server.Broadcast(out, &client);
}

void OnColor(Server& server, RemoteClient& client, const json& ev) {
    if (client.host) {
        return; // the server's own games have no Link anyone sees
    }
    int64_t color = GetInt(ev, "color", kNoColor);
    if (!IsValidPackedColor(color)) {
        server.NoteInvalid(client, Tr(Msg::InvColor));
        return;
    }
    if (color == client.color) {
        return;
    }
    client.color = color;
    client.colorDirty = true;
    FlushColor(server, client);
}

void FlushPendingColors(Server& server) {
    for (RemoteClient* client : server.Players().Welcomed()) {
        FlushColor(server, *client);
    }
}

} // namespace

COOP_SERVER_EVENT(colorChange, ev::kColor, true, OnColor);
COOP_SERVER_ON_TICK(colorFlush, FlushPendingColors);

} // namespace coop::server
