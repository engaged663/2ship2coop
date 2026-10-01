// [COOP] The bosses' rooms (spec §8). There the game that runs the boss shows its cutscenes, its title card and its
// dialogues to everyone in the scene (Cinema.cpp, TalkSync.cpp), and nobody hands its rooms over for being in a
// cutscene (Location.cpp): the boss never changes hands in the middle of one. Minibosses anywhere go by "a boss or
// enemy we simulate started the cutscene" (Cinema.cpp). Add a room here to give another boss the same treatment.
#include "Cinema.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/Leases.h"

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
