#pragma once
// Keeps this game's copy of the shared world in step with the server: every 100 ms of gameplay each world field is
// compared with its last synced copy (the "shadow") and the differences go out as "wops"; the server's wops are
// merged into the live save (and the shadow) as they arrive. Fields and their memory: FieldTable.h.
namespace coop::client {

void WorldSync_Reset();               // forget everything (leaving the world)
void WorldSync_Hold();                // a new world arrived: stop, and drop what was queued (it is in that world)
void WorldSync_TakeShadow();          // the save now holds the synced world (right after building it)
void WorldSync_SetActive(bool active); // true: merge what was queued and start syncing; false: send the last changes
bool WorldSync_Active();
void WorldSync_FrameEnd();
// This game's world changes to the server now, without waiting for the next send (before an upload it saves at once).
void WorldSync_Flush();            // CoopInit.cpp, end of every frame

} // namespace coop::client
