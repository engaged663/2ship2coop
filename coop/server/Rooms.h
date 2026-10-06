#pragma once
// Server side of activity rooms (docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md §3): the actions
// players ask for (the "room_op" events of Handlers/RoomHandlers.cpp and the /sala, /listo, /s commands of
// Commands/RoomCommands.cpp do the same), the events and texts RoomBook's changes turn into, the trips to the
// director, who is in its scene, and who hears what an activity shares: its room's members, in any scene
// (Handlers/ActivityHandlers.cpp).
#include "GroupBook.h"
#include "PlayerRegistry.h"
#include "RoomBook.h"

#include "common/Events.h"

#include <string>
#include <vector>

namespace coop::server {

class Server;

bool Rooms_Enabled(Server& server); // server.json "rooms"
RoomTimes Rooms_Times(Server& server);
bool Rooms_ParsePlace(const std::string& text, RoomPlace& out); // here, entrance, anywhere
const char* Rooms_PlaceName(RoomPlace place);
bool Rooms_ValidMode(const std::string& mode); // together, each, turns, shared
const char* Rooms_StateName(RoomState state);  // lobby, starting, running, ended

// Who hears what (players in the world only).
std::vector<RemoteClient*> Rooms_Mates(Server& server, const RemoteClient& c);        // its room's others
std::vector<RemoteClient*> Rooms_PartyInScene(Server& server, const RemoteClient& c); // group + room mates, its scene
std::vector<RemoteClient*> Rooms_PartyAll(Server& server, const RemoteClient& c);     // group + room mates, any scene
const Room* Rooms_Running(Server& server, const RemoteClient& c); // the running round c directs (nullptr: none)
// The members its prizes go to: its room is running or just ended and shares prizes.
std::vector<RemoteClient*> Rooms_RewardMates(Server& server, const RemoteClient& c);
// The members a trip of c's game into key takes along: c is in that activity's room.
std::vector<RemoteClient*> Rooms_FollowMates(Server& server, const RemoteClient& c, const std::string& key);

json Rooms_StateEvent(Server& server, const Room& room); // "room" for its members
json Rooms_NoRoomEvent(const char* reason);               // "room" with id 0
json Rooms_ListEvent(Server& server);                     // "rooms": the open ones
// Everything a change means: "room" to the members of the changed rooms and id 0 to whoever is out of one, the
// invitations' ends and the texts. cause: the player whose own action explains its part (no text for it).
void Rooms_Publish(Server& server, const RoomChanges& ch, uint8_t cause);
// Here rooms: whether each waiting member is in its director's scene.
void Rooms_UpdatePresence(Server& server, int64_t nowMs, RoomChanges& ch);
// The open rooms' list to every player in the world whose last one was different.
void Rooms_SendOpenLists(Server& server);

// The actions (they answer who with a text when they refuse, and publish what changed).
void Rooms_Open(Server& server, RemoteClient& who, const std::string& key, const std::string& name,
                const std::string& mode, RoomPlace place);
void Rooms_Ready(Server& server, RemoteClient& who, int ready); // -1: the other way round
void Rooms_InviteIds(Server& server, RemoteClient& who, const std::vector<uint8_t>& to);
void Rooms_InviteGroup(Server& server, RemoteClient& who);
// accept or decline an invitation: to that room (0: any) from that player (0: any; the latest one)
void Rooms_Answer(Server& server, RemoteClient& who, uint32_t room, uint8_t from, bool accept);
void Rooms_Leave(Server& server, RemoteClient& who);
void Rooms_Kick(Server& server, RemoteClient& who, uint8_t target);
void Rooms_Close(Server& server, RemoteClient& who);
void Rooms_Join(Server& server, RemoteClient& who, uint32_t room);
void Rooms_Settings(Server& server, RemoteClient& who, int open, int travel, int rewards); // -1: unchanged
void Rooms_Chat(Server& server, RemoteClient& who, const std::string& text);
std::string Rooms_Describe(Server& server, const RemoteClient& who); // /sala
// An "act" of who's game (Handlers/ActivityHandlers.cpp): "end" of the round it directs ends it; "end" and "result"
// keep its result. -1: not known.
void Rooms_OnAct(Server& server, RemoteClient& who, const std::string& state, const std::string& key, int64_t score,
                 int64_t cs, int won);

} // namespace coop::server
