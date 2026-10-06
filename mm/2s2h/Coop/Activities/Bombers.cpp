// [COOP] The Bombers' hide-and-seek in the server's world (spec 2026-10-04-coop-salas-actividades §5.6). Jim sets it
// off (WEEKEVENTREG_73_10 as a Deku, 85_02 as a human) and the five Bombers (En_Bomjimb) hide all over Clock Town;
// catching one sets its flags (WEEKEVENTREG_11_xx, 76_xx) and adds one to bombersCaughtNum, and the fifth sends
// whoever caught it to Jim (75_40) for the code. The flags are the world's but the count is each player's own: with
// two players catching, nobody ever reached five. Now, while it runs, every game takes its count from the world's
// flags (the order is completed), says when another game caught one, and the game that catches the last one takes its
// whole room along to Jim (Director_FollowAll). Its line in ActivityTable.cpp ("bombers": Shared, scene -1) uses the
// hooks below: it runs while it is set off and not finished (wherever we are), this game set it off when the flags
// appear while it simulates Jim, and its room's window shows "n/5".
#include "Activities.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int kBombers = 5;
constexpr int64_t kOwnCatchMs = 15000;  // our last catch this recent: the trip to Jim that follows is ours
constexpr int64_t kStartLatchMs = 2000; // the Director takes "this game set it off" within this long

// The flags of the five, in the order of their ENBOMJIMB_F_0..4 (z_en_bomjimb.c)
bool Caught(int i) {
    switch (i) {
        case 0:
            return CHECK_WEEKEVENTREG(WEEKEVENTREG_11_01);
        case 1:
            return CHECK_WEEKEVENTREG(WEEKEVENTREG_11_02);
        case 2:
            return CHECK_WEEKEVENTREG(WEEKEVENTREG_11_04);
        case 3:
            return CHECK_WEEKEVENTREG(WEEKEVENTREG_11_08);
        case 4:
            return CHECK_WEEKEVENTREG(WEEKEVENTREG_11_10);
        default:
            return false;
    }
}

int CaughtCount() {
    int n = 0;
    for (int i = 0; i < kBombers; i++) {
        n += Caught(i) ? 1 : 0;
    }
    return n;
}

bool SetOff() {
    return CHECK_WEEKEVENTREG(WEEKEVENTREG_73_10) || CHECK_WEEKEVENTREG(WEEKEVENTREG_85_02);
}

bool Finished() {
    return CHECK_WEEKEVENTREG(WEEKEVENTREG_75_40);
}

// Jim (or one of the Bombers running off in his cutscene: En_Bomjima too) updates in this game: the hide-and-seek that
// appears now is the one our Link just accepted.
bool OurJim(PlayState* play) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_NPC].first; a != nullptr; a = a->next) {
        if (a->id != ACTOR_EN_BOMJIMA || a->update == nullptr) {
            continue;
        }
        TrackedActor* t = ActorRegistry_Get(a);
        if (t == nullptr || Leases_IsMine(*t)) {
            return true;
        }
    }
    return false;
}

bool sPrevSetOff = false;
bool sKnown = false;         // sPrevSetOff holds last frame's value
int64_t sStartedMs = -kStartLatchMs; // this game set it off then (taken by the Director once)
int sLastCount = -1;          // the count after our last look (-1: not running)
int64_t sOwnCatchMs = -kOwnCatchMs;
bool sPrevStarting = false;
bool sPrevFinished = false;

// While it runs: the count is the number of Bombers the world says are caught (the order: the ones it had, then the
// rest in their order).
void SyncCount() {
    SaveInfo& info = gSaveContext.save.saveInfo;
    if (!Bombers_Active()) {
        sLastCount = -1;
        return;
    }
    int n = CaughtCount();
    int have = info.bombersCaughtNum;
    if (sLastCount >= 0 && have == sLastCount + 1 && n == have) {
        sOwnCatchMs = Group_NowMs(); // the engine added this one: our Link caught it
    }
    if (have != n) {
        if (sLastCount >= 0 && n > have) {
            Chat_Add(ChatKind::Info, "¡Han atrapado a un Bomber! (" + std::to_string(n) + "/5)");
        }
        s8* order = info.bombersCaughtOrder;
        bool used[kBombers] = {};
        int k = 0;
        for (int i = 0; i < kBombers && i < have && k < n; i++) {
            int idx = order[i] - 1;
            if (idx >= 0 && idx < kBombers && Caught(idx) && !used[idx]) {
                order[k++] = (s8)(idx + 1);
                used[idx] = true;
            }
        }
        for (int idx = 0; idx < kBombers && k < n; idx++) {
            if (Caught(idx) && !used[idx]) {
                order[k++] = (s8)(idx + 1);
                used[idx] = true;
            }
        }
        for (int i = k; i < kBombers; i++) {
            order[i] = 0;
        }
        info.bombersCaughtNum = (s8)n;
        SPDLOG_INFO("[Coop] Bombers caught: {} (this game had {})", n, have);
    }
    sLastCount = n;
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (!WorldSession_Active() || play == nullptr || !PoseCapture_InGameplay()) {
        sKnown = false;
        return;
    }
    bool setOff = SetOff();
    if (sKnown && setOff && !sPrevSetOff && OurJim(play)) {
        sStartedMs = Group_NowMs();
        SPDLOG_INFO("[Coop] This game set off the Bombers' hide-and-seek");
    }
    sPrevSetOff = setOff;
    sKnown = true;
    SyncCount();
    // The end: whoever catches the fifth goes to Jim; the room comes along
    bool finished = Finished();
    bool starting = play->transitionTrigger == TRANS_TRIGGER_START;
    if (starting && !sPrevStarting && finished && Group_NowMs() - sOwnCatchMs < kOwnCatchMs) {
        if (const ActivityDef* def = Activity_ByKey("bombers")) {
            Director_FollowAll(play, *def);
        }
        sOwnCatchMs = -kOwnCatchMs;
    } else if (finished && !sPrevFinished && setOff && Group_NowMs() - sOwnCatchMs >= kOwnCatchMs) {
        Chat_Add(ChatKind::Ok, "¡Los cinco Bombers atrapados! Jim espera en el norte de la ciudad con el código.");
    }
    sPrevStarting = starting;
    sPrevFinished = finished;
}

void Reset() {
    sKnown = false;
    sStartedMs = -kStartLatchMs;
    sLastCount = -1;
    sOwnCatchMs = -kOwnCatchMs;
    sPrevStarting = false;
    sPrevFinished = false;
}

void RegisterBombers() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

bool Bombers_Active() {
    return SetOff() && !Finished();
}

bool Bombers_StartedHere(PlayState* play) {
    (void)play;
    if (Group_NowMs() - sStartedMs >= kStartLatchMs) {
        return false;
    }
    sStartedMs = -kStartLatchMs; // once
    return Bombers_Active();
}

bool Bombers_Progress(char* out, size_t size) {
    if (!Bombers_Active() || out == nullptr || size == 0) {
        return false;
    }
    const char* form = CHECK_WEEKEVENTREG(WEEKEVENTREG_73_10) ? "forma deku" : "forma humana";
    std::snprintf(out, size, "Bombers atrapados: %d/5 (%s)", CaughtCount(), form);
    return true;
}

} // namespace coop::client

COOP_ON_EVENT(bombersWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(bombersLost, [](const std::string&) { coop::client::Reset(); });
static RegisterShipInitFunc sBombersInit(coop::client::RegisterBombers);
