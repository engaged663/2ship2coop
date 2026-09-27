// Songs and cycle events from the games: Song of Double Time, Inverted Song of Time, Song of Time (vote) and the
// new cycle computed by one game. They share the chat/command rate limit. Logic: World/SharedWorldCycle.cpp.
#include "server/Registry.h"
#include "server/Server.h"

namespace coop::server {

namespace {

void OnJump(Server& server, RemoteClient& client, const json&) {
    if (server.AllowMessage(client)) {
        server.World().RequestJump(client);
    }
}

void OnSpeed(Server& server, RemoteClient& client, const json& ev) {
    if (server.AllowMessage(client)) {
        server.World().RequestSpeed(client, GetBool(ev, "inv"));
    }
}

void OnSotPropose(Server& server, RemoteClient& client, const json&) {
    if (server.AllowMessage(client)) {
        server.World().ProposeSot(client);
    }
}

void OnCycleResult(Server& server, RemoteClient& client, const json& ev) {
    server.World().CycleResult(client, ev);
}

} // namespace

COOP_SERVER_EVENT(clockJump, ev::kClockJump, true, OnJump);
COOP_SERVER_EVENT(clockSpeed, ev::kClockSpeed, true, OnSpeed);
COOP_SERVER_EVENT(clockSot, ev::kSotPropose, true, OnSotPropose);
COOP_SERVER_EVENT(clockCycleResult, ev::kCycleResult, true, OnCycleResult);

} // namespace coop::server
