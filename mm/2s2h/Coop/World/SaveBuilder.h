#pragma once
// Builds the save of the server's world in memory (sub-project B) and prepares the game to start playing it.
// Nothing here reads or writes the player's files: the save has fileNum 0xFF (see the [COOP] guards in the engine).
// Also the original end-of-cycle rules, run on a copy of the live save.
#include "2s2h/Coop/Features/Warp.h"

#include "common/Events.h"

#include <cstdint>
#include <functional>
#include <string>

namespace coop::client {

void SaveBuilder_NewWorld();                    // a new save with the state of SkipIntroSequence + SkipFirstCycle
void SaveBuilder_LoadWorld(const json& fields); // a new save with the world's fields (fields::CheckJson passed)
void SaveBuilder_LoadPlayer(const json& inv);   // this player's own data {v, fields, loc}; null = a new player
void SaveBuilder_SetClock(const json& serverClock); // day and time from the server's clock {abs, inv, stopped}
constexpr uint16_t kNoStartEntrance = 0xFFFF;
// What loading a file sets before Play_Init. spot == nullptr: startEntrance (a converted base-game owl save's statue,
// coop/common/SaveImport.h) when it is a real entrance, else South Clock Town.
void SaveBuilder_PrepareStart(const WarpTarget* spot, const std::string& nick,
                              uint16_t startEntrance = kNoStartEntrance);
// Runs the end-of-cycle rules (Song of Time + Dawn of the First Day) on the live save, which needs a loaded scene,
// calls readResult, and puts everything back as it was.
void SaveBuilder_EndOfCycleOnCopy(const std::function<void()>& readResult);
void SaveBuilder_RefreshButtons();     // reloads every B/C/D-pad icon (after their items changed)
void SaveBuilder_ResetForFileSelect(); // leaves the world's save behind before going to the file select

} // namespace coop::client
