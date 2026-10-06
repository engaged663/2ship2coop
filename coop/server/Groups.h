#pragma once
// Server side of groups (spec 2026-09-30-coop-grupos-actividades §2, §9): who takes part in what, and the events and
// texts GroupBook's changes turn into. Used by Handlers/GroupHandlers.cpp, Handlers/ActivityHandlers.cpp and
// Commands/GroupCommands.cpp.
#include "GroupBook.h"
#include "PlayerRegistry.h"

#include "common/Events.h"

#include <string>
#include <vector>

namespace coop::server {

class Server;

bool Groups_Enabled(Server& server);          // server.json "groups"
bool Groups_InWorld(const RemoteClient& c);    // a player (not a host) playing in the server's world
std::vector<RemoteClient*> Groups_Mates(Server& server, const RemoteClient& c);        // its group, in the world
std::vector<RemoteClient*> Groups_MatesInScene(Server& server, const RemoteClient& c); // ...and in its scene
json Groups_Event(Server& server, const Group* g); // "group" (id 0 when g is null)
// mover appears where target is ("tp": the game warps as for /tp). roomTrip: an activity room's trip (the game retries
// it while it cannot travel).
void Groups_SendTrip(Server& server, RemoteClient& mover, const RemoteClient& target, bool roomTrip = false);
// 1..kMaxActivityKey characters of [a-z0-9_]: they name a line of the games' activity table (ActivityTable.cpp).
bool Groups_ValidActivityKey(const std::string& key);
// "group" to every member of the changed groups and to whoever is out of a group now, "invite_end" (and a text) for
// every invitation that ended. cause: the player whose own command already explains its part (no text for it).
void Groups_Publish(Server& server, const GroupChanges& ch, uint8_t cause);

} // namespace coop::server
