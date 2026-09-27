#pragma once
// Sub-project D: this game can run as one of the server's headless hosts. It then simulates the enemies of the
// scene it is in for everyone (the server always gives it the authority, sub-project C), keeps an invisible Link on
// the player it is told to follow (host_follow) so that player's room stays loaded, and never draws a frame.
// It is its own build: 2ship-host.exe (cmake -DCOOP_HEADLESS=ON), with no window and no GPU (libultraship's
// headless backend). The server starts it:
//   2ship-host.exe --coop-host <port> <token>   (the token proves it is the server's own)
// For testing it can also read gCoop.Port and gCoop.HostMode.Token from its 2ship2harkinian.json.
#include <cstdint>

namespace coop::client {

bool HostMode_Enabled();          // this game is a headless host
void HostMode_ParseArgs(int argc, char* argv[]); // BenPort.cpp, before the game starts

} // namespace coop::client
