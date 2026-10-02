#include "Groups.h"

#include "Mods/ModHost.h"
#include "Server.h"

#include <algorithm>
#include <string>

namespace coop::server {

bool Groups_Enabled(Server& server) {
    return server.Config().groups;
}

bool Groups_InWorld(const RemoteClient& c) {
    return c.welcomed && !c.closing && !c.host && c.inWorld;
}

std::vector<RemoteClient*> Groups_Mates(Server& server, const RemoteClient& c) {
    std::vector<RemoteClient*> out;
    if (!Groups_Enabled(server)) {
        return out;
    }
    for (uint8_t id : server.Groups().Mates(c.id)) {
        RemoteClient* m = server.Players().ById(id);
        if (m != nullptr && Groups_InWorld(*m)) {
            out.push_back(m);
        }
    }
    return out;
}

std::vector<RemoteClient*> Groups_MatesInScene(Server& server, const RemoteClient& c) {
    std::vector<RemoteClient*> out;
    for (RemoteClient* m : Groups_Mates(server, c)) {
        if (c.scene >= 0 && m->scene == c.scene) {
            out.push_back(m);
        }
    }
    return out;
}

json Groups_Event(Server& server, const Group* g) {
    json ev = MakeEvent(ev::kGroup);
    json members = json::array();
    std::string activity;
    std::string name;
    if (g != nullptr) {
        for (uint8_t id : g->members) {
            RemoteClient* m = server.Players().ById(id);
            members.push_back({ id, m != nullptr ? m->nick : std::string() });
        }
        if (RemoteClient* leader = server.Players().ById(g->Leader())) {
            activity = leader->activityKey;
            name = leader->activityName;
        }
    }
    ev["id"] = g != nullptr ? g->id : 0;
    ev["leader"] = g != nullptr ? g->Leader() : 0;
    ev["members"] = members;
    ev["activity"] = activity;
    ev["name"] = name;
    return ev;
}

void Groups_Publish(Server& server, const GroupChanges& ch, uint8_t cause) {
    GroupBook& book = server.Groups();
    if (server.Mods().Wants(ModEvent::GroupChange)) {
        for (uint32_t id : ch.groups) {
            if (const Group* g = book.Find(id)) {
                json e = { { "group", g->id }, { "leader", g->Leader() }, { "members", g->members } };
                server.Mods().Fire(ModEvent::GroupChange, e);
            }
        }
        for (uint32_t id : ch.dissolved) {
            json e = { { "group", id }, { "leader", 0 }, { "members", json::array() } };
            server.Mods().Fire(ModEvent::GroupChange, e);
        }
    }
    for (uint32_t id : ch.groups) {
        const Group* g = book.Find(id);
        if (g == nullptr) {
            continue; // dissolved in the same change
        }
        json ev = Groups_Event(server, g);
        for (uint8_t m : g->members) {
            if (RemoteClient* c = server.Players().ById(m)) {
                server.SendEvent(*c, ev);
            }
        }
    }
    for (uint8_t id : ch.removed) {
        if (book.GroupOf(id) != nullptr) {
            continue; // it went to another group: that group's event says so
        }
        RemoteClient* c = server.Players().ById(id);
        if (c == nullptr) {
            continue;
        }
        server.SendEvent(*c, Groups_Event(server, nullptr));
        bool lonely = std::find(ch.lonely.begin(), ch.lonely.end(), id) != ch.lonely.end();
        if (id != cause && !lonely) {
            server.SendSystem(c, Tr(Msg::GroupDissolved));
        }
    }
    for (const GroupChanges::Ended& e : ch.ended) {
        RemoteClient* to = server.Players().ById(e.invite.to);
        RemoteClient* from = server.Players().ById(e.invite.from);
        std::string fromNick = from != nullptr ? from->nick : std::string();
        std::string toNick = to != nullptr ? to->nick : std::string();
        std::string reason = e.reason;
        if (to != nullptr) {
            json ev = MakeEvent(ev::kInviteEnd);
            ev["from"] = e.invite.from;
            ev["nick"] = fromNick;
            ev["reason"] = reason;
            server.SendEvent(*to, ev);
        }
        if (reason == "expired") {
            if (to != nullptr && to->id != cause) {
                server.SendSystem(to, Tr(Msg::InviteExpiredYou, { fromNick }));
            }
            if (from != nullptr && from->id != cause) {
                server.SendSystem(from, Tr(Msg::InviteExpiredOther, { toNick }));
            }
        } else if (reason == "declined") {
            if (from != nullptr && from->id != cause) {
                server.SendSystem(from, Tr(Msg::InviteDeclinedOther, { toNick }));
            }
        } else if (reason == "full") {
            if (to != nullptr && to->id != cause) {
                server.SendSystem(to, Tr(Msg::InviteFullOther, { fromNick }));
            }
        }
    }
}

} // namespace coop::server
