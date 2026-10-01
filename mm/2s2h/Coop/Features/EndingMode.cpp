// [COOP] The ending plays in every game at once, as in the original: nothing skipped (the world's forced skips are
// lifted), the texts advance on their own after a moment (AutoAdvanceEndingText) at the same speed everywhere, and
// everything of the co-op stands aside (no copies of actors, no other Links in the shots, no shared clock or world
// changes: Coop_InEnding). Every scripted cutscene of the ending is the same in every game (the masks are shared),
// so each game says where it is ("ending_sync": part of the ending, scene, cutscene, frame) and a game ahead of
// another one waits for it (Coop_CutsceneHold, z_demo.c): the slowest one sets the pace.
// When the last shot is reached (Coop_OnFinale) the world goes on from the Dawn of the First Day ("ending_done"); the
// ending mode lasts until that new world is built (WorldSession.cpp calls EndingMode_End).
#include "Ending.h"

#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/Coop/World/WorldSync.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <map>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int kReportEvery = 5;            // "ending_sync" 4 times a second (the game runs at 20 frames a second)
constexpr int64_t kStaleMs = 3000;         // a player not heard for this long (loading, the new-day screen, gone)
constexpr int64_t kSegmentWaitMs = 15000;  // at the start of a part, a player still in the previous one is awaited
constexpr int kAheadFrames = 4;            // ahead of someone by more than this: wait for them
constexpr int kGiveUpFrames = 400;         // ...but not for someone 20 s behind (they could never catch up)
constexpr int64_t kFinaleShowMs = 10000;   // the last shot stays this long before the world goes on
constexpr int64_t kLongestEndingMs = 30 * 60 * 1000; // no last shot after this long: the world goes on anyway

struct Peer {
    int seg = 0;
    int scene = -1;
    int cs = 0;
    int frame = 0;
    bool running = false; // its cutscene advanced since its previous report
    int64_t atMs = 0;
};

bool sActive = false;
int64_t sBeganMs = 0;
int sSeg = 0; // parts of the ending: every scene it goes through
int64_t sSegStartMs = 0;
int sFrames = 0;
int64_t sFinaleSinceMs = 0;
bool sDoneSent = false;
std::map<uint8_t, Peer> sPeers;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool CutsceneRunning(PlayState* play) {
    return gSaveContext.save.cutsceneIndex >= 0xFFF0 && play->csCtx.state != CS_STATE_IDLE;
}

// The frame of the ending's cutscene (0 before its first frame and between cutscenes).
int CutsceneFrame(PlayState* play) {
    return (CutsceneRunning(play) && play->csCtx.curFrame != 0xFFFF) ? play->csCtx.curFrame : 0;
}

void Report(PlayState* play) {
    json ev = MakeEvent(ev::kEndingSync);
    ev["seg"] = std::min(sSeg, 1000);
    ev["scene"] = play->sceneId;
    ev["cs"] = CutsceneRunning(play) ? (int)gSaveContext.save.cutsceneIndex : 0;
    ev["frame"] = CutsceneFrame(play);
    NetClient::Get().SendEvent(ev);
}

void FrameEnd() {
    if (!sActive) {
        return;
    }
    if (!WorldSession_InWorld()) {
        EndingMode_End(); // left the server's world
        return;
    }
    PlayState* play = gPlayState;
    if (play == nullptr) {
        return; // the new-day screen
    }
    if (++sFrames >= kReportEvery) {
        sFrames = 0;
        Report(play);
    }
    bool over = (sFinaleSinceMs != 0 && NowMs() - sFinaleSinceMs >= kFinaleShowMs) ||
                NowMs() - sBeganMs >= kLongestEndingMs;
    if (over && !sDoneSent) {
        sDoneSent = true;
        NetClient::Get().SendEvent(MakeEvent(ev::kEndingDone));
    }
}

void OnEndingSync(const json& ev) {
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (!sActive || from == 0 || from == Session_LocalId()) {
        return;
    }
    Peer& p = sPeers[from];
    int seg = (int)GetInt(ev, "seg");
    int scene = (int)GetInt(ev, "scene", -1);
    int cs = (int)GetInt(ev, "cs");
    int frame = (int)GetInt(ev, "frame");
    p.running = seg == p.seg && scene == p.scene && cs == p.cs && frame > p.frame;
    p.seg = seg;
    p.scene = scene;
    p.cs = cs;
    p.frame = frame;
    p.atMs = NowMs();
}

void RegisterEndingMode() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnSceneInit, true, [](s8 sceneId, s8 spawn) {
        if (sActive) {
            sSeg++; // the next part of the ending
            sSegStartMs = NowMs();
        }
    });
}

} // namespace

bool EndingMode_Active() {
    return sActive;
}

void EndingMode_End() {
    if (!sActive) {
        return;
    }
    sActive = false;
    sPeers.clear();
    WorldSession_SetEndingCVars(false);
    SPDLOG_INFO("[Coop] The ending is over");
}

void EndingMode_Begin() {
    if (sActive || !WorldSession_InWorld()) {
        return;
    }
    sActive = true;
    sBeganMs = NowMs();
    sSeg = 0;
    sSegStartMs = NowMs();
    sFrames = 0;
    sFinaleSinceMs = 0;
    sDoneSent = false;
    sPeers.clear();
    WorldSync_SetActive(false);        // the ending's own changes (the Dawn of a New Day) stay in this game
    WorldSession_SetEndingCVars(true); // nothing skipped, the same text speed in every game
    gSaveContext.save.timeSpeedOffset = 0;
    // Majora died in the game that ran her: the texts of the ending advance on their own in every game
    GameInteractor_ExecuteOnGameCompletion();
    SPDLOG_INFO("[Coop] The ending starts");
    Chat_Add(ChatKind::Ok, "Empieza el final del juego. Lo veis todos a la vez.");
}

} // namespace coop::client

using namespace coop::client;

extern "C" s32 Coop_InEnding(void) {
    return EndingMode_Active() ? 1 : 0;
}

// A player behind in the ending: this game's cutscene does not advance this frame (the slowest sets the pace).
extern "C" s32 Coop_CutsceneHold(PlayState* play) {
    if (!sActive) {
        return 0;
    }
    int64_t now = NowMs();
    int mine = CutsceneFrame(play);
    for (const auto& [id, p] : sPeers) {
        if (now - p.atMs > kStaleMs) {
            continue; // not heard lately: not waited for
        }
        if (p.seg < sSeg) {
            if (now - sSegStartMs < kSegmentWaitMs) {
                return 1; // still in the previous part: this one waits at its start
            }
            continue;
        }
        if (p.seg > sSeg || p.scene != play->sceneId || p.cs != (int)gSaveContext.save.cutsceneIndex) {
            continue; // ahead of us, or another cutscene
        }
        int64_t theirs = p.frame + (p.running ? std::min<int64_t>(now - p.atMs, 1000) / 50 : 0);
        int64_t ahead = mine - theirs;
        if (ahead > kAheadFrames && ahead < kGiveUpFrames) {
            return 1;
        }
    }
    return 0;
}

extern "C" void Coop_OnFinale(PlayState* play) {
    if (sActive && sFinaleSinceMs == 0) {
        sFinaleSinceMs = NowMs();
        Chat_Add(ChatKind::Info, "Fin. En unos segundos volvéis al Amanecer del Primer Día.");
    }
}

COOP_ON_EVENT(endingModeSync, coop::ev::kEndingSync, OnEndingSync);
static RegisterShipInitFunc sEndingModeInit(RegisterEndingMode);
