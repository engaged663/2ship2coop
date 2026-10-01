#pragma once
// [COOP] The end of the game, together. Map of the pieces:
//   Ending.cpp        the stages: the Clock Tower's rooftop, Oath to Order -> Majora's lair, Majora defeated, the
//                     ending; whoever reaches one takes everyone there
//   RooftopTimer.cpp  the rooftop's countdown (5 minutes) is the server's: the same for everyone
//   Cinema.cpp        (Features/Cinema.h) Majora's cutscenes through the camera of the game that runs her, as every boss's
//   EndingMode.cpp    the ending plays in every game at once (nothing skipped, the co-op stands aside) and the world
//                     goes on from the Dawn of the First Day when it is over
// Server side: coop/server/Handlers/EndingHandlers.cpp.

namespace coop::client {

// Ending.cpp
bool Ending_StopsTime();  // from Oath to Order on the shared clock waits (the giants hold the moon)

// EndingMode.cpp
bool EndingMode_Active(); // this game plays the ending
void EndingMode_Begin();  // Ending.cpp: the ending starts here
void EndingMode_End();    // WorldSession.cpp: the world goes on (a new world is built) or this game left it

// RooftopTimer.cpp
void RooftopTimer_Stop(); // Oath to Order: the countdown is over (the server stops everyone's)

} // namespace coop::client
