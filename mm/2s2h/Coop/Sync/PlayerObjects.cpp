// [COOP] Sincronización total S3 (spec §4): what our Link throws, shoots or makes (kPlayerObjects) is a copy in the
// other games: it travels in the actor stream as room kPlayerRoom, with keys of ours; the copies only show it (no
// attacks, nothing to hit). A prop of the room's list our Link lifts is "adopted" the same way while carried (and until
// it rests or breaks), and the others adopt theirs. gCoop.Sync.PlayerObjects; server.json "playerObjects".
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

// The objects of our Link the others see. ADD A LINE (its Init must not change our game: CopyCode.cpp keeps the powder
// keg's flag and the magic; read the actor's Init before adding one).
const int16_t kPlayerObjects[] = {
    ACTOR_EN_ARROW,     // arrows (also fire/ice/light: their Arrow_* are its children), Deku nuts and bubbles
    ACTOR_EN_BOM,       // bombs and the powder keg
    ACTOR_EN_BOM_CHU,   // bombchus
    ACTOR_ARMS_HOOK,    // hookshot (its chain is drawn to its player's puppet)
    ACTOR_EN_BOOM,      // Zora fins
    ACTOR_EN_M_THUNDER, // the spin attack's light
    ACTOR_EN_TORCH2,    // the statues of the Elegy of Emptiness
    ACTOR_EFF_CHANGE,   // the flash of a transformation
    // Never the songs' waves (OCEFF_*): they are drawn at the camera of who sees them, a copy would fill our screen
};

constexpr int kRestFrames = 20; // a put-down prop stays ours this long once still

struct Carried {
    Actor* actor = nullptr;
    uint32_t key = 0;
    int still = 0;
};
std::vector<Carried> sCarried;

uint32_t ListKeyOf(const Actor* a) {
    int16_t index = GetActorListIndex(a);
    return (index < 0 || a->room < 0 || a->room > image_limits::kRoomMax) ? 0 : ListKey(a->room, index);
}

// A prop of the room's list nobody replicates: a pot, a rock, a bush, a crate, a bomb flower.
bool Adoptable(const Actor* a) {
    return a != nullptr && a->update != nullptr && a->category == ACTORCAT_PROP && ActorRegistry_Get(a) == nullptr &&
           ListKeyOf(a) != 0;
}

void ReleaseAll() {
    for (const Carried& c : sCarried) {
        TrackedActor* t = ActorRegistry_Find(c.key);
        if (t != nullptr && t->actor == c.actor) {
            ActorRegistry_ReleaseAdopted(c.actor);
        }
    }
    sCarried.clear();
}

// After our Link updates: what it lifts becomes ours to send; what it put down goes back once still.
void Tick(Actor*) {
    PlayState* play = gPlayState;
    Player* link = play != nullptr ? (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
    if (link == nullptr || HostMode_Enabled() || !Sync_On(SyncPart::PlayerObjects)) {
        ReleaseAll();
        return;
    }
    Actor* held = link->heldActor;
    if (Adoptable(held) && ActorRegistry_Adopt(held, ListKeyOf(held), Session_LocalId())) {
        sCarried.push_back({ held, ListKeyOf(held), 0 });
    }
    for (auto it = sCarried.begin(); it != sCarried.end();) {
        TrackedActor* t = ActorRegistry_Find(it->key);
        if (t == nullptr || t->actor != it->actor) {
            it = sCarried.erase(it); // broken: our "gone" list tells the others
            continue;
        }
        Actor* a = it->actor;
        bool still = a != link->heldActor && std::fabs(a->speed) < 0.5f && std::fabs(a->velocity.y) < 0.5f &&
                     (a->bgCheckFlags & BGCHECKFLAG_GROUND);
        it->still = still ? it->still + 1 : 0;
        if (it->still > kRestFrames) {
            ActorRegistry_ReleaseAdopted(a);
            it = sCarried.erase(it);
            continue;
        }
        ++it;
    }
}

void Forget() {
    sCarried.clear(); // the registry forgets them itself
}

void RegisterPlayerObjects() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, Tick);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

bool PlayerObjects_Listed(int16_t actorId) {
    return std::find(std::begin(kPlayerObjects), std::end(kPlayerObjects), actorId) != std::end(kPlayerObjects);
}

bool PlayerObjects_MayCreate(uint32_t key, uint32_t parentKey, uint8_t sender) {
    if ((key & kRuntimeKeyBit) == 0) {
        return false;
    }
    if (parentKey == 0) {
        return ((key >> 24) & 0x3F) == (uint32_t)(sender & 0x3F); // made by its Link: a key of its own
    }
    TrackedActor* parent = ActorRegistry_Find(parentKey);
    return parent != nullptr && parent->owner == sender; // made by one of its objects
}

TrackedActor* PlayerObjects_AdoptFor(uint32_t key, uint16_t actorId, uint8_t sender) {
    if (gPlayState == nullptr || sender == 0 || !IsListKey(key)) {
        return nullptr;
    }
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->id == (s16)actorId && Adoptable(a) && ListKeyOf(a) == key && ActorRegistry_Adopt(a, key, sender)) {
                return ActorRegistry_Find(key);
            }
        }
    }
    return nullptr;
}

static RegisterShipInitFunc sPlayerObjectsInit(RegisterPlayerObjects);

} // namespace coop::client
