// [COOP] What a replicated actor leaves behind when it dies in the game that simulates it. There its own code makes it,
// but a copy never runs that code, and what it leaves must be each game's own: the Deku flower a Mad Scrub sat in (one
// launches from it: never replicated, ReplicationRules.cpp), the token of a Gold Skulltula (one takes it). When its
// simulator removes a copy (ActorSync.cpp), this game makes its own from the copy's last state. Also for whoever
// arrives later: the first list of what its simulator has removes the copy at once, as it died before we came.
// ADD A LINE to kLeftovers: { actor id, also when it died in front of us (else only for a late comer), what it makes }.
#include "ReplicationRules.h"

#include "ActorRegistry.h"
#include "CoopEngine.h"

#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Sw/z_en_sw.h"
#include "overlays/actors/ovl_Obj_Etcetera/z_obj_etcetera.h"
}

namespace coop::client {

namespace {

Actor* SpawnInRoom(const Actor* from, int16_t id, const Vec3f& pos, s16 rotY, s16 params) {
    gCoopSpawnRoom = from->room;
    Actor* a = ActorRegistry_SpawnUntracked(id, pos, params, rotY);
    gCoopSpawnRoom = -1;
    return a;
}

// A Mad Scrub's flower stays where it sat (z_en_dekunuts.c, func_808BE4D4). Seen dying: its health ran out there.
bool DekuFlower(Actor* scrub, bool arrival) {
    if (!arrival && scrub->colChkInfo.health != 0) {
        return false; // removed for another reason (not its death): no flower
    }
    return SpawnInRoom(scrub, ACTOR_OBJ_ETCETERA, scrub->home.pos, scrub->home.rot.y,
                       DEKU_FLOWER_PARAMS(DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE)) != nullptr;
}

// A Gold Skulltula's token, for a late comer (who was there got it as an echo, ReplicationRules.cpp): where it died,
// off its wall as z_en_sw.c does. Taken already: its flag kills the skulltula in its Init, so no copy reaches here.
bool SkulltulaToken(Actor* actor, bool arrival) {
    EnSw* sw = (EnSw*)actor;
    if (!ENSW_GET_3(actor)) {
        return false; // a plain Skullwalltula: no token
    }
    Vec3f pos = actor->world.pos;
    pos.x += sw->unk_368.x * 10.0f;
    pos.y += sw->unk_368.y * 10.0f;
    pos.z += sw->unk_368.z * 10.0f;
    return SpawnInRoom(actor, ACTOR_EN_SI, pos, 0, actor->params) != nullptr;
}

struct Leftover {
    int16_t id;
    bool seenDying; // also when its simulator removes it in front of us (else only for a late comer)
    bool (*make)(Actor* copy, bool arrival); // false: nothing made
};

// ADD A LINE (its own code must make it in the game that simulates it, and nothing replicates it: an echo or a never
// replicated actor; a late-comer-only line when the echo already gives it to who is there)
const Leftover kLeftovers[] = {
    { ACTOR_EN_DEKUNUTS, true, DekuFlower },  // Mad Scrubs: the flower they sat in (Woodfall's way to the temple)
    { ACTOR_EN_SW, false, SkulltulaToken },   // Gold Skulltulas: the token
};

} // namespace

void Leftovers_OnCopyRemoved(Actor* copy, bool arrival) {
    if (copy == nullptr || gPlayState == nullptr) {
        return;
    }
    for (const Leftover& l : kLeftovers) {
        if (l.id == copy->id && (arrival || l.seenDying)) {
            if (l.make(copy, arrival)) {
                SPDLOG_INFO("[Coop] Actor {:#x} died in its simulator's game{}: what it leaves made here",
                            (uint16_t)copy->id, arrival ? " before we came" : "");
            }
            return;
        }
    }
}

} // namespace coop::client
