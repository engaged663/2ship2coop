#include "WorldSession.h"

#include "ClockSync.h"
#include "FieldTable.h"
#include "SaveBuilder.h"
#include "WorldSync.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Warp.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"
#include "common/Text.h"
#include "common/WorldFields.h"
#include "common/WorldRules.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include <chrono>
#include <string>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/gamestates/ovl_daytelop/z_daytelop.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
}

namespace coop::client {

namespace {

constexpr int kUploadEveryMs = 5000; // own data goes to the server this often (if it changed) and when leaving
constexpr int kSettleWaitMs = 10000; // a new world waits at most this long for a dialog, a cutscene, a scene change
                                     // or the new-day screen to end (the pause menu, as long as it stays open)

// Forced while playing in the server's world and restored when leaving: every cutscene skipped, and nothing that
// moves the clock by itself.
struct ForcedCVar {
    const char* name;
    int32_t value;
};
constexpr ForcedCVar kForcedCVars[] = {
    { "gEnhancements.Cutscenes.SkipStoryCutscenes", 1 },
    { "gEnhancements.Cutscenes.SkipEnemyCutscenes", 1 },
    { "gEnhancements.Cutscenes.SkipEntranceCutscenes", 1 },
    { "gEnhancements.Cutscenes.SkipMiscInteractions", 1 },
    { "gEnhancements.Cutscenes.SkipOnePointCutscenes", 1 },
    { "gEnhancements.Cutscenes.HideTitleCards", 1 },
    { "gEnhancements.Cutscenes.AutoAdvanceEndingText", 1 },
    { "gEnhancements.Cutscenes.SkipGetItemCutscenes", 2 },
    { "gEnhancements.Songs.SkipSoTCutscenes", 1 },
    { "gEnhancements.Songs.SkipSoaringCutscene", 1 },
    { "gEnhancements.Songs.BetterSongOfDoubleTime", 0 },
    { "gModes.TimeMovesWhenYouMove", 0 },
};

// Enhancements that read a forced CVar but register their hooks under another name: re-evaluated with them.
constexpr const char* kAlsoInit[] = { "gEnhancements.Minigames.SwampArcheryScore" };
// What the forced CVars were ("name=value;", empty value = did not exist). Kept in the settings file while they are
// forced: a game closed (or crashed) inside the server's world gets them back at its next start.
constexpr const char* kRestoreListCVar = "gCoop.World.RestoreCVars";

struct SavedCVar {
    const char* name;
    bool existed;
    int32_t value;
};

WorldState sState = WorldState::Outside;
json sPending; // the last world_full not processed yet (null: none)
int64_t sPendingSinceMs = 0;
json sBuild; // what BuildNow uses: {create, fields, inv, stale, reset}
int sCycle = 0;
bool sBuildOnDestroy = false; // the running scene ends first (it saves its flags), then the world is built
bool sResetOnDestroy = false; // the same, then the save is emptied for the file select
bool sStale = false;          // own data from an earlier cycle: the end-of-cycle rules run on entering
uint32_t sStolen = 0;         // what Takkuri had stolen from this player before those rules gave it back
bool sComputePending = false; // the server asked this game to compute the new cycle
bool sAutoEnterPending = false;
bool sCVarsForced = false;
bool sRestoreCVars = false; // done at the start of a frame, never inside the scene's end hooks
std::vector<SavedCVar> sSavedCVars;
WarpTarget sSpot; // last safe spot (on the ground) in the world
bool sHaveSpot = false;
std::string sLastUpload;
int64_t sLastUploadMs = 0;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

const json& Member(const json& j, const char* key) {
    static const json kNull;
    if (!j.is_object()) {
        return kNull;
    }
    auto it = j.find(key);
    return it != j.end() ? *it : kNull;
}

void InitForcedEnhancements() {
    for (const ForcedCVar& c : kForcedCVars) {
        ShipInit::Init(c.name); // the enhancement registers or drops its hooks
    }
    for (const char* name : kAlsoInit) {
        ShipInit::Init(name);
    }
}

void ForceCVars() {
    sRestoreCVars = false;
    if (sCVarsForced) {
        return;
    }
    sCVarsForced = true;
    sSavedCVars.clear();
    std::string list;
    for (const ForcedCVar& c : kForcedCVars) {
        SavedCVar saved = { c.name, CVarGet(c.name) != nullptr, CVarGetInteger(c.name, 0) };
        sSavedCVars.push_back(saved);
        list += std::string(c.name) + "=" + (saved.existed ? std::to_string(saved.value) : "") + ";";
        CVarSetInteger(c.name, c.value);
    }
    CVarSetString(kRestoreListCVar, list.c_str());
    CVarSave();
    InitForcedEnhancements();
}

void RestoreCVars() {
    if (!sCVarsForced) {
        return;
    }
    sCVarsForced = false;
    for (const SavedCVar& c : sSavedCVars) {
        if (c.existed) {
            CVarSetInteger(c.name, c.value);
        } else {
            CVarClear(c.name);
        }
    }
    sSavedCVars.clear();
    CVarClear(kRestoreListCVar);
    CVarSave();
    InitForcedEnhancements();
}

// First frame after starting the game: settings forced by a game that closed inside the server's world go back.
void RecoverCVars() {
    std::string list = std::string(";") + CVarGetString(kRestoreListCVar, "");
    if (list.size() == 1 || sCVarsForced) {
        return;
    }
    for (const ForcedCVar& c : kForcedCVars) {
        std::string key = std::string(";") + c.name + "=";
        size_t at = list.find(key);
        if (at == std::string::npos) {
            continue;
        }
        size_t from = at + key.size();
        std::string value = list.substr(from, list.find(';', from) - from);
        int parsed = 0;
        if (value.empty()) {
            CVarClear(c.name);
        } else if (ParseInt(value, parsed)) {
            CVarSetInteger(c.name, parsed);
        }
    }
    CVarClear(kRestoreListCVar);
    CVarSave();
    InitForcedEnhancements();
}

void Upload(bool force) {
    json inv = { { "v", 1 }, { "fields", fields::ReadPlayer() } };
    if (sHaveSpot) {
        inv["loc"] = Warp_ToJson(sSpot);
    }
    std::string text = inv.dump();
    if (!force && text == sLastUpload) {
        return;
    }
    sLastUpload = text;
    json ev = MakeEvent(ev::kInv);
    ev["inv"] = std::move(inv);
    ev["cycle"] = sCycle;
    NetClient::Get().SendEvent(ev);
}

// Where to come back: the last spot on the ground (not in a grotto: they share one scene and their exits need the
// way in).
void TrackLocation() {
    if (!PoseCapture_InGameplay() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF ||
        gPlayState->sceneId == SCENE_KAKUSIANA) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (player->stateFlags1 & PLAYER_STATE1_DEAD)) {
        return;
    }
    WarpTarget here;
    if (Warp_Current(here)) {
        sSpot = here;
        sHaveSpot = true;
    }
}

void GoToFileSelect() {
    sBuildOnDestroy = false;
    if (gPlayState != nullptr) {
        sResetOnDestroy = true; // after the scene saved its flags (WhenPlayEnds)
        STOP_GAMESTATE(&gPlayState->state);
        SET_NEXT_GAMESTATE(&gPlayState->state, FileSelect_Init, sizeof(FileSelectState));
    } else if (gGameState != nullptr) {
        SaveBuilder_ResetForFileSelect();
        STOP_GAMESTATE(gGameState);
        SET_NEXT_GAMESTATE(gGameState, FileSelect_Init, sizeof(FileSelectState));
    }
}

void LeaveWorld(const std::string& message, bool toFileSelect) {
    if (sState == WorldState::Outside) {
        return;
    }
    bool changedGame = sState == WorldState::Booting || sState == WorldState::Active;
    if (Session_IsConnected()) {
        if (WorldSession_Active()) {
            WorldSync_SetActive(false); // the last changes
            Upload(true);
        }
        NetClient::Get().SendEvent(MakeEvent(ev::kWorldLeave));
    }
    sState = WorldState::Outside;
    sPending = nullptr;
    sBuild = nullptr;
    sComputePending = false;
    sStale = false;
    sStolen = 0;
    sHaveSpot = false;
    sLastUpload.clear();
    WorldSync_Reset();
    ClockSync_Reset();
    sRestoreCVars = sCVarsForced;
    sBuildOnDestroy = false;
    if (toFileSelect && changedGame) {
        GoToFileSelect();
    }
    if (!message.empty()) {
        Chat_Add(ChatKind::Info, message);
    }
}

void BuildNow() {
    if (GetBool(sBuild, "create")) {
        SaveBuilder_NewWorld();
        json ev = MakeEvent(ev::kWorldInit);
        ev["fields"] = fields::ReadAllJson();
        NetClient::Get().SendEvent(ev);
    } else {
        SaveBuilder_LoadWorld(Member(sBuild, "fields"));
    }
    const json& inv = Member(sBuild, "inv");
    SaveBuilder_LoadPlayer(inv);
    SaveBuilder_SetClock(ClockSync_ServerJson());
    WarpTarget spot;
    // A reset (Song of Time, moon) starts at the foot of the clock tower, like the original
    sHaveSpot = GetString(sBuild, "reset").empty() && Warp_FromJson(Member(inv, "loc"), spot);
    sSpot = spot;
    SaveBuilder_PrepareStart(sHaveSpot ? &spot : nullptr, Session_LocalNick());
    WorldSync_TakeShadow();
    sStale = GetBool(sBuild, "stale");
    sLastUpload.clear();
}

void StartBuild() {
    sState = WorldState::Booting;
    sResetOnDestroy = false;
    if (gPlayState != nullptr) {
        // The running scene saves its flags when it ends: the world is built after that (WhenPlayEnds)
        sBuildOnDestroy = true;
        STOP_GAMESTATE(&gPlayState->state);
        SET_NEXT_GAMESTATE(&gPlayState->state, Play_Init, sizeof(PlayState));
        return;
    }
    BuildNow();
    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState, Play_Init, sizeof(PlayState));
}

// A new world replaces the running scene: it waits until the game is between things, so it cuts nothing off.
bool ReadyForNewWorld(bool keepLive) {
    bool waitedEnough = NowMs() - sPendingSinceMs >= kSettleWaitMs;
    if (!PoseCapture_InGameplay()) {
        // Title screen, file select: nothing to cut off. New-day screen: a Song of Time reset waits for the scene
        // after it, to keep what this game has now.
        return !keepLive || waitedEnough;
    }
    if (IS_PAUSED(&gPlayState->pauseCtx)) {
        return false; // as soon as the player closes the menu
    }
    return (fields::PlayLive() && Warp_BlockedReason().empty()) || waitedEnough;
}

void ProcessFull() {
    json ev = sPending;
    bool create = GetBool(ev, "create");
    std::string reset = GetString(ev, "reset");
    bool keepLive = reset == "sot" && sState == WorldState::Active;
    if (sState != WorldState::Booting && !ReadyForNewWorld(keepLive)) {
        return;
    }
    std::string err;
    if (!create && !fields::CheckJson(Member(ev, "fields"), &err)) {
        sPending = nullptr;
        LeaveWorld("El mundo del servidor no es válido (" + err + ").", true);
        return;
    }
    json inv = Member(Member(ev, "you"), "inv");
    bool stale = GetBool(Member(ev, "you"), "stale");
    sStolen = 0;
    if (keepLive && fields::PlayLive()) {
        // Keep what this game has now (the last upload can be 5 s old), after the end-of-cycle rules
        sStolen = gSaveContext.save.saveInfo.stolenItems;
        json live = { { "v", 1 } };
        SaveBuilder_EndOfCycleOnCopy([&live] { live["fields"] = fields::ReadPlayer(); });
        inv = live;
        stale = false;
    }
    sPending = nullptr;
    sBuild = json{ { "create", create },
                   { "fields", Member(ev, "fields") },
                   { "inv", inv },
                   { "stale", stale },
                   { "reset", reset } };
    sCycle = (int)GetInt(ev, "cycle", 1);
    ForceCVars();
    StartBuild();
}

// The end-of-cycle rules gave this player's stolen swords back only in its own copy, and the new world can come
// from another game: they go back into the world now, and WorldSync sends them to everyone.
void ReturnStolenSwords() {
    uint32_t stolen = sStolen;
    sStolen = 0;
    if (stolen == 0) {
        return;
    }
    static const int kEquipment = world::FindField("equipment");
    static const int kItems = world::FindField("items");
    std::vector<uint8_t> equipment(world::kFields[kEquipment].size);
    std::vector<uint8_t> items(world::kFields[kItems].size);
    fields::Read(kEquipment, equipment.data());
    fields::Read(kItems, items.data());
    if (world::ReturnStolenSwords(stolen, equipment.data(), items.data())) {
        fields::Write(kEquipment, equipment.data());
        fields::Write(kItems, items.data());
        Chat_Add(ChatKind::Info, "La Canción del Tiempo ha devuelto a la partida la espada que robó Takkuri.");
    }
}

void TryActivate() {
    if (sBuildOnDestroy || !PoseCapture_InGameplay() || !fields::PlayLive() || !gGameState->running ||
        gPlayState->transitionTrigger != TRANS_TRIGGER_OFF || gPlayState->transitionMode != TRANS_MODE_OFF) {
        return;
    }
    if (sStale) {
        // Own data from an earlier cycle: the Song of Time rules, as if this player had been there
        sStolen = gSaveContext.save.saveInfo.stolenItems;
        json own;
        SaveBuilder_EndOfCycleOnCopy([&own] { own = fields::ReadPlayer(); });
        fields::WritePlayer(own);
        fields::SyncSwordButton();
        SaveBuilder_RefreshButtons();
        sStale = false;
        Chat_Add(ChatKind::Info, "Tu inventario era de un ciclo anterior: vuelves con lo que conserva la Canción del "
                                 "Tiempo.");
    }
    sState = WorldState::Active;
    WorldSync_SetActive(true);
    ReturnStolenSwords();
    static bool sHeartGranted = false; // once per run: every reset builds the world and activates again
    if (!sHeartGranted && CVarGetInteger("gCoop.Debug.GrantHeartOnEnter", 0)) {
        // Test aid: a change of the shared world to watch in the other games (one more heart container)
        sHeartGranted = true;
        SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
        if (player.healthCapacity < 20 * 0x10) {
            player.healthCapacity += 0x10;
            player.health += 0x10;
        }
    }
    TrackLocation();
    Upload(true);
    sLastUploadMs = NowMs();
    Chat_Add(ChatKind::Ok, "Estás en la partida del servidor. /tiempo muestra el reloj.");
}

void ComputeCycle() {
    json shared;
    SaveBuilder_EndOfCycleOnCopy([&shared] { shared = fields::ReadAllJson(); });
    json ev = MakeEvent(ev::kCycleResult);
    ev["fields"] = std::move(shared);
    NetClient::Get().SendEvent(ev);
}

// OnPlayDestroy: the scene has saved its flags and is gone; the next game state has not started yet.
void WhenPlayEnds() {
    GameStateFunc next = gGameState != nullptr ? gGameState->init : nullptr;
    if (sBuildOnDestroy) {
        sBuildOnDestroy = false;
        if (next == Play_Init) {
            BuildNow();
        } else {
            LeaveWorld("No se pudo entrar en la partida del servidor.", false); // something else ended the scene
        }
        return;
    }
    if (sResetOnDestroy) {
        sResetOnDestroy = false;
        SaveBuilder_ResetForFileSelect();
        return;
    }
    if (WorldSession_InWorld() && next != Play_Init && next != DayTelop_Init) {
        LeaveWorld("Has salido de la partida del servidor.", false); // owl statue, reset, end of the game...
    }
}

void OnWorldFull(const json& ev) {
    if (sState == WorldState::Outside) {
        return; // left meanwhile
    }
    sPending = ev;
    sPendingSinceMs = NowMs();
    sComputePending = false;
    WorldSync_Hold();
    ClockSync_OnFull(Member(ev, "clock"));
}

void OnCycleCompute(const json&) {
    if (WorldSession_InWorld()) {
        sComputePending = true;
    }
}

void OnWelcome(const json&) {
    sAutoEnterPending = CVarGetInteger("gCoop.AutoEnter", 0) != 0; // next frame: Session has taken the welcome
}

void OnLost(const std::string&) {
    if (sState != WorldState::Outside) {
        LeaveWorld("Sin conexión con el servidor: vuelves a la selección de archivo.", true);
    }
}

void RegisterWorldSession() {
    COND_HOOK(OnPlayDestroy, true, WhenPlayEnds);
}

} // namespace

WorldState WorldSession_State() {
    return sState;
}

bool WorldSession_InWorld() {
    return sState == WorldState::Booting || sState == WorldState::Active;
}

bool WorldSession_Active() {
    return sState == WorldState::Active && sPending.is_null();
}

int WorldSession_Cycle() {
    return sCycle;
}

std::string WorldSession_StatusText() {
    switch (sState) {
        case WorldState::Outside:
            return Session_IsConnected() ? "No estás en la partida del servidor."
                                         : "Conéctate a un servidor para jugar en su partida.";
        case WorldState::Requested:
            return "Esperando al servidor...";
        case WorldState::Booting:
            return "Entrando en la partida del servidor...";
        case WorldState::Active:
            return "En la partida del servidor: " + ClockSync_Describe() + ".";
    }
    return "";
}

void WorldSession_RequestEnter() {
    if (!Session_IsConnected()) {
        Chat_Add(ChatKind::Warn, "Conéctate primero a un servidor (menú Co-op).");
        return;
    }
    if (sState != WorldState::Outside) {
        return;
    }
    sState = WorldState::Requested;
    NetClient::Get().SendEvent(MakeEvent(ev::kWorldEnter));
    Chat_Add(ChatKind::Info, "Entrando en la partida del servidor...");
}

void WorldSession_RequestLeave() {
    LeaveWorld("Has salido de la partida del servidor.", true);
}

void WorldSession_FrameStart() {
    static bool sRecovered = false;
    if (!sRecovered) {
        sRecovered = true;
        RecoverCVars();
    }
    if (sRestoreCVars) {
        sRestoreCVars = false;
        RestoreCVars();
    }
    if (sAutoEnterPending) {
        sAutoEnterPending = false;
        if (sState == WorldState::Outside && !PoseCapture_InGameplay()) { // never abandons a game being played
            WorldSession_RequestEnter();
        }
    }
    if (!sPending.is_null()) {
        ProcessFull();
    }
    if (sState == WorldState::Booting && sPending.is_null()) {
        TryActivate();
    }
    if (sComputePending && WorldSession_Active() && fields::PlayLive()) {
        sComputePending = false;
        ComputeCycle();
    }
}

void WorldSession_FrameEnd() {
    if (!WorldSession_Active()) {
        return;
    }
    TrackLocation();
    int64_t now = NowMs();
    if (now - sLastUploadMs >= kUploadEveryMs) {
        sLastUploadMs = now;
        Upload(false);
    }
}

COOP_ON_EVENT(worldSessionFull, ev::kWorldFull, OnWorldFull);
COOP_ON_EVENT(worldSessionCompute, ev::kCycleCompute, OnCycleCompute);
COOP_ON_EVENT(worldSessionWelcome, ev::kWelcome, OnWelcome);
COOP_ON_LOST(worldSessionLost, OnLost);

} // namespace coop::client

static RegisterShipInitFunc sWorldSessionInit(coop::client::RegisterWorldSession);
