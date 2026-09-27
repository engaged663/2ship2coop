#pragma once
// Puts Link at an exact spot of any scene, loading it if needed (the respawn trick of DeveloperTools/WarpPoint.cpp).
// Used by /tp (Teleport.cpp) and by the shared world (entering at the saved spot, reloading after a time jump).
#include "common/Events.h"

#include <cstdint>
#include <string>

namespace coop::client {

struct WarpTarget {
    uint16_t entrance = 0; // any entrance of the scene (only its scene is used)
    int8_t room = 0;
    float pos[3] = {};
    int16_t rot = 0;
};

bool Warp_IsValid(const WarpTarget& t);             // a scene the game has, a room, a finite position in range
bool Warp_FromJson(const json& j, WarpTarget& out); // {entrance, room, pos[3], rot}: "tp" events, saved spots
json Warp_ToJson(const WarpTarget& t);
bool Warp_Current(WarpTarget& out);                 // where Link is now (false outside gameplay)
std::string Warp_BlockedReason();                   // "" = Link can be moved now; otherwise why not (Spanish)
void Warp_SetRespawn(const WarpTarget& t);          // the next scene load puts Link there
void Warp_Go(const WarpTarget& t);                  // loads t's scene now and puts Link there
void Warp_MoveInsideRoom(const WarpTarget& t);      // same scene and room: moves Link directly

} // namespace coop::client
