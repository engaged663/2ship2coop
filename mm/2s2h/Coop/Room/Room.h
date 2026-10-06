#pragma once
// [COOP] Activity rooms (spec: docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md). Every minigame a
// game starts opens a room in the server: whoever started it (the director) and its group, plus whoever it invites for
// that activity only; it has its own chat and settings, and the activity starts when everyone is ready. While it runs,
// its members share it wherever they are. The server keeps the rooms; this is this game's copy ("room",
// "room_invite", "room_chat", "rooms") and its actions ("room_op"). Map:
//   RoomState.cpp   the copy, the actions, who our mates are (group or room)
//   RoomHold.cpp    the director's game waits (frozen) until everyone is ready (Coop_PlayHeld, z_play.c)
//   RoomWindow.cpp  the room's window: members, ready, invitations, settings, chat
//   RoomMenu.cpp    the "Salas" section of the Co-op menu (open rooms, options)
#include <cstdint>
#include <string>
#include <vector>

struct Actor;

namespace coop::client {

enum class RoomPhase : uint8_t { None, Lobby, Starting, Running, Ended };

struct RoomMemberInfo {
    uint8_t id = 0;
    std::string nick;
    bool ready = false;
    bool here = true;   // in the director's scene (rooms played where their NPC is wait for it)
    int64_t score = -1; // this round's result (-1: none)
    int64_t cs = -1;
    int won = -1;       // -1 unknown, 0 lost, 1 won
};

struct RoomPendingInvite {
    uint8_t id = 0;
    std::string nick;
    int64_t expiresMs = 0; // Group_NowMs() clock
};

struct RoomInfo {
    uint32_t id = 0; // 0: in no room
    std::string key, name;
    std::string mode;  // together, each, turns, shared
    std::string place; // here, entrance, anywhere
    RoomPhase phase = RoomPhase::None;
    int64_t untilMs = 0; // Lobby: when it gives up; Starting: when it runs; Ended: when it closes (Group_NowMs)
    uint8_t host = 0;     // invites, removes, changes the settings, closes it
    uint8_t director = 0; // the game that runs (or ran) the round
    std::vector<RoomMemberInfo> members; // the host first
    std::vector<RoomPendingInvite> invites;
    bool open = false, travel = true, rewards = true;
};

struct RoomInvitation { // to us
    uint32_t room = 0;
    uint8_t from = 0;
    std::string nick, key, name;
    int64_t expiresMs = 0; // Group_NowMs() clock
};

struct OpenRoom { // anyone in the world may join it
    uint32_t id = 0;
    uint8_t host = 0;
    std::string nick, key, name, state;
    int count = 0;
};

struct RoomChatLine {
    std::string nick;
    std::string text;
};

// The player's options (gCoop.Room.*, on by default)
bool Room_Enabled();     // this game opens a room when it starts an activity
bool Room_HoldEnabled(); // ...and waits for everyone's confirmation before it really starts

const RoomInfo& Room_Get();
uint32_t Room_Serial(); // grows with every "room" the server sends (RoomHold.cpp: it answered)
bool Room_Has();        // in a room
bool Room_IsHost();
bool Room_IsDirector();
bool Room_IsMate(uint8_t playerId); // in our room and not us
bool Room_HasMates();               // someone else in our room
const RoomMemberInfo* Room_Me();
// In a round of that activity (waiting, starting or running) that another game directs: we are its guest.
bool Room_GuestOf(const std::string& key);
// We direct a round of that activity now (waiting, starting or running).
bool Room_DirectsRound(const std::string& key);
bool Room_Running(const std::string& key); // our room's round of that activity runs
const std::vector<RoomInvitation>& Room_Invitations(); // pending (expired ones dropped)
const std::vector<OpenRoom>& Room_OpenRooms();
const std::vector<RoomChatLine>& Room_ChatLines(); // the room's chat, oldest first
std::string Room_NickOf(uint8_t playerId);         // a member's nick ("?" if unknown)

// Actions ("room_op"); the server answers with the room's state or a text.
void Room_Open(const std::string& key, const std::string& name, const char* mode, const char* place);
void Room_SetReady(bool ready);
void Room_Invite(uint8_t playerId);
void Room_InviteGroup();
void Room_Answer(uint32_t room, bool accept);
void Room_Leave();
void Room_Kick(uint8_t playerId);
void Room_Close();
void Room_Settings(bool open, bool travel, bool rewards);
void Room_Join(uint32_t room);
void Room_Chat(const std::string& text);

// Mates: group mates or room mates (what dialogues, cutscenes and prizes are shared with).
bool Mates_Is(uint8_t playerId);
Actor* Mates_Actor(uint8_t playerId);             // its Link (puppet) in our scene, or nullptr
bool Mates_Near(uint8_t playerId, float maxDist); // ...within maxDist of our Link (<= 0: anywhere in the scene)
bool Mates_AnyNear(float maxDist);

// RoomHold.cpp
bool RoomHold_Active();                     // our game is frozen now, waiting for the room
void RoomHold_Begin(const std::string& key); // the director's game opened a room for key: it waits from now on

// RoomWindow.cpp
void RoomWindow_Show(); // open it in full

// RoomMenu.cpp
void RoomMenu_Draw(); // the "Salas" section (CoopMenu.cpp)

} // namespace coop::client
