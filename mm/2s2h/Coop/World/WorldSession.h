#pragma once
// Playing in the server's shared world (sub-project B): entering and leaving, building the save when a world_full
// arrives (entering, Song of Time, moon), the forced cutscene skips, the inventory uploads and computing the new
// cycle when the server asks. Menu: Co-op -> "Partida del servidor".
#include <string>

namespace coop::client {

enum class WorldState {
    Outside,   // playing something else (or nothing)
    Requested, // asked the server to enter; waiting for the world
    Booting,   // the save of the world is built; waiting for the first playable frame
    Active,    // playing it: changes are synced and the clock followed
};

WorldState WorldSession_State();
bool WorldSession_InWorld(); // Booting or Active: the save in memory is the server's world
bool WorldSession_Active();  // Active and no new world waiting
int WorldSession_Cycle();    // the world cycle of that save
std::string WorldSession_StatusText();
void WorldSession_RequestEnter();
void WorldSession_RequestLeave();
// This player's own data to the server now (save: and onto its disk at once). Only while playing in its world.
void WorldSession_UploadNow(bool save);
// The game is closing (CoopInit.cpp Coop_OnExit): leave the world properly, with the last changes sent.
void WorldSession_OnExit();
// The ending (Features/EndingMode.cpp): true lifts the forced cutscene skips and gives the settings that change the
// pace of its texts and screens their default, so it plays whole and at the same pace everywhere; false undoes it.
void WorldSession_SetEndingCVars(bool ending);
// One of the settings the co-op forces in the server's world or during the ending: the co-op needs its value, so the
// server's mods may not force it (Mods/ModSettings.cpp).
bool WorldSession_IsForcedCVar(const char* name);

// CoopInit.cpp calls these every frame: FrameStart after the network, FrameEnd at the end of the frame.
void WorldSession_FrameStart();
void WorldSession_FrameEnd();

} // namespace coop::client
