// Shared world events (sub-project B): entering/leaving the server's world, creating it, world changes (wops)
// and inventory uploads. The logic lives in World/SharedWorld.cpp.
#include "server/Registry.h"
#include "server/Server.h"

namespace coop::server {

namespace {

// Entering sends the whole world back and both tell everyone in the world: a client toggling them in a loop is
// cut off (and each extra request counts as invalid, so a flood gets it kicked).
bool TakeEntry(Server& server, RemoteClient& client) {
    if (client.worldEntryBudget.Take(server.NowMs())) {
        return true;
    }
    server.NoteInvalid(client, Tr(Msg::InvWorldFlood));
    return false;
}

void OnEnter(Server& server, RemoteClient& client, const json&) {
    if (TakeEntry(server, client)) {
        server.World().Enter(client);
    }
}

void OnLeave(Server& server, RemoteClient& client, const json&) {
    if (TakeEntry(server, client)) {
        server.World().Leave(client, false);
    }
}

void OnInit(Server& server, RemoteClient& client, const json& ev) {
    server.World().Create(client, ev);
}

void OnWops(Server& server, RemoteClient& client, const json& ev) {
    server.World().ApplyOps(client, ev);
}

void OnInv(Server& server, RemoteClient& client, const json& ev) {
    server.World().Upload(client, ev);
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (client.welcomed) {
        server.World().Leave(client, true);
    }
}

void OnTick(Server& server) {
    server.World().Tick();
}

} // namespace

COOP_SERVER_EVENT(worldEnter, ev::kWorldEnter, true, OnEnter);
COOP_SERVER_EVENT(worldLeave, ev::kWorldLeave, true, OnLeave);
COOP_SERVER_EVENT(worldInit, ev::kWorldInit, true, OnInit);
COOP_SERVER_EVENT(worldOps, ev::kWops, true, OnWops);
COOP_SERVER_EVENT(worldInv, ev::kInv, true, OnInv);
COOP_SERVER_ON_DISCONNECT(worldDisconnect, OnDisconnect);
COOP_SERVER_ON_TICK(worldTick, OnTick);

} // namespace coop::server
