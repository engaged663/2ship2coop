// [COOP] In the server's world no action stops the time (pause menu, ocarina, talking, item pick-ups, putting on a
// transformation mask): the engine asks these functions (z_actor.c, z_play.c, z_kankyo.c, marked [COOP]). The world
// keeps running, and so does the clock (it follows the server's anyway). Outside the world the game is unchanged.
// The pause menu itself (what else it stopped: Link, the picture, the sound, the pace) is LiveMenu.cpp.
#include "CoopEngine.h"

#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/World/WorldSession.h"

extern "C" {
#include "functions.h"
#include "variables.h"
}

extern "C" s32 Coop_InWorld(void) {
    // The ending plays as the original (its cutscenes freeze what they freeze)
    return (coop::client::WorldSession_Active() && !coop::client::EndingMode_Active()) ? 1 : 0;
}

extern "C" u32 Coop_FreezeStateFlags(u32 stateFlags1) {
    if (!Coop_InWorld()) {
        return stateFlags1;
    }
    // Kept: death. Dropped: putting on a mask (2), talking, item pick-ups, ocarina and the generic "time stop" bits.
    constexpr u32 kNeverFreeze = PLAYER_STATE1_2 | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |
                                 PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000;
    return stateFlags1 & ~kNeverFreeze;
}

extern "C" s32 Coop_HoldWhilePaused(PlayState* play, Actor* actor) {
    if (!IS_PAUSED(&play->pauseCtx) || !Coop_InWorld() || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return 0;
    }
    // Behind the pause menu Link runs like everything else (LiveMenu.cpp)
    if (Coop_PauseLive(play) && !Coop_LiveMenuHoldsLink(play)) {
        return 0;
    }
    Player* link = GET_PLAYER(play);
    return (actor == &link->actor || actor == link->rideActor) ? 1 : 0;
}
