#pragma once
// [COOP] Cutscenes seen together (spec §7-§8). Map:
//   Cinema.cpp      a cutscene of this game goes to its group / to its boss's scene ("cinema", "title"); we watch
//                   theirs through their camera (Majora's lair's way, for everyone and every boss)
//   BossArenas.cpp  the bosses' rooms: the game that runs the boss shows its cutscenes to everyone there
//   TalkSync.cpp    their dialogues in our text box (TalkSync.h)
#include <cstdint>

struct PlayState;

namespace coop::client {

bool Cinema_Watching();                   // this game shows another game's cutscene camera
bool Cinema_WatchingFrom(uint8_t player); // ...that player's
bool Cinema_DirectingScene();             // our camera goes to the whole scene (a boss): our rooms stay ours
uint8_t Cinema_ActorOwner();   // who drives our cutscene actors: the game whose cutscene we watch, else us
bool Cinema_DirectingShared(); // our cutscene camera goes to others (group or scene)

bool BossArena_Is(int16_t sceneId);
bool BossArena_RunsBoss(PlayState* play); // in an arena, this game simulates its (living) boss

} // namespace coop::client
