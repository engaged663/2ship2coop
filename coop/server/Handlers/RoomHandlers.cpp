// Activity rooms (spec 2026-10-04-coop-salas-actividades §3-§4): "room_op" (the games' room window and their
// directors), the rooms' clock (presence in the director's scene, countdowns, invitations, lobbies that give up, ended
// rooms that close, the open rooms' list) and the players who leave. The rules are in RoomBook, the texts and events
// in Rooms.cpp.
#include "server/Groups.h"
#include "server/Registry.h"
#include "server/Rooms.h"
#include "server/Server.h"

#include "common/Text.h"

#include <algorithm>
#include <vector>

namespace coop::server {

namespace {

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

// A bool field: -1 missing, 0/1 its value; -2 present with another type (invalid).
int OptBool(const json& ev, const char* key) {
    auto it = ev.find(key);
    if (it == ev.end()) {
        return -1;
    }
    return it->is_boolean() ? (it->get<bool>() ? 1 : 0) : -2;
}

bool KnownOp(const std::string& op) {
    static const char* const kOps[] = { "open",  "ready",    "invite", "answer", "leave", "kick",
                                        "close", "settings", "join",   "chat" };
    return std::find(std::begin(kOps), std::end(kOps), op) != std::end(kOps);
}

void Invalid(Server& server, RemoteClient& client) {
    server.NoteInvalid(client, Tr(Msg::InvRoom));
}

void OnRoomOp(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.roomBudget.Take(server.NowMs())) {
        return;
    }
    std::string op = GetString(ev, "op");
    if (!KnownOp(op)) {
        Invalid(server, client);
        return;
    }
    if (!Rooms_Enabled(server)) {
        if (op == "open") {
            server.SendEvent(client, Rooms_NoRoomEvent("off")); // its game must not wait for anyone
        } else {
            server.SendSystem(&client, Tr(Msg::RoomsOff), level::kError);
        }
        return;
    }
    if (op == "open") {
        std::string key = GetString(ev, "key");
        std::string mode = GetString(ev, "mode");
        RoomPlace place;
        if (!Groups_ValidActivityKey(key) || !Rooms_ValidMode(mode) ||
            !Rooms_ParsePlace(GetString(ev, "place"), place)) {
            Invalid(server, client);
            return;
        }
        std::string name = SanitizeChat(GetString(ev, "name"), kMaxActivityName);
        Rooms_Open(server, client, key, name.empty() ? key : name, mode, place);
    } else if (op == "ready") {
        int ready = OptBool(ev, "ready");
        if (ready < 0) {
            Invalid(server, client);
            return;
        }
        Rooms_Ready(server, client, ready);
    } else if (op == "invite") {
        if (OptBool(ev, "group") == 1) {
            Rooms_InviteGroup(server, client);
        } else if (IntIn(ev, "to", 1, 255)) {
            Rooms_InviteIds(server, client, { (uint8_t)GetInt(ev, "to") });
        } else {
            Invalid(server, client);
        }
    } else if (op == "answer") {
        int accept = OptBool(ev, "accept");
        if (!IntIn(ev, "room", 1, 0xFFFFFFFFll) || accept < 0) {
            Invalid(server, client);
            return;
        }
        Rooms_Answer(server, client, (uint32_t)GetInt(ev, "room"), 0, accept == 1);
    } else if (op == "leave") {
        Rooms_Leave(server, client);
    } else if (op == "kick") {
        if (!IntIn(ev, "who", 1, 255)) {
            Invalid(server, client);
            return;
        }
        Rooms_Kick(server, client, (uint8_t)GetInt(ev, "who"));
    } else if (op == "close") {
        Rooms_Close(server, client);
    } else if (op == "settings") {
        int open = OptBool(ev, "open");
        int travel = OptBool(ev, "travel");
        int rewards = OptBool(ev, "rewards");
        if (open == -2 || travel == -2 || rewards == -2) {
            Invalid(server, client);
            return;
        }
        Rooms_Settings(server, client, open, travel, rewards);
    } else if (op == "join") {
        if (!IntIn(ev, "room", 1, 0xFFFFFFFFll)) {
            Invalid(server, client);
            return;
        }
        Rooms_Join(server, client, (uint32_t)GetInt(ev, "room"));
    } else if (op == "chat") {
        auto text = ev.find("text");
        if (text == ev.end() || !text->is_string()) {
            Invalid(server, client);
            return;
        }
        if (server.AllowMessage(client)) { // the chat's own limit too
            Rooms_Chat(server, client, text->get<std::string>());
        }
    }
}

void TickRooms(Server& server) {
    RoomBook& book = server.Rooms();
    if (book.Rooms().empty() && book.Invites().empty()) {
        Rooms_SendOpenLists(server);
        return;
    }
    book.SetTimes(Rooms_Times(server));
    int64_t now = server.NowMs();
    RoomChanges ch;
    // Whoever left the world (a disconnection is handled at once, below)
    std::vector<uint8_t> gone;
    auto check = [&](uint8_t id) {
        RemoteClient* c = server.Players().ById(id);
        if ((c == nullptr || !Groups_InWorld(*c)) && std::find(gone.begin(), gone.end(), id) == gone.end()) {
            gone.push_back(id);
        }
    };
    for (const Room& room : book.Rooms()) {
        for (const RoomMember& m : room.members) {
            check(m.id);
        }
    }
    for (const RoomInvite& inv : book.Invites()) {
        check(inv.to);
    }
    for (uint8_t id : gone) {
        book.Forget(id, now, ch);
    }
    Rooms_UpdatePresence(server, now, ch);
    book.Tick(now, ch);
    if (!ch.Empty()) {
        Rooms_Publish(server, ch, 0);
    }
    Rooms_SendOpenLists(server);
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (!client.welcomed || client.host) {
        return;
    }
    RoomChanges ch;
    server.Rooms().Forget(client.id, server.NowMs(), ch);
    if (!ch.Empty()) {
        Rooms_Publish(server, ch, client.id);
    }
}

} // namespace

COOP_SERVER_EVENT(roomOp, ev::kRoomOp, true, OnRoomOp);
COOP_SERVER_ON_TICK(roomsTick, TickRooms);
COOP_SERVER_ON_DISCONNECT(roomsDisconnect, OnDisconnect);

} // namespace coop::server
