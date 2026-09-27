#pragma once
// Follows the server's 3-day clock (sub-project B). At the start of every frame the game's time is set from the
// server's, so nothing pauses it; the Song of Double Time, the Inverted Song of Time and the Song of Time become
// requests to the server; and the game never starts the moon crash by itself (the server does, for everyone).
#include "common/Events.h"

#include <string>

namespace coop::client {

void ClockSync_OnFull(const json& serverClock); // a world_full: the server's clock at that moment
json ClockSync_ServerJson();                    // {abs, inv, stopped} as the server has it now
std::string ClockSync_Describe();               // "Día 2, 14:35 · tiempo ralentizado"
void ClockSync_Reset();                         // leaving the world
void ClockSync_FrameStart();                    // CoopInit.cpp, after WorldSession_FrameStart

} // namespace coop::client
