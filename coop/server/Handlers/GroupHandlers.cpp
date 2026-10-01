// Groups (spec 2026-09-30-coop-grupos-actividades §2): invitations expire, and whoever leaves the server's world or
// disconnects leaves its group at once (so a newcomer who gets its player id never inherits anything). The rules are
// in GroupBook, the texts and events in Groups.cpp, the commands in Commands/GroupCommands.cpp.
#include "server/Groups.h"
#include "server/Registry.h"
#include "server/Server.h"

#include <algorithm>
#include <vector>

namespace coop::server {

namespace {

// id leaves its group without a command of its own: its mates are told.
void LeaveQuietly(Server& server, uint8_t id, GroupChanges& ch) {
    RemoteClient* c = server.Players().ById(id);
    std::vector<uint8_t> mates = server.Groups().Mates(id);
    bool wasInGroup = server.Groups().GroupOf(id) != nullptr;
    server.Groups().Leave(id, ch);
    if (c == nullptr || !wasInGroup) {
        return;
    }
    server.Log().Info(Tr(Msg::LogGroupLeave, { c->nick }));
    for (uint8_t m : mates) {
        if (RemoteClient* mc = server.Players().ById(m)) {
            server.SendSystem(mc, Tr(Msg::GroupLeftOther, { c->nick }));
        }
    }
}

void TickGroups(Server& server) {
    GroupBook& book = server.Groups();
    GroupChanges ch;
    book.Expire(server.NowMs(), ch);
    std::vector<uint8_t> gone;
    auto check = [&](uint8_t id) {
        RemoteClient* c = server.Players().ById(id);
        if ((c == nullptr || !Groups_InWorld(*c)) && std::find(gone.begin(), gone.end(), id) == gone.end()) {
            gone.push_back(id);
        }
    };
    for (const Group& g : book.Groups()) {
        for (uint8_t m : g.members) {
            check(m);
        }
    }
    for (const Invitation& inv : book.Invites()) {
        check(inv.from);
        check(inv.to);
    }
    for (uint8_t id : gone) {
        LeaveQuietly(server, id, ch);
    }
    if (!ch.groups.empty() || !ch.removed.empty() || !ch.ended.empty()) {
        Groups_Publish(server, ch, 0);
    }
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (!client.welcomed || client.host) {
        return;
    }
    GroupChanges ch;
    LeaveQuietly(server, client.id, ch);
    Groups_Publish(server, ch, client.id);
}

} // namespace

COOP_SERVER_ON_TICK(groupsTick, TickGroups);
COOP_SERVER_ON_DISCONNECT(groupsDisconnect, OnDisconnect);

} // namespace coop::server
