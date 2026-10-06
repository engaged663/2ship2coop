#include "Rooms.h"

#include "Groups.h"
#include "Server.h"

#include "common/I18n.h"
#include "common/Text.h"

#include <algorithm>
#include <string>

namespace coop::server {

namespace {

const char* const kModes[] = { "together", "each", "turns", "shared" };

void AddUnique(std::vector<RemoteClient*>& v, RemoteClient* c) {
    if (c != nullptr && std::find(v.begin(), v.end(), c) == v.end()) {
        v.push_back(c);
    }
}

void AddUniqueId(std::vector<uint32_t>& v, uint32_t id) {
    if (std::find(v.begin(), v.end(), id) == v.end()) {
        v.push_back(id);
    }
}

std::string NickOf(Server& server, uint8_t id) {
    RemoteClient* c = server.Players().ById(id);
    return c != nullptr ? c->nick : std::string("?");
}

void Tell(Server& server, uint8_t id, const std::string& text, const char* level = level::kInfo) {
    if (RemoteClient* c = server.Players().ById(id)) {
        server.SendSystem(c, text, level);
    }
}

void SendTo(Server& server, uint8_t id, const json& ev) {
    if (RemoteClient* c = server.Players().ById(id)) {
        server.SendEvent(*c, ev);
    }
}

// What is left of the room's current state: until the lobby gives up, until it runs, until it closes.
int64_t MsLeft(Server& server, const Room& room) {
    RoomTimes t = Rooms_Times(server);
    int64_t now = server.NowMs();
    switch (room.state) {
        case RoomState::Lobby:
            return std::max<int64_t>(0, room.roundMs + t.lobbyMs - now);
        case RoomState::Starting:
            return std::max<int64_t>(0, room.startAtMs - now);
        case RoomState::Running:
            return 0;
        case RoomState::Ended:
            return std::max<int64_t>(0, room.endedMs + t.lingerMs - now);
    }
    return 0;
}

json MemberJson(Server& server, const RoomMember& m) {
    json j = { { "id", m.id }, { "nick", NickOf(server, m.id) }, { "ready", m.ready }, { "here", m.here } };
    if (m.score >= 0) {
        j["score"] = m.score;
    }
    if (m.cs >= 0) {
        j["cs"] = m.cs;
    }
    if (m.won >= 0) {
        j["won"] = m.won != 0;
    }
    return j;
}

std::string StateText(RoomState state) {
    switch (state) {
        case RoomState::Lobby:
            return Tr(Msg::RoomStateLobby);
        case RoomState::Starting:
            return Tr(Msg::RoomStateStarting);
        case RoomState::Running:
            return Tr(Msg::RoomStateRunning);
        case RoomState::Ended:
            return Tr(Msg::RoomStateEnded);
    }
    return "";
}

std::string YesNo(bool value) {
    return Tr(value ? Msg::RoomYes : Msg::RoomNo);
}

// The trip to the director when the room takes its members there (never into a special entrance: they all go there
// together when it starts, Activities/Director.cpp).
void Travel(Server& server, const Room& room, RemoteClient& member) {
    if (!room.settings.travel || room.place == RoomPlace::Entrance || member.id == room.director) {
        return;
    }
    RemoteClient* director = server.Players().ById(room.director);
    if (director == nullptr || !director->hasState || director->scene < 0 || member.scene == director->scene) {
        return;
    }
    Groups_SendTrip(server, member, *director, true);
}

// A room the action needs: who's, and (host) that it leads it. Answers the refusal.
const Room* OwnRoom(Server& server, RemoteClient& who, bool host) {
    const Room* room = server.Rooms().RoomOf(who.id);
    if (room == nullptr) {
        server.SendSystem(&who, Tr(Msg::RoomNone), level::kError);
        return nullptr;
    }
    if (host && room->host != who.id) {
        server.SendSystem(&who, Tr(Msg::RoomOnlyHost), level::kError);
        return nullptr;
    }
    return room;
}

void InviteOne(Server& server, RemoteClient& from, uint8_t toId, RoomChanges& ch) {
    RemoteClient* to = server.Players().ById(toId);
    if (to == nullptr || !Groups_InWorld(*to)) {
        server.SendSystem(&from, Tr(Msg::InviteNotInWorld, { to != nullptr ? to->nick : std::string("?") }),
                          level::kError);
        return;
    }
    RoomBook& book = server.Rooms();
    int64_t ttl = server.Config().inviteMs;
    switch (book.Invite(from.id, to->id, server.NowMs(), ttl, ch)) {
        case RoomInviteResult::NoRoom:
            server.SendSystem(&from, Tr(Msg::RoomNone), level::kError);
            return;
        case RoomInviteResult::NotHost:
            server.SendSystem(&from, Tr(Msg::RoomOnlyHost), level::kError);
            return;
        case RoomInviteResult::Self:
            server.SendSystem(&from, Tr(Msg::InviteSelf), level::kError);
            return;
        case RoomInviteResult::AlreadyMember:
            server.SendSystem(&from, Tr(Msg::RoomAlreadyMember, { to->nick }), level::kWarn);
            return;
        case RoomInviteResult::Full:
            server.SendSystem(&from, Tr(Msg::RoomFull, { std::to_string(kMaxPlayers) }), level::kError);
            return;
        case RoomInviteResult::Sent:
        case RoomInviteResult::Renewed:
            break;
    }
    const Room* room = book.RoomOf(from.id);
    json ev = MakeEvent(ev::kRoomInvite);
    ev["room"] = room->id;
    ev["from"] = from.id;
    ev["nick"] = from.nick;
    ev["key"] = room->key;
    ev["name"] = room->name;
    ev["ms"] = ttl;
    server.SendEvent(*to, ev);
    server.SendSystem(to, Tr(Msg::RoomInvitedYou, { from.nick, room->name }));
    server.SendSystem(&from, Tr(Msg::RoomInviteSent, { to->nick }), level::kOk);
    server.Log().Info(Tr(Msg::LogRoomInvite, { from.nick, to->nick, room->name }));
    AddUniqueId(ch.rooms, room->id); // its pending invitations changed
}

} // namespace

bool Rooms_Enabled(Server& server) {
    return server.Config().rooms;
}

RoomTimes Rooms_Times(Server& server) {
    RoomTimes t;
    t.countdownMs = server.Config().roomCountdownMs;
    t.lobbyMs = server.Config().roomLobbyMs;
    t.lingerMs = server.Config().roomLingerMs;
    return t;
}

bool Rooms_ParsePlace(const std::string& text, RoomPlace& out) {
    if (text == "here") {
        out = RoomPlace::Here;
    } else if (text == "entrance") {
        out = RoomPlace::Entrance;
    } else if (text == "anywhere") {
        out = RoomPlace::Anywhere;
    } else {
        return false;
    }
    return true;
}

const char* Rooms_PlaceName(RoomPlace place) {
    switch (place) {
        case RoomPlace::Here:
            return "here";
        case RoomPlace::Entrance:
            return "entrance";
        case RoomPlace::Anywhere:
            return "anywhere";
    }
    return "here";
}

bool Rooms_ValidMode(const std::string& mode) {
    return std::find(std::begin(kModes), std::end(kModes), mode) != std::end(kModes);
}

const char* Rooms_StateName(RoomState state) {
    switch (state) {
        case RoomState::Lobby:
            return "lobby";
        case RoomState::Starting:
            return "starting";
        case RoomState::Running:
            return "running";
        case RoomState::Ended:
            return "ended";
    }
    return "lobby";
}

std::vector<RemoteClient*> Rooms_Mates(Server& server, const RemoteClient& c) {
    std::vector<RemoteClient*> out;
    if (!Rooms_Enabled(server)) {
        return out;
    }
    for (uint8_t id : server.Rooms().Mates(c.id)) {
        RemoteClient* m = server.Players().ById(id);
        if (m != nullptr && Groups_InWorld(*m)) {
            out.push_back(m);
        }
    }
    return out;
}

std::vector<RemoteClient*> Rooms_PartyInScene(Server& server, const RemoteClient& c) {
    std::vector<RemoteClient*> out = Groups_MatesInScene(server, c);
    for (RemoteClient* m : Rooms_Mates(server, c)) {
        if (c.scene >= 0 && m->scene == c.scene) {
            AddUnique(out, m);
        }
    }
    return out;
}

std::vector<RemoteClient*> Rooms_PartyAll(Server& server, const RemoteClient& c) {
    std::vector<RemoteClient*> out = Groups_Mates(server, c);
    for (RemoteClient* m : Rooms_Mates(server, c)) {
        AddUnique(out, m);
    }
    return out;
}

const Room* Rooms_Running(Server& server, const RemoteClient& c) {
    if (!Rooms_Enabled(server)) {
        return nullptr;
    }
    const Room* room = server.Rooms().RoomOf(c.id);
    return (room != nullptr && room->director == c.id && room->state == RoomState::Running) ? room : nullptr;
}

std::vector<RemoteClient*> Rooms_RewardMates(Server& server, const RemoteClient& c) {
    const Room* room = Rooms_Enabled(server) ? server.Rooms().RoomOf(c.id) : nullptr;
    if (room == nullptr || !room->settings.rewards ||
        (room->state != RoomState::Running && room->state != RoomState::Ended)) {
        return {};
    }
    return Rooms_Mates(server, c);
}

std::vector<RemoteClient*> Rooms_FollowMates(Server& server, const RemoteClient& c, const std::string& key) {
    const Room* room = Rooms_Enabled(server) ? server.Rooms().RoomOf(c.id) : nullptr;
    if (room == nullptr || key.empty() || room->key != key) {
        return {};
    }
    return Rooms_Mates(server, c);
}

json Rooms_StateEvent(Server& server, const Room& room) {
    json ev = MakeEvent(ev::kRoom);
    ev["id"] = room.id;
    ev["key"] = room.key;
    ev["name"] = room.name;
    ev["mode"] = room.mode;
    ev["place"] = Rooms_PlaceName(room.place);
    ev["state"] = Rooms_StateName(room.state);
    ev["ms"] = MsLeft(server, room);
    ev["host"] = room.host;
    ev["director"] = room.director;
    json members = json::array();
    for (const RoomMember& m : room.members) {
        members.push_back(MemberJson(server, m));
    }
    ev["members"] = std::move(members);
    json invites = json::array();
    int64_t now = server.NowMs();
    for (const RoomInvite& inv : server.Rooms().Invites()) {
        if (inv.room == room.id) {
            invites.push_back({ { "id", inv.to },
                                { "nick", NickOf(server, inv.to) },
                                { "ms", std::max<int64_t>(0, inv.expiresMs - now) } });
        }
    }
    ev["invites"] = std::move(invites);
    ev["settings"] = { { "open", room.settings.open },
                       { "travel", room.settings.travel },
                       { "rewards", room.settings.rewards } };
    return ev;
}

json Rooms_NoRoomEvent(const char* reason) {
    json ev = MakeEvent(ev::kRoom);
    ev["id"] = 0;
    ev["reason"] = reason;
    return ev;
}

json Rooms_ListEvent(Server& server) {
    json list = json::array();
    for (const Room& room : server.Rooms().Rooms()) {
        if (!room.settings.open) {
            continue;
        }
        list.push_back({ { "id", room.id },
                         { "host", room.host },
                         { "nick", NickOf(server, room.host) },
                         { "key", room.key },
                         { "name", room.name },
                         { "count", room.members.size() },
                         { "state", Rooms_StateName(room.state) } });
    }
    json ev = MakeEvent(ev::kRooms);
    ev["list"] = std::move(list);
    return ev;
}

void Rooms_Publish(Server& server, const RoomChanges& ch, uint8_t cause) {
    RoomBook& book = server.Rooms();
    std::vector<uint32_t> rooms = ch.rooms;
    // Who is out (first: one that went to another room gets that room's state below)
    for (const RoomChanges::Out& o : ch.out) {
        std::string reason = o.reason;
        std::string nick = NickOf(server, o.player);
        bool elsewhere = book.RoomOf(o.player) != nullptr;
        if (!elsewhere) {
            SendTo(server, o.player, Rooms_NoRoomEvent(o.reason));
        }
        const Room* room = book.Find(o.room); // its others hear it, if it is still there
        if (reason == "kicked") {
            Tell(server, o.player, Tr(Msg::RoomKickedYou, { NickOf(server, cause) }), level::kWarn);
            if (room != nullptr) {
                for (const RoomMember& m : room->members) {
                    if (m.id != cause) {
                        Tell(server, m.id, Tr(Msg::RoomKickedOther, { nick }));
                    }
                }
            }
        } else if (reason == "left" || reason == "gone") {
            if (o.player == cause && !elsewhere) {
                Tell(server, o.player, Tr(Msg::RoomLeftYou));
            }
            if (room != nullptr) {
                for (const RoomMember& m : room->members) {
                    if (m.id != cause) {
                        Tell(server, m.id, Tr(Msg::RoomLeftOther, { nick }));
                    }
                }
            }
        } else if (reason == "closed") {
            if (o.player != cause) {
                Tell(server, o.player, Tr(Msg::RoomClosed));
            }
        } else if (reason == "timeout") {
            Tell(server, o.player, Tr(Msg::RoomClosedTimeout), level::kWarn);
        } else if (reason == "ended") {
            Tell(server, o.player, Tr(Msg::RoomClosed));
        }
        server.Log().Info(Tr(Msg::LogRoomLeave, { nick, reason }));
    }
    // Who came in
    for (const RoomChanges::Joined& j : ch.joined) {
        const Room* room = book.Find(j.room);
        if (room == nullptr || room->Member(j.player) == nullptr) {
            continue;
        }
        std::string how = j.how;
        std::string nick = NickOf(server, j.player);
        if (how == "auto") {
            Tell(server, j.player, Tr(Msg::RoomAutoJoined, { NickOf(server, room->director), room->name }));
        } else {
            if (j.player == cause) {
                Tell(server, j.player, Tr(Msg::RoomJoinedYou, { NickOf(server, room->host), room->name }), level::kOk);
            }
            for (const RoomMember& m : room->members) {
                if (m.id != j.player) {
                    Tell(server, m.id, Tr(Msg::RoomJoinedOther, { nick }));
                }
            }
        }
        server.Log().Info(Tr(Msg::LogRoomJoin, { nick, NickOf(server, room->host), room->name }));
    }
    for (uint32_t id : ch.hostChanged) {
        const Room* room = book.Find(id);
        if (room == nullptr) {
            continue;
        }
        for (const RoomMember& m : room->members) {
            Tell(server, m.id,
                 m.id == room->host ? Tr(Msg::RoomHostNow) : Tr(Msg::RoomHostOther, { NickOf(server, room->host) }));
        }
    }
    for (uint32_t id : ch.started) {
        const Room* room = book.Find(id);
        if (room == nullptr) {
            continue;
        }
        for (const RoomMember& m : room->members) {
            Tell(server, m.id, Tr(Msg::RoomStarted, { room->name }), level::kOk);
        }
        server.Log().Info(Tr(Msg::LogRoomStart, { NickOf(server, room->director), room->name }));
    }
    // Invitations that are over: the invitee hears it, and the texts of whoever did not cause it
    for (const RoomChanges::Ended& e : ch.ended) {
        std::string reason = e.reason;
        json ev = MakeEvent(ev::kRoomInviteEnd);
        ev["room"] = e.invite.room;
        ev["from"] = e.invite.from;
        ev["reason"] = reason;
        SendTo(server, e.invite.to, ev);
        std::string fromNick = NickOf(server, e.invite.from);
        std::string toNick = NickOf(server, e.invite.to);
        if (reason == "expired") {
            if (e.invite.to != cause) {
                Tell(server, e.invite.to, Tr(Msg::RoomInviteExpiredYou, { fromNick }));
            }
            if (e.invite.from != cause) {
                Tell(server, e.invite.from, Tr(Msg::RoomInviteExpiredOther, { toNick }));
            }
        } else if (reason == "declined" && e.invite.from != cause) {
            Tell(server, e.invite.from, Tr(Msg::RoomInviteDeclinedOther, { toNick }));
        }
        if (book.Find(e.invite.room) != nullptr) {
            AddUniqueId(rooms, e.invite.room); // its pending list changed
        }
    }
    // The state of every room that changed, to its members
    for (uint32_t id : rooms) {
        const Room* room = book.Find(id);
        if (room == nullptr) {
            continue;
        }
        json ev = Rooms_StateEvent(server, *room);
        for (const RoomMember& m : room->members) {
            SendTo(server, m.id, ev);
        }
    }
}

void Rooms_UpdatePresence(Server& server, int64_t nowMs, RoomChanges& ch) {
    std::vector<std::pair<uint8_t, bool>> changes;
    for (const Room& room : server.Rooms().Rooms()) {
        if (room.place != RoomPlace::Here || (room.state != RoomState::Lobby && room.state != RoomState::Starting)) {
            continue;
        }
        RemoteClient* director = server.Players().ById(room.director);
        for (const RoomMember& m : room.members) {
            RemoteClient* c = server.Players().ById(m.id);
            bool here = m.id == room.director ||
                        (director != nullptr && c != nullptr && director->scene >= 0 && c->scene == director->scene);
            if (here != m.here) {
                changes.push_back({ m.id, here });
            }
        }
    }
    for (const auto& [id, here] : changes) {
        server.Rooms().SetHere(id, here, nowMs, ch);
    }
}

void Rooms_SendOpenLists(Server& server) {
    if (!Rooms_Enabled(server)) {
        return;
    }
    json list = Rooms_ListEvent(server);
    std::string text = list.dump();
    for (RemoteClient* c : server.Players().Welcomed()) {
        if (!Groups_InWorld(*c)) {
            c->roomsSent.clear(); // it hears the list again when it comes back
            continue;
        }
        if (c->roomsSent != text) {
            c->roomsSent = text;
            server.SendEvent(*c, list);
        }
    }
}

void Rooms_Open(Server& server, RemoteClient& who, const std::string& key, const std::string& name,
                const std::string& mode, RoomPlace place) {
    RoomBook& book = server.Rooms();
    std::vector<uint8_t> autoMembers;
    for (RemoteClient* m : Groups_Mates(server, who)) {
        autoMembers.push_back(m->id); // the group shares its activities (none when groups are off)
    }
    int64_t now = server.NowMs();
    RoomChanges ch;
    RoomOpenResult result = book.Open(who.id, key, name, mode, place, autoMembers, now, ch);
    if (result == RoomOpenResult::Same) {
        if (const Room* room = book.RoomOf(who.id)) {
            server.SendEvent(who, Rooms_StateEvent(server, *room)); // it asked again: here is how it stands
        }
        return;
    }
    if (result == RoomOpenResult::Created) {
        server.Log().Info(Tr(Msg::LogRoomOpen, { who.nick, name }));
    }
    Rooms_UpdatePresence(server, now, ch);
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Ready(Server& server, RemoteClient& who, int ready) {
    const Room* room = OwnRoom(server, who, false);
    if (room == nullptr) {
        return;
    }
    if (room->state != RoomState::Lobby && room->state != RoomState::Starting) {
        server.SendSystem(&who, Tr(Msg::RoomNotWaiting), level::kWarn);
        return;
    }
    bool value = ready < 0 ? !room->Member(who.id)->ready : ready != 0;
    int64_t now = server.NowMs();
    RoomChanges ch;
    Rooms_UpdatePresence(server, now, ch);
    server.Rooms().SetReady(who.id, value, now, ch);
    if (value) {
        if (const Room* current = server.Rooms().RoomOf(who.id)) {
            Travel(server, *current, who);
        }
    }
    Rooms_Publish(server, ch, who.id);
}

void Rooms_InviteIds(Server& server, RemoteClient& who, const std::vector<uint8_t>& to) {
    if (OwnRoom(server, who, true) == nullptr) {
        return;
    }
    if (!who.inviteBudget.Take(server.NowMs())) { // as /invitar: an invitation is a window on someone else's screen
        server.SendSystem(&who, Tr(Msg::RateLimited), level::kWarn);
        return;
    }
    RoomChanges ch;
    for (uint8_t id : to) {
        InviteOne(server, who, id, ch);
    }
    Rooms_Publish(server, ch, who.id);
}

void Rooms_InviteGroup(Server& server, RemoteClient& who) {
    const Room* room = OwnRoom(server, who, true);
    if (room == nullptr) {
        return;
    }
    std::vector<uint8_t> ids;
    for (RemoteClient* m : Groups_Mates(server, who)) {
        if (room->Member(m->id) == nullptr) {
            ids.push_back(m->id);
        }
    }
    if (ids.empty()) {
        server.SendSystem(&who, Tr(Msg::RoomGroupNobody), level::kWarn);
        return;
    }
    Rooms_InviteIds(server, who, ids);
}

void Rooms_Answer(Server& server, RemoteClient& who, uint32_t room, uint8_t from, bool accept) {
    RoomBook& book = server.Rooms();
    RoomInvite chosen;
    bool found = false;
    for (const RoomInvite& inv : book.Invites()) {
        if (inv.to == who.id && (room == 0 || inv.room == room) && (from == 0 || inv.from == from)) {
            chosen = inv; // the last matching one is the latest
            found = true;
        }
    }
    if (!found) {
        server.SendSystem(&who,
                          from != 0 ? Tr(Msg::RoomInviteNoneFrom, { NickOf(server, from) }) : Tr(Msg::RoomInviteNone),
                          level::kError);
        return;
    }
    int64_t now = server.NowMs();
    RoomChanges ch;
    if (!accept) {
        book.Decline(who.id, chosen.from, ch);
        server.SendSystem(&who, Tr(Msg::RoomInviteDeclinedYou, { NickOf(server, chosen.from) }));
        Rooms_Publish(server, ch, who.id);
        return;
    }
    switch (book.Accept(who.id, chosen.room, chosen.from, now, ch)) {
        case RoomAcceptResult::Joined: {
            Rooms_UpdatePresence(server, now, ch);
            const Room* joined = book.RoomOf(who.id);
            if (joined != nullptr && joined->state == RoomState::Running) {
                Travel(server, *joined, who); // it comes in late: to the director
            }
            break;
        }
        case RoomAcceptResult::Gone:
            server.SendSystem(&who, Tr(Msg::RoomGone), level::kError);
            break;
        case RoomAcceptResult::Full:
            server.SendSystem(&who, Tr(Msg::RoomFull, { std::to_string(kMaxPlayers) }), level::kError);
            break;
        case RoomAcceptResult::NoInvite:
            break; // cannot happen: it was just there
    }
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Leave(Server& server, RemoteClient& who) {
    RoomChanges ch;
    if (!server.Rooms().Leave(who.id, "left", server.NowMs(), ch)) {
        server.SendSystem(&who, Tr(Msg::RoomNone), level::kWarn);
        return;
    }
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Kick(Server& server, RemoteClient& who, uint8_t target) {
    const Room* room = OwnRoom(server, who, true);
    if (room == nullptr) {
        return;
    }
    if (target == who.id || room->Member(target) == nullptr) {
        server.SendSystem(&who, Tr(Msg::RoomNotMember, { NickOf(server, target) }), level::kError);
        return;
    }
    RoomChanges ch;
    server.Rooms().Kick(who.id, target, server.NowMs(), ch);
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Close(Server& server, RemoteClient& who) {
    if (OwnRoom(server, who, true) == nullptr) {
        return;
    }
    RoomChanges ch;
    server.Rooms().Close(who.id, ch);
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Join(Server& server, RemoteClient& who, uint32_t room) {
    RoomBook& book = server.Rooms();
    const Room* target = book.Find(room);
    std::string hostNick = target != nullptr ? NickOf(server, target->host) : std::string("?");
    int64_t now = server.NowMs();
    RoomChanges ch;
    switch (book.Join(who.id, room, now, ch)) {
        case RoomJoinResult::Joined: {
            Rooms_UpdatePresence(server, now, ch);
            const Room* joined = book.RoomOf(who.id);
            if (joined != nullptr && joined->state == RoomState::Running) {
                Travel(server, *joined, who);
            }
            break;
        }
        case RoomJoinResult::Gone:
            server.SendSystem(&who, Tr(Msg::RoomGone), level::kError);
            break;
        case RoomJoinResult::NotOpen:
            server.SendSystem(&who, Tr(Msg::RoomNotOpen, { hostNick }), level::kError);
            break;
        case RoomJoinResult::Full:
            server.SendSystem(&who, Tr(Msg::RoomFull, { std::to_string(kMaxPlayers) }), level::kError);
            break;
        case RoomJoinResult::Already:
            server.SendSystem(&who, Tr(Msg::RoomAlreadyIn), level::kWarn);
            break;
    }
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Settings(Server& server, RemoteClient& who, int open, int travel, int rewards) {
    const Room* room = OwnRoom(server, who, true);
    if (room == nullptr) {
        return;
    }
    RoomSettings s = room->settings;
    if (open >= 0) {
        s.open = open != 0;
    }
    if (travel >= 0) {
        s.travel = travel != 0;
    }
    if (rewards >= 0) {
        s.rewards = rewards != 0;
    }
    RoomChanges ch;
    server.Rooms().SetSettings(who.id, s, ch);
    Rooms_Publish(server, ch, who.id);
}

void Rooms_Chat(Server& server, RemoteClient& who, const std::string& raw) {
    const Room* room = OwnRoom(server, who, false);
    if (room == nullptr) {
        return;
    }
    std::string text = SanitizeChat(raw, kChatMaxChars);
    if (text.empty()) {
        return;
    }
    json ev = MakeEvent(ev::kRoomChat);
    ev["room"] = room->id;
    ev["from"] = who.id;
    ev["nick"] = who.nick;
    ev["text"] = text;
    for (const RoomMember& m : room->members) {
        SendTo(server, m.id, ev); // the sender too: it sees its line once the server took it
    }
    server.Log().Info(Tr(Msg::LogRoomChat, { NickOf(server, room->host), who.nick, text }));
}

std::string Rooms_Describe(Server& server, const RemoteClient& who) {
    RoomBook& book = server.Rooms();
    std::string text;
    if (const Room* room = book.RoomOf(who.id)) {
        text = Tr(Msg::RoomTitle, { NickOf(server, room->host), room->name, StateText(room->state),
                                    std::to_string(room->members.size()), std::to_string(kMaxPlayers) });
        for (const RoomMember& m : room->members) {
            text += "\n" + NickOf(server, m.id);
            if (m.id == room->host) {
                text += Tr(Msg::RoomHostTag);
            }
            if (room->state == RoomState::Lobby || room->state == RoomState::Starting) {
                text += Tr(m.ready ? Msg::RoomReadyTag : Msg::RoomWaitingTag);
            }
        }
        text += "\n" + Tr(Msg::RoomSettingsNow,
                          { YesNo(room->settings.open), YesNo(room->settings.travel), YesNo(room->settings.rewards) });
    } else {
        text = Tr(Msg::RoomNone);
    }
    std::vector<RoomInvite> invites = book.InvitesTo(who.id);
    if (!invites.empty()) {
        text += "\n" + Tr(Msg::RoomInvitesTitle);
        int64_t now = server.NowMs();
        for (const RoomInvite& inv : invites) {
            const Room* room = book.Find(inv.room);
            int64_t secs = std::max<int64_t>(0, (inv.expiresMs - now + 999) / 1000);
            text += "\n" + Tr(Msg::RoomInviteLine, { NickOf(server, inv.from),
                                                     room != nullptr ? room->name : std::string("?"),
                                                     std::to_string(secs) });
        }
    }
    return text;
}

void Rooms_OnAct(Server& server, RemoteClient& who, const std::string& state, const std::string& key, int64_t score,
                 int64_t cs, int won) {
    if (!Rooms_Enabled(server)) {
        return;
    }
    RoomBook& book = server.Rooms();
    RoomChanges ch;
    if (state == "end") {
        book.SetResult(who.id, key, score, cs, -1, ch);
        book.EndRound(who.id, key, server.NowMs(), ch);
    } else if (state == "result") {
        book.SetResult(who.id, key, -1, cs, won, ch);
    }
    if (!ch.Empty()) {
        Rooms_Publish(server, ch, 0);
    }
}

} // namespace coop::server
