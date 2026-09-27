#pragma once
// Sub-project D: this game can run as one of the server's headless hosts. It then simulates the enemies of the
// scene it is in for everyone (the server always gives it the authority, sub-project C), keeps an invisible Link on
// the player it is told to follow (host_follow) so that player's room stays loaded, and never draws a frame.
// The same 2ship.exe as the players' (D3 copies pointers into the game's code), started by the server with:
//   2ship.exe --coop-host <port> <token>   (the token proves it is the server's own)
// With that argument it has no window and no GPU (libultraship's headless backend, chosen before the window).
#include <cstdint>

namespace coop::client {

bool HostMode_Enabled();          // this game is a headless host
// BenPort.cpp, before the game starts. Removes its own arguments from argv (and lowers argc): the extractor takes
// every argument left for a ROM to extract, and a "not a ROM" popup could never be closed without a window.
void HostMode_ParseArgs(int& argc, char* argv[]);
void HostCrash_Install();         // HostCrash.cpp: abort stack to host-crash.txt

} // namespace coop::client
