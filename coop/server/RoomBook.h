#pragma once
// Activity rooms (docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md §2). Pure bookkeeping by player
// id: Rooms.cpp turns its changes into events and texts; the room_op handler (Handlers/RoomHandlers.cpp) and the
// commands (Commands/RoomCommands.cpp) call it. Rules: a player is in one room at most; a room has kMaxPlayers members
// at most (the host first, then by arrival); a round starts when every member is ready (and, for a minigame played
// where its NPC is, in the director's scene); an ended room stays a while for another round.
#include "common/Protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

enum class RoomState : uint8_t {
    Lobby,    // waiting for everyone to be ready (the director's game waits)
    Starting, // everyone ready: the countdown runs
    Running,  // the activity runs
    Ended,    // its round is over: the room stays kRoomLingerMs for the prizes, the chat and another round
};

enum class RoomPlace : uint8_t {
    Here,     // played where its NPC is: everyone must be in the director's scene to start
    Entrance, // its scene is reloaded at a special entrance: everyone goes there when it starts
    Anywhere, // played in several scenes (the Bombers' hide-and-seek)
};

struct RoomSettings {
    bool open = false;   // anyone in the world may join without an invitation
    bool travel = true;  // a member who gets ready is taken to the director (never for Entrance)
    bool rewards = true; // the activity's prizes go to the room
};

struct RoomMember {
    uint8_t id = 0;
    bool ready = false;
    bool here = true;    // in the director's scene (Rooms.cpp keeps it; only Here rooms wait for it)
    int64_t score = -1;  // this round's result (-1: none)
    int64_t cs = -1;
    int8_t won = -1;     // -1 unknown, 0 lost, 1 won
};

struct Room {
    uint32_t id = 0;
    std::string key;  // the activity (the games' table)
    std::string name;
    std::string mode; // together, each, turns, shared (shown by the games)
    RoomPlace place = RoomPlace::Here;
    RoomState state = RoomState::Lobby;
    uint8_t host = 0;     // its owner: invites, removes, changes the settings, closes it
    uint8_t director = 0; // the game that runs (or ran) the round
    std::vector<RoomMember> members; // the host first
    RoomSettings settings;
    int64_t roundMs = 0;   // when this round's lobby opened (the lobby's timeout counts from here)
    int64_t startAtMs = 0; // Starting: when it runs
    int64_t endedMs = 0;   // Ended: since when

    const RoomMember* Member(uint8_t id) const;
};

struct RoomInvite {
    uint32_t room = 0;
    uint8_t from = 0;
    uint8_t to = 0;
    int64_t expiresMs = 0;
};

// What a call changed (Rooms.cpp tells whoever must know).
struct RoomChanges {
    std::vector<uint32_t> rooms; // their state goes to their members
    struct Out {
        uint8_t player;
        uint32_t room;
        const char* reason; // left, kicked, closed, ended, timeout, gone
    };
    std::vector<Out> out; // no longer in that room
    struct Ended {
        RoomInvite invite;
        const char* reason; // accepted, declined, expired, cancelled, full
    };
    std::vector<Ended> ended; // invitations that are over
    struct Joined {
        uint32_t room;
        uint8_t player;
        const char* how; // auto (the director's group), invite, open
    };
    std::vector<Joined> joined;
    std::vector<uint32_t> started;     // went Running
    std::vector<uint32_t> hostChanged; // a new host (the old one left)

    bool Empty() const;
};

enum class RoomOpenResult { Created, Reopened, Same };
enum class RoomInviteResult { Sent, Renewed, Self, AlreadyMember, Full, NotHost, NoRoom };
enum class RoomAcceptResult { Joined, NoInvite, Gone, Full };
enum class RoomJoinResult { Joined, Gone, NotOpen, Full, Already };

struct RoomTimes {
    int64_t countdownMs = kRoomCountdownMs;
    int64_t lobbyMs = kRoomLobbyMs;
    int64_t lingerMs = kRoomLingerMs;
};

class RoomBook {
  public:
    explicit RoomBook(const RoomTimes& times = {}) : mTimes(times) {
    }
    void SetTimes(const RoomTimes& times) {
        mTimes = times;
    }

    const Room* Find(uint32_t id) const;
    const Room* RoomOf(uint8_t player) const;
    std::vector<uint8_t> Mates(uint8_t player) const;         // the others of its room
    std::vector<RoomInvite> InvitesTo(uint8_t player) const; // oldest first
    const std::vector<Room>& Rooms() const {
        return mRooms;
    }
    const std::vector<RoomInvite>& Invites() const {
        return mInvites;
    }

    // director's game started the activity key: a new room (it leaves the one it was in; autoMembers in no room come
    // along), its ended room with that key again (a new round it directs), or nothing new (Same).
    RoomOpenResult Open(uint8_t director, const std::string& key, const std::string& name, const std::string& mode,
                        RoomPlace place, const std::vector<uint8_t>& autoMembers, int64_t nowMs, RoomChanges& out);
    // Only the host invites, to its room.
    RoomInviteResult Invite(uint8_t from, uint8_t to, int64_t nowMs, int64_t ttlMs, RoomChanges& out);
    // to accepts its latest invitation (to that room / from that player when not 0); it leaves its old room.
    RoomAcceptResult Accept(uint8_t to, uint32_t room, uint8_t from, int64_t nowMs, RoomChanges& out);
    int Decline(uint8_t to, uint8_t from, RoomChanges& out); // from 0: all of them; returns how many
    RoomJoinResult Join(uint8_t player, uint32_t room, int64_t nowMs, RoomChanges& out); // an open room
    bool SetReady(uint8_t player, bool ready, int64_t nowMs, RoomChanges& out); // false: not in a waiting room
    void SetHere(uint8_t player, bool here, int64_t nowMs, RoomChanges& out);
    bool Leave(uint8_t player, const char* reason, int64_t nowMs, RoomChanges& out); // false: in no room
    bool Kick(uint8_t host, uint8_t who, int64_t nowMs, RoomChanges& out);
    bool Close(uint8_t host, RoomChanges& out);
    bool SetSettings(uint8_t host, const RoomSettings& settings, RoomChanges& out);
    // The director's activity ended (its "act end"): the round is over.
    void EndRound(uint8_t director, const std::string& key, int64_t nowMs, RoomChanges& out);
    // A result of player's round of key (-1: not known): kept in its room for everyone to see.
    void SetResult(uint8_t player, const std::string& key, int64_t score, int64_t cs, int won, RoomChanges& out);
    // player left the world or disconnected: out of its room ("gone"), its invitations end.
    void Forget(uint8_t player, int64_t nowMs, RoomChanges& out);
    void Tick(int64_t nowMs, RoomChanges& out);

  private:
    Room* FindMut(uint32_t id);
    Room* RoomOfMut(uint8_t player);
    void Changed(const Room& room, RoomChanges& out);
    void Evaluate(Room& room, int64_t nowMs, RoomChanges& out);
    void Run(Room& room, RoomChanges& out);
    void CloseAt(size_t index, const char* reason, RoomChanges& out);
    void CancelInvites(uint32_t room, RoomChanges& out);

    RoomTimes mTimes;
    std::vector<Room> mRooms;
    std::vector<RoomInvite> mInvites;
    uint32_t mNextId = 1;
};

} // namespace coop::server
