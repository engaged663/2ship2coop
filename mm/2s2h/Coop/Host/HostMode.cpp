// [COOP] Sub-project D1: the headless host (HostMode.h). What it changes in this game:
//  - it connects to the server as a host (nick "#host" + the token) and enters the server's world at once;
//  - its Link is a ghost: not drawn, no colliders, cannot be hurt, and each frame stands where the player it follows
//    is (so the enemies of that room are loaded here, and the server gives us their authority);
//  - it goes to the scene of that player when they change scene;
//  - it is silent and has no window (2ship-host.exe, libultraship's headless backend): the game still runs its draw
//    pass on the CPU, because many enemies place their colliders while drawing.
#include "HostMode.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Warp.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "common/PlayerState.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/Fast3dWindow.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

bool sEnabled = false;
std::string sToken;
uint16_t sPort = kDefaultPort;
uint8_t sFollow = 0;         // the player whose room we keep loaded
int16_t sWarpedScene = -1;   // the scene we last asked to load for them
bool sWelcomed = false;      // the server accepted us once
bool sWarping = false;       // a Warp_Go we asked for is in flight
int sRetryIn = 0;            // frames until the next connection attempt (0: none pending)
constexpr int kRetryFrames = 60; // ~3 s at the game's 20 frames per second

void OnFollow(const json& ev) {
    sFollow = (uint8_t)GetInt(ev, "id");
    SPDLOG_INFO("[Coop] Host: following player {}", (int)sFollow);
}

void OnLost(const std::string& reason) {
    if (!sEnabled) {
        return;
    }
    // Once it served a world, a host without its server has nothing to do: it closes (the server starts a new one
    // when needed). Straight out: the engine's threads (audio, network) are still running and would hold a normal exit.
    if (sWelcomed) {
        SPDLOG_WARN("[Coop] Host: lost the server ({}), closing", reason);
        spdlog::shutdown();
        std::_Exit(0);
    }
    // Never accepted yet (the server is still starting, or refused us): try again in a moment.
    SPDLOG_WARN("[Coop] Host: could not join the server ({}), retrying", reason);
    sRetryIn = kRetryFrames;
}

} // namespace

bool HostMode_Enabled() {
    return sEnabled;
}

void HostMode_ParseArgs(int& argc, char* argv[]) {
    int kept = 1;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--coop-host" && i + 2 < argc) {
            sEnabled = true;
            sPort = (uint16_t)std::atoi(argv[i + 1]);
            sToken = argv[i + 2];
            i += 2;
            continue;
        }
        argv[kept++] = argv[i];
    }
    argc = kept;
    if (!sEnabled) {
        return;
    }
    // No window, no GPU; nobody reads its console, so its errors go to files next to it.
    Fast::Fast3dWindow::SetHeadless(true);
    (void)std::freopen("host-stderr.txt", "w", stderr);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    // Boots straight into the debug save (South Clock Town, never written to disk): the server needs to know its
    // scene to hand it rooms. Set here, before the logo screen runs, so DebugBoot's hook catches it.
    CVarSetInteger("gCoop.Debug.BootToClockTown", 1);
    HostCrash_Install();
}

} // namespace coop::client

namespace {

using namespace coop::client;

// The ghost: our Link stands where the followed player's Link is (their latest pose) and loads their room.
void FollowPlayer() {
    if (!WorldSession_Active() || !PoseCapture_InGameplay() || sFollow == 0 ||
        gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    coop::PlayerState pose;
    if (!PuppetManager_GetCurrent(sFollow, pose)) {
        return;
    }
    if (pose.sceneId != gPlayState->sceneId) {
        if (sWarpedScene != pose.sceneId) {
            sWarpedScene = pose.sceneId;
            WarpTarget t;
            t.entrance = pose.entrance;
            t.room = pose.roomNum;
            t.pos[0] = pose.pos[0];
            t.pos[1] = pose.pos[1];
            t.pos[2] = pose.pos[2];
            t.rot = pose.rot.y;
            if (Warp_IsValid(t)) {
                SPDLOG_INFO("[Coop] Host: going to scene {} with player {}", (int)pose.sceneId, (int)sFollow);
                sWarping = true;
                Warp_Go(t);
            }
        }
        return;
    }
    sWarpedScene = pose.sceneId;
    Player* link = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    Vec3f pos = { pose.pos[0], pose.pos[1], pose.pos[2] };
    link->actor.world.pos = pos;
    link->actor.prevPos = pos;
    link->actor.home.pos = pos;
    link->actor.shape.rot.y = pose.rot.y;
    link->actor.speed = 0.0f;
    link->actor.velocity = { 0.0f, 0.0f, 0.0f };
    // Their room: the engine loads a room when Link walks through a door; the ghost does it directly.
    if (pose.roomNum >= 0 && pose.roomNum != gPlayState->roomCtx.curRoom.num &&
        pose.roomNum < gPlayState->roomList.count && gPlayState->roomCtx.status == 0) {
        Room_RequestNewRoom(gPlayState, &gPlayState->roomCtx, pose.roomNum);
    }
}

// Invisible and untouchable: it must never be the Link an enemy goes for, nor take damage, nor push anything.
void GhostLink(Actor* actor, bool* should) {
    if (!sEnabled || actor != gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first) {
        return;
    }
    Player* link = (Player*)actor;
    link->stateFlags2 |= PLAYER_STATE2_20000000; // not drawn
    link->invincibilityTimer = 100;
    link->cylinder.base.acFlags &= ~AC_ON;
    link->cylinder.base.ocFlags1 &= ~OC1_ON;
    link->cylinder.base.atFlags &= ~AT_ON;
    gSaveContext.save.saveInfo.playerData.health = gSaveContext.save.saveInfo.playerData.healthCapacity;
}

void Connect() {
    // The token replaces the nick and the password. Entering the world is automatic after each welcome.
    CVarSetInteger("gCoop.AutoEnter", 1);
    NetClient::Get().ConnectHost("127.0.0.1", sPort, sToken);
    SPDLOG_INFO("[Coop] Host mode: connecting to port {}", sPort);
}

void HostTick() {
    sWelcomed = sWelcomed || Session_IsConnected();
    if (sRetryIn > 0 && --sRetryIn == 0) {
        Connect();
    }
    FollowPlayer();
}

// The ghost never triggers transitions (falls, voids, grabs by the engine's own rules aimed at it) and never sits
// in a text: a host frozen in a dialog would freeze the world it simulates for everyone (spec D §5.5).
void GhostConstraints() {
    if (gPlayState == nullptr) {
        return;
    }
    if (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
        sWarping = false;
        return;
    }
    if (gPlayState->transitionTrigger == TRANS_TRIGGER_START && !sWarping) {
        // A transition not asked by us (Warp_Go sets nextEntrance itself): undo it and stand where our player is.
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        gPlayState->nextEntrance = gSaveContext.save.entrance;
        SPDLOG_DEBUG("[Coop] Host: cancelled a transition aimed at the ghost");
    }
    if (gPlayState->msgCtx.msgMode != MSGMODE_NONE && gPlayState->msgCtx.msgLength != 0) {
        Message_CloseTextbox(gPlayState);
    }
}

} // namespace

static void RegisterHostMode() {
    if (!sEnabled) { // a normal game: only --coop-host makes one a host
        return;
    }
    static bool sStarted = false;
    if (!sStarted) {
        sStarted = true;
        // Silent and in the background: nobody plays it.
        CVarSetFloat("gSettings.Audio.MasterVolume", 0.0f);
        Connect();
    }
    COND_HOOK(OnGameStateMainFinish, true, HostTick);
    COND_HOOK(OnGameStateMainStart, true, GhostConstraints);
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, GhostLink);
}

COOP_ON_EVENT(hostFollow, coop::ev::kHostFollow, OnFollow);
COOP_ON_LOST(hostLost, OnLost);
static RegisterShipInitFunc sHostModeInit(RegisterHostMode);
