// Test aids:
// - gCoop.Debug.BootToClockTown: boots straight into South Clock Town with the debug file (every item and mask,
//   never written to disk), so the co-op features can be tried in seconds.
// - gCoop.Debug.EnterEntrance: once playing in the server's world, goes once to that entrance (e.g. 21504 = Termina
//   Field): puts two test games next to the same enemies.
// - gCoop.Debug.DumpActors: every 15 s of gameplay writes the loaded actors (id, name, category, room, position) to
//   the log, to see what a scene really has.
// - gCoop.Debug.FieldSelfTest: once in gameplay, writes every shared-world and player field back to the save and
//   checks that nothing changed (the result goes to the chat and the log).
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/FieldTable.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/gamestates/ovl_select/z_select.h"
}

#define CVAR_NAME "gCoop.Debug.BootToClockTown"
// Optional: another entrance id to boot into (e.g. 54784 = North Clock Town). Default: South Clock Town.
#define CVAR_ENTRANCE "gCoop.Debug.BootEntrance"
#define CVAR_SELF_TEST "gCoop.Debug.FieldSelfTest"
#define CVAR_ENTER_ENTRANCE "gCoop.Debug.EnterEntrance"
#define CVAR_DUMP_ACTORS "gCoop.Debug.DumpActors"

static void RegisterDebugBoot() {
    COND_HOOK(OnConsoleLogoUpdate, CVarGetInteger(CVAR_NAME, 0), []() {
        if (gPlayState != nullptr) {
            return;
        }
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = 0xFF; // debug file slot: MapSelect_LoadGame creates the debug save
        u16 entrance = (u16)CVarGetInteger(CVAR_ENTRANCE, ENTRANCE(SOUTH_CLOCK_TOWN, 0));
        MapSelect_LoadGame((MapSelectState*)gGameState, entrance, 0);
    });

    COND_HOOK(OnGameStateMainFinish, CVarGetInteger(CVAR_ENTER_ENTRANCE, 0) != 0, []() {
        static bool sDone = false;
        if (sDone || !coop::client::WorldSession_Active() || !PoseCapture_InGameplay() ||
            gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
            return;
        }
        sDone = true;
        u16 entrance = (u16)CVarGetInteger(CVAR_ENTER_ENTRANCE, 0);
        SPDLOG_INFO("[Coop] Debug: going to entrance {:#x}", entrance);
        gPlayState->nextEntrance = entrance;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    });

    COND_HOOK(OnGameStateMainFinish, CVarGetInteger(CVAR_DUMP_ACTORS, 0), []() {
        if (!PoseCapture_InGameplay() || gPlayState->gameplayFrames % 300 != 100) {
            return;
        }
        SPDLOG_INFO("[Coop] Actors of scene {:#x} (frame {}):", (int)gPlayState->sceneId, gPlayState->gameplayFrames);
        for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
            for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
                const char* name = (a->id >= 0 && a->id < ACTOR_ID_MAX) ? gActorOverlayTable[a->id].name : "?";
                SPDLOG_INFO("[Coop]   cat {} id {:#x} {} room {} params {:#x} pos ({:.0f}, {:.0f}, {:.0f}){}", cat,
                            (uint16_t)a->id, name != nullptr ? name : "?", (int)a->room, (uint16_t)a->params,
                            a->world.pos.x, a->world.pos.y, a->world.pos.z, a->update == nullptr ? " [dead]" : "");
            }
        }
    });

    COND_HOOK(OnGameStateMainFinish, CVarGetInteger(CVAR_SELF_TEST, 0), []() {
        static bool sDone = false;
        if (sDone || !PoseCapture_InGameplay() || !coop::client::fields::PlayLive()) {
            return;
        }
        sDone = true;
        std::string err = coop::client::fields::SelfTest();
        if (err.empty()) {
            SPDLOG_INFO("[Coop] Field self-test OK");
            coop::client::Chat_Add(coop::client::ChatKind::Ok, "Prueba de campos del mundo: correcta.");
        } else {
            SPDLOG_ERROR("[Coop] Field self-test failed: {}", err);
            coop::client::Chat_Add(coop::client::ChatKind::Error, "Prueba de campos del mundo: " + err);
        }
    });
}

static RegisterShipInitFunc sDebugBootInit(RegisterDebugBoot,
                                                  { CVAR_NAME, CVAR_SELF_TEST, CVAR_ENTER_ENTRANCE, CVAR_DUMP_ACTORS });
