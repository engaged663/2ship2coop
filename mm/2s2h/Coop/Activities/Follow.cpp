// [COOP] "follow" (spec §4): a mate's game goes to another entrance and takes us along: the entrance of the minigame
// we play with it, or wherever the cutscene we are watching (Features/Cinema.cpp) takes it. The same trip as
// Features/Ending.cpp: close our text, same entrance, same cutscene, same transition, as soon as we can move (a text
// or a cutscene of our own is waited for up to 8 s). Activity rooms: a trip of our room's activity takes us along
// wherever we are when it says "all" (into the minigame when the room starts, the end of the Bombers' hide-and-seek),
// otherwise only if we are in the sender's scene (into its entrance or out of it: the ones that were in it).
#include "Activities.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Room/Room.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr float kFollowDist = 3000.f;
constexpr int64_t kForceAfterMs = 8000;
constexpr int64_t kGiveUpMs = 20000;

struct Trip {
    bool active = false;
    uint16_t entrance = 0;
    uint16_t cs = 0;
    uint8_t trans = 0;
    int64_t sinceMs = 0;
    std::string key; // the activity it takes us into ("" = a cutscene's trip)
};

Trip sTrip;

bool CanGo(PlayState* play, bool force) {
    if (RoomHold_Active()) {
        return false; // our own activity waits for its room here: our trip goes first
    }
    Player* player = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF ||
        IS_PAUSED(&play->pauseCtx) || (player->stateFlags1 & PLAYER_STATE1_DEAD) ||
        play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return false;
    }
    return force || Cinema_Watching() ||
           (play->msgCtx.msgMode == MSGMODE_NONE && !Play_InCsMode(play) &&
            CutsceneManager_GetCurrentCsId() == CS_ID_NONE);
}

void Go(PlayState* play) {
    // Cada uno: our own run starts there too (the Gorman race's referee needs its state)
    const ActivityDef* def = sTrip.key.empty() ? nullptr : Activity_ByKey(sTrip.key);
    if (def != nullptr && def->hooks != nullptr && def->hooks->guestStart != nullptr &&
        Activity_IsSpecialEntrance(*def, sTrip.entrance)) {
        def->hooks->guestStart();
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        Message_CloseTextbox(play);
    }
    play->nextEntrance = sTrip.entrance;
    gSaveContext.nextCutsceneIndex = sTrip.cs;
    gSaveContext.respawnFlag = 0;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = sTrip.trans;
    gSaveContext.nextTransitionType = sTrip.trans;
    SPDLOG_INFO("[Coop] Following to entrance {:#x}", sTrip.entrance);
}

void OnFollow(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (play == nullptr || !WorldSession_Active() || from == 0) {
        return;
    }
    std::string key = GetString(ev, "key");
    bool watching = Cinema_WatchingFrom(from);
    bool guest = Guest_Director() == from;
    bool mate = !key.empty() && Group_OptMinigames() && Group_MateNear(from, kFollowDist);
    const RemotePlayer* sender = Session_FindPlayer(from);
    bool sameScene = sender != nullptr && sender->scene == play->sceneId;
    bool room = !key.empty() && Group_OptMinigames() && Room_IsMate(from) && Room_Get().key == key &&
                (GetBool(ev, "all") || sameScene);
    if (!watching && !guest && !mate && !room) {
        return;
    }
    uint16_t entrance = (uint16_t)GetInt(ev, "entrance");
    if (((entrance >> 9) & 0x7F) >= ENTR_SCENE_MAX) {
        return; // not an entrance of this game
    }
    if (!key.empty()) {
        Guest_OnFollow(from, key);
    }
    sTrip = Trip{ true, entrance, (uint16_t)GetInt(ev, "cs"), (uint8_t)GetInt(ev, "trans"), Group_NowMs(), key };
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (!sTrip.active || play == nullptr) {
        return;
    }
    int64_t waited = Group_NowMs() - sTrip.sinceMs;
    if (waited > kGiveUpMs || !WorldSession_Active()) {
        sTrip.active = false;
        return;
    }
    if (CanGo(play, waited > kForceAfterMs)) {
        Go(play);
        sTrip.active = false;
    }
}

void RegisterFollow() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

} // namespace coop::client

COOP_ON_EVENT(followEvent, coop::ev::kFollow, coop::client::OnFollow);
COOP_ON_LOST(followLost, [](const std::string&) { coop::client::sTrip.active = false; });
static RegisterShipInitFunc sFollowInit(coop::client::RegisterFollow);
