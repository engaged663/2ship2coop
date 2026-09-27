#include "SharedActors.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "z64.h"
#include "overlays/actors/ovl_En_Bat/z_en_bat.h"
#include "overlays/actors/ovl_En_Crow/z_en_crow.h"
#include "overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"
#include "overlays/actors/ovl_En_Grasshopper/z_en_grasshopper.h"
#include "overlays/actors/ovl_En_Karebaba/z_en_karebaba.h"
#include "overlays/actors/ovl_En_Mkk/z_en_mkk.h"
#include "overlays/actors/ovl_En_Skb/z_en_skb.h"
#include "overlays/actors/ovl_En_Slime/z_en_slime.h"
#include "overlays/actors/ovl_En_Tite/z_en_tite.h"
#include "overlays/actors/ovl_En_Wf/z_en_wf.h"
}

namespace coop::client {

namespace {

uint8_t ToByte(float v, float unit) {
    return (uint8_t)std::clamp(std::lround(v * unit), 0l, 255l);
}

// The damage effect (fire, ice, light...) drawn over most enemies: type, alpha and scale. Every enemy that has it
// uses the same three field names, so one template covers them all.
template <class T> void WriteDamageEffect(Actor* actor, std::vector<uint8_t>& out) {
    T* self = (T*)actor;
    out = { (uint8_t)self->drawDmgEffType, ToByte(self->drawDmgEffAlpha, 255.f), ToByte(self->drawDmgEffScale, 100.f) };
}

template <class T> void ReadDamageEffect(Actor* actor, const std::vector<uint8_t>& in) {
    if (in.size() < 3) {
        return;
    }
    T* self = (T*)actor;
    self->drawDmgEffType = in[0];
    self->drawDmgEffAlpha = in[1] / 255.f;
    self->drawDmgEffScale = in[2] / 100.f;
}

// Dragonflies have no alpha field.
void WriteGrasshopper(Actor* actor, std::vector<uint8_t>& out) {
    EnGrasshopper* self = (EnGrasshopper*)actor;
    out = { (uint8_t)self->drawDmgEffType, ToByte(self->drawDmgEffScale, 100.f) };
}

void ReadGrasshopper(Actor* actor, const std::vector<uint8_t>& in) {
    if (in.size() >= 2) {
        EnGrasshopper* self = (EnGrasshopper*)actor;
        self->drawDmgEffType = in[0];
        self->drawDmgEffScale = in[1] / 100.f;
    }
}

// Boes fade in and out.
void WriteMkk(Actor* actor, std::vector<uint8_t>& out) {
    out = { ((EnMkk*)actor)->alpha };
}

void ReadMkk(Actor* actor, const std::vector<uint8_t>& in) {
    if (!in.empty()) {
        ((EnMkk*)actor)->alpha = in[0];
    }
}

#define DAMAGE_EFFECT(T) WriteDamageEffect<T>, ReadDamageEffect<T>

const SharedActorDef kSharedActors[] = {
    { ACTOR_EN_DEKUBABA, DAMAGE_EFFECT(EnDekubaba) },     // Deku Baba
    { ACTOR_EN_KAREBABA, DAMAGE_EFFECT(EnKarebaba) },     // Mini Baba
    { ACTOR_EN_SLIME, DAMAGE_EFFECT(EnSlime) },           // Chuchu
    { ACTOR_EN_FIREFLY, DAMAGE_EFFECT(EnFirefly) },       // Keese
    { ACTOR_EN_BAT, DAMAGE_EFFECT(EnBat) },               // Bad Bat
    { ACTOR_EN_SKB, DAMAGE_EFFECT(EnSkb) },               // Stalchild
    { ACTOR_EN_WF, DAMAGE_EFFECT(EnWf) },                 // Wolfos
    { ACTOR_EN_TITE, DAMAGE_EFFECT(EnTite) },             // Tektite
    { ACTOR_EN_CROW, DAMAGE_EFFECT(EnCrow) },             // Guay
    { ACTOR_EN_GRASSHOPPER, WriteGrasshopper, ReadGrasshopper }, // Dragonfly
    { ACTOR_EN_NEO_REEBA, nullptr, nullptr },             // Leever
    { ACTOR_EN_MKK, WriteMkk, ReadMkk },                  // Boe
    { ACTOR_EN_SB, nullptr, nullptr },                    // Shellblade
};

#undef DAMAGE_EFFECT

} // namespace

const SharedActorDef* SharedActors_Find(int16_t actorId) {
    for (const SharedActorDef& def : kSharedActors) {
        if (def.actorId == actorId) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace coop::client
