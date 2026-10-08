// [COOP] Drops compartidos (Sync.h, SharedDrops.cpp): which of the actors the game creates are shared items, and the key
// of twins: what a spawner drops in every game at the same moment because it reacts to a shared flag or to the camera,
// not to one player's hit. ADD A LINE to kTwinSpawners for another such spawner.
#include "Sync.h"

#include "common/ItemState.h"

#include "2s2h/ObjectExtension/ActorListIndex.h"

extern "C" {
#include "z64.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#include "overlays/actors/ovl_Obj_Mure3/z_obj_mure3.h"
}

static_assert(ACTOR_EN_ITEM00 == coop::kItemActorCollectible, "common/ItemState.h: EN_ITEM00");
static_assert(ACTOR_EN_ELF == coop::kItemActorFairy, "common/ItemState.h: EN_ELF");
static_assert(ACTOR_EN_ELFORG == coop::kItemActorStrayFairy, "common/ItemState.h: EN_ELFORG");

namespace coop::client {

namespace {

constexpr uint32_t kNoSlot = 0xFFFFFFFFu;

// The fairies that are given away: drops, butterflies, Gossip Stones, the Sun's Song (the fountains' are made in an
// Init; Tatl and the bottle's are Link's).
bool SharedFairy(s32 type) {
    return type == FAIRY_TYPE_2 || type == FAIRY_TYPE_7;
}

// Obj_Mure3 keeps its rupees in unk148[0..5] and the seventh (the red one) in unk160.
uint32_t Mure3Slot(Actor* spawner, const Actor* item, uint32_t) {
    ObjMure3* mure = (ObjMure3*)spawner;
    for (uint32_t i = 0; i < ARRAY_COUNT(mure->unk148); i++) {
        if ((const Actor*)mure->unk148[i] == item) {
            return i;
        }
    }
    return mure->unk160 == item ? (uint32_t)ARRAY_COUNT(mure->unk148) : kNoSlot;
}

uint32_t InOrder(Actor*, const Actor*, uint32_t seq) {
    return seq;
}

struct TwinSpawner {
    int16_t id;
    uint32_t (*slot)(Actor* spawner, const Actor* item, uint32_t seq);
};
const TwinSpawner kTwinSpawners[] = {
    { ACTOR_OBJ_MURE3, Mure3Slot }, // rupee formations: every game makes them when its camera comes near (by number)
    { ACTOR_OBJ_SWPRIZE, InOrder }, // a switch's prizes: every game drops them when the switch's flag is set (in order)
};

} // namespace

DropKind DropRules_Classify(const Actor* actor, const Actor* spawner) {
    switch (actor->id) {
        case ACTOR_EN_ITEM00:
            if (GetActorListIndex(actor) >= 0 && actor->room >= 0) {
                return DropKind::ListItem; // lying in the room: every game has it
            }
            if ((actor->params & 0x8000) || spawner == nullptr) {
                return DropKind::None; // given at once in its Init; or made by a cutscene, by the co-op itself
            }
            return DropKind::Dropped;
        case ACTOR_EN_ELF:
            return (spawner != nullptr && spawner->id != ACTOR_PLAYER && SharedFairy(FAIRY_GET_TYPE(actor)))
                       ? DropKind::Dropped
                       : DropKind::None;
        case ACTOR_EN_ELFORG:
            return (spawner != nullptr && STRAY_FAIRY_TYPE(actor) == STRAY_FAIRY_TYPE_COLLECTIBLE) ? DropKind::Dropped
                                                                                                 : DropKind::None;
        default:
            return DropKind::None;
    }
}

bool DropRules_IsTwinSpawner(int16_t actorId) {
    for (const TwinSpawner& t : kTwinSpawners) {
        if (t.id == actorId) {
            return true;
        }
    }
    return false;
}

uint32_t DropRules_TwinKey(const Actor* item, Actor* spawner, uint32_t spawnerSeq) {
    int16_t index = GetActorListIndex(spawner);
    if (index < 0 || spawner->room < 0) {
        return 0;
    }
    for (const TwinSpawner& t : kTwinSpawners) {
        if (t.id == spawner->id) {
            uint32_t slot = t.slot(spawner, item, spawnerSeq);
            return slot == kNoSlot ? 0 : MakeTwinItemKey(spawner->id, MakeListItemKey(spawner->room, index), slot);
        }
    }
    return 0;
}

} // namespace coop::client
