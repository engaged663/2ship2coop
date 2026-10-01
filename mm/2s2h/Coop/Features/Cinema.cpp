// [COOP] Cutscenes seen together (spec §7-§8), Majora's lair's way for every boss and for the group (this replaces
// MajoraCinema.cpp). A game showing a cutscene sends its camera, the screen's fill and its music every frame
// ("cinema"); the others that should see it look through that camera (a camera of their own copying it, Link waiting,
// letterbox and no HUD, as the original) until it stops coming. Who sees it:
//   - everyone in the scene ("scene"): in a boss's room the game that runs the boss (BossArenas.cpp); anywhere, a
//     cutscene that a boss or enemy we simulate started (minibosses: their entrance, their death);
//   - the group ("group"): a cutscene an NPC started, a scripted one, or any while we run a minigame, when mates are
//     near. Doors, switches and item cameras are ours alone.
// Boss title cards go with it ("title"); if a group cutscene takes the game to another scene, the ones watching go
// too ("follow", Activities/Follow.cpp). While a shared actor of ours chasing another player's puppet runs a
// cutscene, its cutscene actions for Link go to our Link (Coop_CsActionTarget): our camera is the one that shows it.
#include "Cinema.h"

#include "Ending.h"
#include "TalkSync.h"
#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include "functions.h"
#include "seqcmd.h"
#include "sequence.h"
#include "variables.h"
}

namespace coop::client {

namespace {

enum class Scope { None, Group, Scene };

constexpr int64_t kLostMs = 600;          // no camera for this long: the cutscene is over (or its game is gone)
constexpr float kGroupWatchDist = 2500.f; // mates this close watch a group cutscene
constexpr float kSceneWatchDist = 3000.f; // a miniboss's cutscene outside a boss's room reaches this far

// The seven boss title cards (their texture paths; the watcher uses its own copy of the string)
alignas(2) const char kOdolwaTitle[] = "__OTR__objects/object_boss01/gOdolwaTitleCardTex";
alignas(2) const char kTwinmoldTitle[] = "__OTR__objects/object_boss02/gTwinmoldTitleCardTex";
alignas(2) const char kGyorgTitle[] = "__OTR__objects/object_boss03/gGyorgTitleCardTex";
alignas(2) const char kGohtTitle[] = "__OTR__objects/object_boss_hakugin/gGohtTitleCardTex";
alignas(2) const char kMajoraMaskTitle[] = "__OTR__objects/object_boss07/gMajorasMaskTitleCardTex";
alignas(2) const char kMajoraIncarnationTitle[] = "__OTR__objects/object_boss07/gMajorasIncarnationTitleCardTex";
alignas(2) const char kMajoraWrathTitle[] = "__OTR__objects/object_boss07/gMajorasWrathTitleCardTex";
const char* const kBossTitles[] = { kOdolwaTitle,     kTwinmoldTitle,          kGyorgTitle,      kGohtTitle,
                                    kMajoraMaskTitle, kMajoraIncarnationTitle, kMajoraWrathTitle };

// ---- Director ----
struct Started {
    s16 csId = CS_ID_NONE;
    Actor* actor = nullptr;
};
Started sStarted;          // the last cutscene started in this game, and by whom (VB_START_CUTSCENE)
bool sEntranceCs = false;  // the scene began in a cutscene (arriving): that one is ours alone
Scope sDirecting = Scope::None;
bool sPrevStarting = false;
bool sOwnTitle = false;    // we are showing a watched title card: never sent again

// ---- Watcher ----
struct Shot {
    Vec3f eye = {};
    Vec3f at = {};
    f32 fov = 60.0f;
    s16 roll = 0;
    bool fill = false;
    u8 rgba[4] = {};
    int bgm = -1;
    int blur = -1; // motion blur alpha (-1: none)
    int64_t atMs = 0;
    uint8_t from = 0;
};
Shot sShot;
bool sHave = false;
bool sWatching = false;
uint8_t sWatchFrom = 0;
s16 sCamId = CAM_ID_NONE;
bool sFilling = false;
bool sBlurring = false; // we turned the motion blur on for the cutscene we watch
int sAskedBgm = -1;

Actor* sCsActorOnUs = nullptr; // a shared actor's cutscene put our Link in it (Coop_CsActionTarget)

Player* Link(PlayState* play) {
    return (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool IsPlayerCs(PlayState* play, s16 csId) {
    if (csId == CS_ID_GLOBAL_ELEGY || csId == CS_ID_GLOBAL_RETURN_TO_CAM || csId == CS_ID_GLOBAL_TALK ||
        csId == CS_ID_GLOBAL_DOOR) {
        return true;
    }
    for (int i = 0; i < PLAYER_CS_ID_MAX; i++) {
        if (play->playerCsIds[i] == csId) {
            return true;
        }
    }
    return false;
}

bool AnyoneElseHere(PlayState* play) {
    for (const auto& [id, p] : Session_Players()) {
        if (p.scene == play->sceneId) {
            return true;
        }
    }
    return false;
}

Scope DirectorScope(PlayState* play) {
    if (sWatching || EndingMode_Active() || play->activeCamId == CAM_ID_MAIN ||
        play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return Scope::None;
    }
    s16 cs = CutsceneManager_GetCurrentCsId();
    if (cs != CS_ID_NONE && IsPlayerCs(play, cs)) {
        return Scope::None;
    }
    if (BossArena_Is(play->sceneId)) {
        return (BossArena_RunsBoss(play) && AnyoneElseHere(play)) ? Scope::Scene : Scope::None;
    }
    if (sEntranceCs) {
        return Scope::None;
    }
    Actor* starter = (cs != CS_ID_NONE && cs == sStarted.csId) ? sStarted.actor : nullptr;
    TrackedActor* t = starter != nullptr ? ActorRegistry_Get(starter) : nullptr;
    if (t != nullptr && (starter->category == ACTORCAT_BOSS || starter->category == ACTORCAT_ENEMY) &&
        !Leases_IsRemote(*t)) {
        return AnyoneElseHere(play) ? Scope::Scene : Scope::None;
    }
    if (!Group_Has() || !Group_AnyMateNear(kGroupWatchDist)) {
        return Scope::None;
    }
    bool byNpc = t != nullptr && starter->category == ACTORCAT_NPC;
    bool scripted = (cs != CS_ID_NONE && CutsceneManager_GetCutsceneScriptIndex(cs) != CS_SCRIPT_ID_NONE) ||
                    play->csCtx.state != CS_STATE_IDLE;
    return (byNpc || scripted || Director_Running() != nullptr) ? Scope::Group : Scope::None;
}

bool Finite(const Vec3f& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

void SendShot(PlayState* play, Scope scope) {
    Camera* cam = Play_GetCamera(play, play->activeCamId);
    const View& view = play->view; // the camera plus its shake (earthquakes, impacts), as this game draws it
    if (cam == nullptr || !std::isfinite(view.fovy) || !Finite(view.eye) || !Finite(view.at)) {
        return;
    }
    json ev = MakeEvent(ev::kCinema);
    ev["eye"] = { view.eye.x, view.eye.y, view.eye.z };
    ev["at"] = { view.at.x, view.at.y, view.at.z };
    ev["fov"] = std::clamp(view.fovy, 1.0f, 179.0f);
    ev["roll"] = cam->roll;
    if (play->envCtx.fillScreen) {
        const u8* c = play->envCtx.screenFillColor;
        ev["fill"] = { c[0], c[1], c[2], c[3] };
    }
    ev["bgm"] = AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN);
    ev["scope"] = scope == Scope::Scene ? "scene" : "group";
    // The motion blur of the cutscene (Majora's forms, the moon): -1 = none
    ev["blur"] = R_MOTION_BLUR_PRIORITY_ENABLED
                     ? std::clamp<int>(R_MOTION_BLUR_PRIORITY_ALPHA, 0, 255)
                     : (R_MOTION_BLUR_ENABLED ? std::clamp<int>(R_MOTION_BLUR_ALPHA, 0, 255) : -1);
    NetClient::Get().SendEvent(ev);
}

// A group cutscene takes us to another scene: the ones watching come along (Activities/Follow.cpp).
void SendFollow(PlayState* play) {
    json ev = MakeEvent(ev::kFollow);
    ev["entrance"] = play->nextEntrance;
    ev["cs"] = gSaveContext.nextCutsceneIndex;
    ev["trans"] = play->transitionType;
    ev["scope"] = "group";
    NetClient::Get().SendEvent(ev);
}

// ---- Watching another game's camera ----

bool Wants(PlayState* play, uint8_t from, const std::string& scope) {
    if (play == nullptr || !Group_OptCutscenes() || !WorldSession_Active() || EndingMode_Active()) {
        return false;
    }
    if (scope == "scene") {
        if (BossArena_Is(play->sceneId)) {
            return true;
        }
        Actor* p = PuppetManager_Actor(from);
        return p != nullptr && Actor_WorldDistXYZToActor(&Link(play)->actor, p) < kSceneWatchDist;
    }
    return Group_IsMate(from) && (Group_MateNear(from, kGroupWatchDist) || Guest_Director() == from);
}

bool CanWatch(PlayState* play) {
    Player* player = Link(play);
    return play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF &&
           !IS_PAUSED(&play->pauseCtx) && play->gameOverCtx.state == GAMEOVER_INACTIVE &&
           !(player->stateFlags1 & PLAYER_STATE1_DEAD) && play->activeCamId == CAM_ID_MAIN &&
           play->csCtx.state == CS_STATE_IDLE && CutsceneManager_GetCurrentCsId() == CS_ID_NONE &&
           (play->msgCtx.msgMode == MSGMODE_NONE || TalkSync_Mirroring());
}

void StartWatching(PlayState* play) {
    sCamId = Play_CreateSubCamera(play);
    if (sCamId == CAM_ID_NONE) {
        return;
    }
    Cutscene_StartManual(play, &play->csCtx); // letterbox, no HUD
    Player_SetCsAction(play, NULL, PLAYER_CSACTION_WAIT);
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_WAIT);
    Play_ChangeCameraStatus(play, sCamId, CAM_STATUS_ACTIVE);
    sWatching = true;
    sWatchFrom = sShot.from;
    sFilling = false;
    sAskedBgm = -1;
    SPDLOG_INFO("[Coop] Watching the cutscene of player {}", (int)sWatchFrom);
}

void StopWatching(PlayState* play, bool restore) {
    if (restore && sCamId != CAM_ID_NONE) {
        func_80169AFC(play, sCamId, 0); // back to the main camera
        Cutscene_StopManual(play, &play->csCtx);
        Player_SetCsAction(play, NULL, PLAYER_CSACTION_END);
        if (sFilling) {
            play->envCtx.fillScreen = false;
        }
    }
    sWatching = false;
    sWatchFrom = 0;
    sCamId = CAM_ID_NONE;
    sFilling = false;
    if (sBlurring) {
        Play_DisableMotionBlur();
        sBlurring = false;
    }
}

void Apply(PlayState* play) {
    Play_SetCameraAtEye(play, sCamId, &sShot.at, &sShot.eye);
    Play_SetCameraFov(play, sCamId, sShot.fov);
    Play_SetCameraRoll(play, sCamId, sShot.roll);
    if (sShot.fill) {
        play->envCtx.fillScreen = true;
        for (int i = 0; i < 4; i++) {
            play->envCtx.screenFillColor[i] = sShot.rgba[i];
        }
        sFilling = true;
    } else if (sFilling) {
        play->envCtx.fillScreen = false;
        sFilling = false;
    }
    // The music of the cutscene (a boss's forms change it; a death stops it)
    int mine = AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN);
    if (sShot.bgm >= 0 && sShot.bgm != sAskedBgm && (sShot.bgm & 0x7FFF) != (mine & 0x7FFF)) {
        sAskedBgm = sShot.bgm;
        if (sShot.bgm == NA_BGM_DISABLED) {
            SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 20);
        } else {
            SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0, (u16)sShot.bgm);
        }
    }
    // The cutscene's motion blur (Majora's forms, the moon)
    if (sShot.blur >= 0) {
        Play_EnableMotionBlur((u32)sShot.blur);
        sBlurring = true;
    } else if (sBlurring) {
        Play_DisableMotionBlur();
        sBlurring = false;
    }
}

bool ReadVec(const json& ev, const char* key, Vec3f& out) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_array() || it->size() != 3) {
        return false;
    }
    f32 v[3];
    for (int i = 0; i < 3; i++) {
        if (!(*it)[i].is_number() || !std::isfinite((*it)[i].get<double>())) {
            return false;
        }
        v[i] = (f32)(*it)[i].get<double>();
    }
    out = { v[0], v[1], v[2] };
    return true;
}

void OnCinema(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (from == 0 || from == Session_LocalId() || !Wants(play, from, GetString(ev, "scope", "group")) ||
        (sWatching && from != sWatchFrom)) {
        return; // one camera at a time
    }
    Shot shot;
    auto fov = ev.find("fov");
    if (!ReadVec(ev, "eye", shot.eye) || !ReadVec(ev, "at", shot.at) || fov == ev.end() || !fov->is_number()) {
        return;
    }
    shot.fov = std::clamp((f32)fov->get<double>(), 1.0f, 179.0f);
    shot.roll = (s16)GetInt(ev, "roll");
    auto fill = ev.find("fill");
    if (fill != ev.end() && fill->is_array() && fill->size() == 4) {
        shot.fill = true;
        for (int i = 0; i < 4; i++) {
            shot.rgba[i] = (u8)((*fill)[i].is_number_integer() ? (*fill)[i].get<int>() : 0);
        }
    }
    int64_t bgm = GetInt(ev, "bgm", -1);
    shot.bgm = (bgm >= 0 && bgm <= 0xFFFF) ? (int)bgm : -1;
    shot.blur = (int)std::clamp<int64_t>(GetInt(ev, "blur", -1), -1, 255);
    shot.atMs = Group_NowMs();
    shot.from = from;
    sShot = shot;
    sHave = true;
}

const char* KnownTitle(const char* tex) {
    for (const char* t : kBossTitles) {
        if (std::strcmp(t, tex) == 0) {
            return t;
        }
    }
    return nullptr;
}

void OnTitle(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (from == 0 || from == Session_LocalId() || !Wants(play, from, GetString(ev, "scope", "group"))) {
        return;
    }
    std::string tex = GetString(ev, "tex");
    const char* known = KnownTitle(tex.c_str());
    if (known == nullptr) {
        return;
    }
    sOwnTitle = true;
    TitleCard_InitBossName(&play->state, &play->actorCtx.titleCtx, (TexturePtr)known,
                           (s16)std::clamp<int64_t>(GetInt(ev, "x"), -2000, 2000),
                           (s16)std::clamp<int64_t>(GetInt(ev, "y"), -2000, 2000),
                           (u8)std::clamp<int64_t>(GetInt(ev, "w"), 1, 255),
                           (u8)std::clamp<int64_t>(GetInt(ev, "h"), 1, 255));
    sOwnTitle = false;
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        return;
    }
    if (!WorldSession_Active() || EndingMode_Active()) {
        if (sWatching) {
            StopWatching(play, play->transitionTrigger == TRANS_TRIGGER_OFF);
        }
        sHave = false;
        sDirecting = Scope::None;
        return;
    }
    if (Link(play)->csAction == PLAYER_CSACTION_NONE) {
        sCsActorOnUs = nullptr;
    }
    if (sEntranceCs && play->activeCamId == CAM_ID_MAIN && play->csCtx.state == CS_STATE_IDLE &&
        CutsceneManager_GetCurrentCsId() == CS_ID_NONE) {
        sEntranceCs = false; // the arrival's cutscene is over
    }
    // Director: a transition during a group cutscene takes the watchers along
    bool starting = play->transitionTrigger == TRANS_TRIGGER_START;
    if (starting && !sPrevStarting && sDirecting == Scope::Group) {
        SendFollow(play);
    }
    sPrevStarting = starting;
    sDirecting = DirectorScope(play);
    if (sDirecting != Scope::None) {
        SendShot(play, sDirecting);
    }
    // Watcher
    bool fresh = sHave && Group_NowMs() - sShot.atMs < kLostMs;
    if (sWatching) {
        if (play->transitionTrigger != TRANS_TRIGGER_OFF) {
            StopWatching(play, false); // leaving the scene: it takes everything with it
        } else if (!fresh) {
            StopWatching(play, true);
        } else {
            Apply(play);
        }
    } else if (fresh && CanWatch(play)) {
        StartWatching(play);
        if (sWatching) {
            Apply(play);
        }
    }
}

void Forget() {
    sHave = false;
    sWatching = false; // the scene is gone: its cameras too
    sWatchFrom = 0;
    sCamId = CAM_ID_NONE;
    sFilling = false;
    if (sBlurring) {
        Play_DisableMotionBlur(); // a register: it outlives the scene
        sBlurring = false;
    }
    sStarted = Started{};
    sCsActorOnUs = nullptr;
    sDirecting = Scope::None;
    sPrevStarting = false;
}

void RegisterCinema() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, Forget);
    COND_HOOK(OnSceneInit, true, [](s8 sceneId, s8 spawn) { sEntranceCs = true; });
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) {
        if (actor == sStarted.actor) {
            sStarted = Started{};
        }
        if (actor == sCsActorOnUs) {
            sCsActorOnUs = nullptr;
        }
    });
    // Who starts each cutscene: a boss or enemy we simulate makes it the scene's, an NPC the group's
    COND_VB_SHOULD(VB_START_CUTSCENE, true, {
        s16* csId = va_arg(args, s16*);
        Actor* actor = va_arg(args, Actor*);
        if (*should && csId != nullptr) {
            sStarted.csId = *csId;
            sStarted.actor = actor;
        }
    });
}

} // namespace

bool Cinema_Watching() {
    return sWatching;
}

bool Cinema_WatchingFrom(uint8_t player) {
    return sWatching && player != 0 && player == sWatchFrom;
}

bool Cinema_DirectingScene() {
    return sDirecting == Scope::Scene;
}

uint8_t Cinema_ActorOwner() {
    return (sWatching && sWatchFrom != 0) ? sWatchFrom : Session_LocalId();
}

bool Cinema_DirectingShared() {
    return sDirecting != Scope::None;
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnBossTitleCard(PlayState* play, TexturePtr texture, s16 x, s16 y, u8 width, u8 height) {
    if (sOwnTitle || play == nullptr || texture == nullptr || !WorldSession_Active() ||
        !BossArena_Is(play->sceneId) || !BossArena_RunsBoss(play)) {
        return;
    }
    const char* tex = (const char*)texture;
    if (std::strncmp(tex, "__OTR__", 7) != 0 || KnownTitle(tex) == nullptr) {
        return;
    }
    json ev = MakeEvent(ev::kTitle);
    ev["tex"] = tex;
    ev["x"] = x;
    ev["y"] = y;
    ev["w"] = width;
    ev["h"] = height;
    ev["scope"] = "scene";
    NetClient::Get().SendEvent(ev);
}

extern "C" Player* Coop_CsActionTarget(PlayState* play, Actor* csActor) {
    Player* local = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (gCoopPlayerOverride == nullptr || gCoopPlayerOverride == local) {
        return local;
    }
    bool inCutscene = CutsceneManager_GetCurrentCsId() != CS_ID_NONE || play->csCtx.state != CS_STATE_IDLE;
    if (inCutscene || (csActor != nullptr && csActor == sCsActorOnUs)) {
        sCsActorOnUs = csActor;
        return local;
    }
    return gCoopPlayerOverride; // a grab or a push aimed at a puppet: it does nothing to our Link
}

COOP_ON_EVENT(cinemaEvent, coop::ev::kCinema, OnCinema);
COOP_ON_EVENT(cinemaTitle, coop::ev::kTitle, OnTitle);
COOP_ON_LOST(cinemaLost, [](const std::string&) { sHave = false; });
static RegisterShipInitFunc sCinemaInit(RegisterCinema);
