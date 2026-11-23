#pragma once
// [COOP] The way out of a grotto. Every grotto is a room of one scene (SCENE_KAKUSIANA); its exit goes back through
// the respawn point the hole set when Link jumped in (gSaveContext.respawn[RESPAWN_MODE_UNK_3]: the entrance, room and
// spot of the hole, and the chest the grotto shows). A teleport into a grotto never went through a hole: that respawn
// kept whatever the save had (entrance 0, the Mayor's Residence) and the exit left Link there, unable to move.
// A game in a grotto tells the server its way out ("loc" "grotto"); a "tp" to it carries it (Teleport.cpp).
// Tested in coop/tests/TestGrottoReturn.cpp.

#include "Events.h"

#include <cstdint>

namespace coop {

struct GrottoReturn {
    uint16_t entrance = 0;
    int8_t room = 0;
    float pos[3] = { 0.f, 0.f, 0.f };
    int16_t yaw = 0;
    uint16_t playerParams = 0; // how Link appears (PLAYER_START_MODE_GROTTO: jumping out of the hole)
    int8_t data = 0;           // the grotto's chest (its item and flag: En_Torch)
};

// False when it is missing or any value is impossible (an entrance of no scene, a room or a spot out of range).
bool GrottoReturnFromJson(const json& j, GrottoReturn& out);
json GrottoReturnToJson(const GrottoReturn& g);

} // namespace coop
