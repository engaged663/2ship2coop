// [COOP] See Room.h. This game's copy of its activity room ("room"), its invitations ("room_invite",
// "room_invite_end"), the open rooms ("rooms") and the room's chat ("room_chat"); the actions are "room_op" events
// (the server answers refusals with a text in its language). Mates: what a group or a room shares (dialogues,
// cutscenes, prizes) goes to group mates and room mates alike.
#include "Room.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "common/Protocol.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr size_t kChatKeep = 50;
constexpr int64_t kMaxMs = 3600000; // what the server says is left, at most (an hour)

RoomInfo sRoom;
uint32_t sSerial = 0;
bool sServerOff = false; // the server has no rooms (server.json "rooms": false): we stop asking until we reconnect
std::vector<RoomInvitation> sInvitations;
std::vector<OpenRoom> sOpen;
std::vector<RoomChatLine> sChat;

RoomPhase PhaseOf(const std::string& state) {
    if (state == "lobby") {
        return RoomPhase::Lobby;
    }
    if (state == "starting") {
        return RoomPhase::Starting;
    }
    if (state == "running") {
        return RoomPhase::Running;
    }
    if (state == "ended") {
        return RoomPhase::Ended;
    }
    return RoomPhase::None;
}

void Reset() {
    sRoom = RoomInfo{};
    sServerOff = false;
    sInvitations.clear();
    sOpen.clear();
    sChat.clear();
}

void OnRoom(const json& ev) {
    sSerial++;
    uint32_t id = (uint32_t)GetInt(ev, "id");
    if (id == 0) {
        sRoom = RoomInfo{}; // the server says why in the chat
        sChat.clear();
        if (GetString(ev, "reason") == "off") {
            sServerOff = true;
        }
        return;
    }
    RoomInfo r;
    r.id = id;
    r.key = GetString(ev, "key");
    r.name = GetString(ev, "name");
    r.mode = GetString(ev, "mode");
    r.place = GetString(ev, "place");
    r.phase = PhaseOf(GetString(ev, "state"));
    r.untilMs = Group_NowMs() + std::clamp<int64_t>(GetInt(ev, "ms"), 0, kMaxMs);
    r.host = (uint8_t)GetInt(ev, "host");
    r.director = (uint8_t)GetInt(ev, "director");
    auto members = ev.find("members");
    if (members != ev.end() && members->is_array()) {
        for (const json& m : *members) {
            if (!m.is_object()) {
                continue;
            }
            RoomMemberInfo info;
            info.id = (uint8_t)GetInt(m, "id");
            info.nick = GetString(m, "nick");
            info.ready = GetBool(m, "ready");
            info.here = GetBool(m, "here", true);
            info.score = GetInt(m, "score", -1);
            info.cs = GetInt(m, "cs", -1);
            info.won = m.contains("won") ? (GetBool(m, "won") ? 1 : 0) : -1;
            if (info.id != 0) {
                r.members.push_back(std::move(info));
            }
        }
    }
    auto invites = ev.find("invites");
    if (invites != ev.end() && invites->is_array()) {
        int64_t now = Group_NowMs();
        for (const json& i : *invites) {
            if (i.is_object()) {
                r.invites.push_back({ (uint8_t)GetInt(i, "id"), GetString(i, "nick"),
                                      now + std::clamp<int64_t>(GetInt(i, "ms"), 0, kMaxMs) });
            }
        }
    }
    auto settings = ev.find("settings");
    if (settings != ev.end() && settings->is_object()) {
        r.open = GetBool(*settings, "open");
        r.travel = GetBool(*settings, "travel", true);
        r.rewards = GetBool(*settings, "rewards", true);
    }
    if (sRoom.id != id) {
        sChat.clear(); // another room: its own chat
    }
    sRoom = std::move(r);
    // Our answer to an invitation to this room is no longer pending
    sInvitations.erase(std::remove_if(sInvitations.begin(), sInvitations.end(),
                                      [&](const RoomInvitation& inv) { return inv.room == id; }),
                       sInvitations.end());
}

void OnRoomInvite(const json& ev) {
    RoomInvitation inv;
    inv.room = (uint32_t)GetInt(ev, "room");
    inv.from = (uint8_t)GetInt(ev, "from");
    inv.nick = GetString(ev, "nick");
    inv.key = GetString(ev, "key");
    inv.name = GetString(ev, "name");
    inv.expiresMs = Group_NowMs() + std::clamp<int64_t>(GetInt(ev, "ms", 60000), 1000, 600000);
    if (inv.room == 0 || inv.from == 0) {
        return;
    }
    sInvitations.erase(std::remove_if(sInvitations.begin(), sInvitations.end(),
                                      [&](const RoomInvitation& i) { return i.room == inv.room; }),
                       sInvitations.end());
    sInvitations.push_back(inv);
}

void OnRoomInviteEnd(const json& ev) {
    uint32_t room = (uint32_t)GetInt(ev, "room");
    sInvitations.erase(std::remove_if(sInvitations.begin(), sInvitations.end(),
                                      [&](const RoomInvitation& i) { return i.room == room; }),
                       sInvitations.end());
}

void OnRoomChat(const json& ev) {
    RoomChatLine line{ GetString(ev, "nick"), GetString(ev, "text") };
    if (line.text.empty()) {
        return;
    }
    sChat.push_back(line);
    if (sChat.size() > kChatKeep) {
        sChat.erase(sChat.begin());
    }
    // Also in the main chat: it is read while playing, when the room's window is small
    Chat_Add(ChatKind::Room, "[Sala] <" + line.nick + "> " + line.text);
}

void OnRooms(const json& ev) {
    sOpen.clear();
    auto list = ev.find("list");
    if (list == ev.end() || !list->is_array()) {
        return;
    }
    for (const json& r : *list) {
        if (!r.is_object()) {
            continue;
        }
        OpenRoom o;
        o.id = (uint32_t)GetInt(r, "id");
        o.host = (uint8_t)GetInt(r, "host");
        o.nick = GetString(r, "nick");
        o.key = GetString(r, "key");
        o.name = GetString(r, "name");
        o.state = GetString(r, "state");
        o.count = (int)GetInt(r, "count");
        if (o.id != 0) {
            sOpen.push_back(std::move(o));
        }
    }
}

void OnPlayerLeft(const json& ev) {
    uint8_t id = (uint8_t)GetInt(ev, "id");
    sInvitations.erase(std::remove_if(sInvitations.begin(), sInvitations.end(),
                                      [&](const RoomInvitation& i) { return i.from == id; }),
                       sInvitations.end());
}

json Op(const char* op) {
    json ev = MakeEvent(ev::kRoomOp);
    ev["op"] = op;
    return ev;
}

void Send(const json& ev) {
    if (!Session_IsConnected()) {
        Chat_Add(ChatKind::Error, "No estás conectado a ningún servidor.");
        return;
    }
    NetClient::Get().SendEvent(ev);
}

bool Waiting(RoomPhase phase) {
    return phase == RoomPhase::Lobby || phase == RoomPhase::Starting;
}

bool InRound(RoomPhase phase) {
    return Waiting(phase) || phase == RoomPhase::Running;
}

Player* LocalLink() {
    return gPlayState != nullptr ? (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
}

} // namespace

bool Room_Enabled() {
    return CVarGetInteger("gCoop.Room.Enabled", 1) != 0 && !sServerOff;
}

bool Room_HoldEnabled() {
    return CVarGetInteger("gCoop.Room.Hold", 1) != 0;
}

const RoomInfo& Room_Get() {
    return sRoom;
}

uint32_t Room_Serial() {
    return sSerial;
}

bool Room_Has() {
    return sRoom.id != 0;
}

bool Room_IsHost() {
    return Room_Has() && sRoom.host == Session_LocalId();
}

bool Room_IsDirector() {
    return Room_Has() && sRoom.director == Session_LocalId();
}

bool Room_IsMate(uint8_t playerId) {
    if (!Room_Has() || playerId == 0 || playerId == Session_LocalId()) {
        return false;
    }
    for (const RoomMemberInfo& m : sRoom.members) {
        if (m.id == playerId) {
            return true;
        }
    }
    return false;
}

bool Room_HasMates() {
    return Room_Has() && sRoom.members.size() > 1;
}

const RoomMemberInfo* Room_Me() {
    for (const RoomMemberInfo& m : sRoom.members) {
        if (m.id == Session_LocalId()) {
            return &m;
        }
    }
    return nullptr;
}

bool Room_GuestOf(const std::string& key) {
    return Room_Has() && sRoom.key == key && sRoom.director != Session_LocalId() && InRound(sRoom.phase);
}

bool Room_DirectsRound(const std::string& key) {
    return Room_Has() && sRoom.key == key && sRoom.director == Session_LocalId() && InRound(sRoom.phase);
}

bool Room_Running(const std::string& key) {
    return Room_Has() && sRoom.key == key && sRoom.phase == RoomPhase::Running;
}

const std::vector<RoomInvitation>& Room_Invitations() {
    int64_t now = Group_NowMs();
    sInvitations.erase(std::remove_if(sInvitations.begin(), sInvitations.end(),
                                      [&](const RoomInvitation& i) { return i.expiresMs <= now; }),
                       sInvitations.end());
    return sInvitations;
}

const std::vector<OpenRoom>& Room_OpenRooms() {
    return sOpen;
}

const std::vector<RoomChatLine>& Room_ChatLines() {
    return sChat;
}

std::string Room_NickOf(uint8_t playerId) {
    for (const RoomMemberInfo& m : sRoom.members) {
        if (m.id == playerId) {
            return m.nick;
        }
    }
    if (playerId == Session_LocalId()) {
        return Session_LocalNick();
    }
    const RemotePlayer* p = Session_FindPlayer(playerId);
    return p != nullptr ? p->nick : std::string("?");
}

void Room_Open(const std::string& key, const std::string& name, const char* mode, const char* place) {
    json ev = Op("open");
    ev["key"] = key;
    ev["name"] = name;
    ev["mode"] = mode;
    ev["place"] = place;
    Send(ev);
}

void Room_SetReady(bool ready) {
    json ev = Op("ready");
    ev["ready"] = ready;
    Send(ev);
}

void Room_Invite(uint8_t playerId) {
    json ev = Op("invite");
    ev["to"] = playerId;
    Send(ev);
}

void Room_InviteGroup() {
    json ev = Op("invite");
    ev["group"] = true;
    Send(ev);
}

void Room_Answer(uint32_t room, bool accept) {
    json ev = Op("answer");
    ev["room"] = room;
    ev["accept"] = accept;
    Send(ev);
}

void Room_Leave() {
    Send(Op("leave"));
}

void Room_Kick(uint8_t playerId) {
    json ev = Op("kick");
    ev["who"] = playerId;
    Send(ev);
}

void Room_Close() {
    Send(Op("close"));
}

void Room_Settings(bool open, bool travel, bool rewards) {
    json ev = Op("settings");
    ev["open"] = open;
    ev["travel"] = travel;
    ev["rewards"] = rewards;
    Send(ev);
}

void Room_Join(uint32_t room) {
    json ev = Op("join");
    ev["room"] = room;
    Send(ev);
}

void Room_Chat(const std::string& text) {
    json ev = Op("chat");
    ev["text"] = text;
    Send(ev);
}

bool Mates_Is(uint8_t playerId) {
    return Group_IsMate(playerId) || Room_IsMate(playerId);
}

Actor* Mates_Actor(uint8_t playerId) {
    if (!Mates_Is(playerId) || gPlayState == nullptr) {
        return nullptr;
    }
    Actor* puppet = PuppetManager_Actor(playerId);
    return (puppet != nullptr && puppet->update != nullptr) ? puppet : nullptr;
}

bool Mates_Near(uint8_t playerId, float maxDist) {
    Actor* mate = Mates_Actor(playerId);
    Player* link = LocalLink();
    if (mate == nullptr || link == nullptr) {
        return false;
    }
    return maxDist <= 0.f || Actor_WorldDistXYZToActor(&link->actor, mate) <= maxDist;
}

bool Mates_AnyNear(float maxDist) {
    for (const GroupMember& m : Group_Members()) {
        if (Mates_Near(m.id, maxDist)) {
            return true;
        }
    }
    for (const RoomMemberInfo& m : sRoom.members) {
        if (Mates_Near(m.id, maxDist)) {
            return true;
        }
    }
    return false;
}

} // namespace coop::client

COOP_ON_EVENT(roomState, coop::ev::kRoom, coop::client::OnRoom);
COOP_ON_EVENT(roomInvite, coop::ev::kRoomInvite, coop::client::OnRoomInvite);
COOP_ON_EVENT(roomInviteEnd, coop::ev::kRoomInviteEnd, coop::client::OnRoomInviteEnd);
COOP_ON_EVENT(roomChat, coop::ev::kRoomChat, coop::client::OnRoomChat);
COOP_ON_EVENT(roomsList, coop::ev::kRooms, coop::client::OnRooms);
COOP_ON_EVENT(roomPlayerLeft, coop::ev::kLeave, coop::client::OnPlayerLeft);
COOP_ON_EVENT(roomWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(roomLost, [](const std::string&) { coop::client::Reset(); });
