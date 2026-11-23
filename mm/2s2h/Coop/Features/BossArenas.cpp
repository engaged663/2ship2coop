// [COOP] The bosses' rooms (spec §8). There the game that runs the boss shows its cutscenes, its title card and its
// dialogues to everyone in the scene (Cinema.cpp, TalkSync.cpp), and nobody hands its rooms over for being in a
// cutscene (Location.cpp): the boss never changes hands in the middle of one. Minibosses anywhere go by "a boss or
// enemy we simulate started the cutscene" (Cinema.cpp). Add a room here to give another boss the same treatment.
#include "Cinema.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <set>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

struct Arena {
    int16_t scene;
    int16_t boss;
};

constexpr Arena kArenas[] = {
    { SCENE_MITURIN_BS, ACTOR_BOSS_01 },      // Odolwa
    { SCENE_HAKUGIN_BS, ACTOR_BOSS_HAKUGIN }, // Goht
    { SCENE_SEA_BS, ACTOR_BOSS_03 },          // Gyorg
    { SCENE_INISIE_BS, ACTOR_BOSS_02 },       // Twinmold
    { SCENE_LAST_BS, ACTOR_BOSS_07 },         // Majora
};

const Arena* Find(int16_t scene) {
    for (const Arena& a : kArenas) {
        if (a.scene == scene) {
            return &a;
        }
    }
    return nullptr;
}

// Twinmold's Giant's Mask (z_boss_02.c): the players whose puppet asks for the arena's switch (its Link puts the mask on
// or takes it off: PLAYER_STATE1_100 travels in the pose) and were answered already. A request lasts until that player's
// game follows the switch (its Link stops waiting): asked again meanwhile it would switch the arena back.
std::set<uint8_t> sGiantAnswered;

bool AsksGiantSwitch(uint8_t id) {
    Actor* puppet = PuppetManager_Actor(id);
    return puppet != nullptr && puppet->update != nullptr &&
           (((Player*)puppet)->stateFlags1 & PLAYER_STATE1_100) != 0;
}

void GiantMaskFrame() {
    PlayState* play = gPlayState;
    if (play == nullptr || play->sceneId != SCENE_INISIE_BS || !WorldSession_InWorld()) {
        sGiantAnswered.clear();
        return;
    }
    for (auto it = sGiantAnswered.begin(); it != sGiantAnswered.end();) {
        it = AsksGiantSwitch(*it) ? std::next(it) : sGiantAnswered.erase(it); // it stopped asking: may ask again
    }
    Boss02_CoopFollow(play);
}

void RegisterBossArenas() {
    // After our Link's update (as the original's battle handler runs after it in the actor lists' order)
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, [](Actor*) { GiantMaskFrame(); });
}

} // namespace

bool BossArena_Is(int16_t sceneId) {
    return Find(sceneId) != nullptr;
}

bool BossArena_RunsBoss(PlayState* play) {
    const Arena* arena = play != nullptr ? Find(play->sceneId) : nullptr;
    if (arena == nullptr) {
        return false;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].first; a != nullptr; a = a->next) {
        if (a->id != arena->boss || a->update == nullptr) {
            continue;
        }
        const TrackedActor* t = ActorRegistry_Get(a);
        if (t == nullptr || Leases_IsMine(*t)) {
            return true; // ours (or nobody else runs it)
        }
    }
    return false;
}

} // namespace coop::client

using namespace coop::client;

extern "C" s32 Coop_GiantMaskRequested(PlayState* play) {
    if (!WorldSession_InWorld()) {
        return 0;
    }
    for (const auto& [id, remote] : Session_Players()) {
        if (sGiantAnswered.count(id) == 0 && AsksGiantSwitch(id)) {
            sGiantAnswered.insert(id);
            return 1;
        }
    }
    return 0;
}

extern "C" void Coop_GiantMaskModeChanged(void) {
    for (const auto& [id, remote] : Session_Players()) {
        if (AsksGiantSwitch(id)) {
            sGiantAnswered.insert(id); // this switch answers them too (two players at once: one switch)
        }
    }
}

static RegisterShipInitFunc sBossArenasInit(RegisterBossArenas);
