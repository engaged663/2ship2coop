// [COOP] Hits (spec §5-§6). Everything goes through the engine's collision results, so each game's own rules decide
// the outcome: the damage table of the enemy on the owner, Link's defense, masks and invincibility on the victim.
//  - Our attack hits a replica (its AC collider is registered by ActorSync.cpp): we read AC_HIT before it shows its
//    next state and send "hit"; the owner writes the same hit into its enemy's collider before that enemy updates
//    and calls CollisionCheck_ApplyDamage, as the engine does for a local hit.
//  - An enemy of ours hits a puppet (its cylinder is registered while we own a room): we send "hurt" to its player,
//    whose game writes it into its Link's cylinder before Link updates. Knockbacks without a collider
//    (func_800B8D10 aimed at a puppet) travel the same way.
#include "HitSync.h"

#include "ActorSync.h"
#include "Authority.h"
#include "CoopEngine.h"
#include "Leases.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetActor.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <deque>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
}

namespace coop::client {

namespace {

constexpr uint8_t kHitCooldownFrames = 8; // one sword swing touches for several frames: report it once

// What hit our Link, waiting for its next update.
struct PendingHurt {
    std::string kind;
    uint32_t dmgFlags = 0;
    uint8_t effect = 0;
    uint8_t damage = 0;
    uint8_t hitEffect = 0;
    Vec3f pos = {};
    float knockSpeed = 0.f;
    int16_t knockYaw = 0;
    float knockVelY = 0.f;
    int32_t knockType = 0;
    uint8_t from = 0;
};

std::deque<PendingHurt> sHurts;

// The attacker of an injected hit. Link's hit code reads the attacker's actor (direction of the knockback, a body
// hit sound flag), so it must be a real Actor: this one never enters any actor list.
Actor sHurtSource;
Collider sFakeAtCollider;
ColliderElement sFakeAtElement;

bool InWorld() {
    return WorldSession_Active() && gPlayState != nullptr;
}

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Element i of a collider (JntSph: its i-th sphere; the other shapes have one). nullptr if out of range.
ColliderElement* Element(Collider* col, int i) {
    switch (col->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jnt = (ColliderJntSph*)col;
            return (i < jnt->count && jnt->elements != nullptr) ? &jnt->elements[i].base : nullptr;
        }
        case COLSHAPE_CYLINDER:
            return i == 0 ? &((ColliderCylinder*)col)->elem : nullptr;
        case COLSHAPE_SPHERE:
            return i == 0 ? &((ColliderSphere*)col)->elem : nullptr;
        case COLSHAPE_QUAD:
            return i == 0 ? &((ColliderQuad*)col)->elem : nullptr;
        default:
            return nullptr;
    }
}

int ElementCount(Collider* col) {
    return col->shape == COLSHAPE_JNTSPH ? std::min(((ColliderJntSph*)col)->count, 32) : 1;
}

// The actor a collision pass named is still in that list of this game (never read before knowing: it may be gone).
bool InList(const Actor* who, uint8_t category) {
    for (Actor* a = gPlayState->actorCtx.actorLists[category].first; a != nullptr; a = a->next) {
        if (a == who) {
            return true;
        }
    }
    return false;
}

// An explosive of ours: a local one, or our own object the others see (Sync/PlayerObjects.cpp).
bool OwnExplosive(Actor* who) {
    TrackedActor* t = ActorRegistry_Get(who);
    return t == nullptr || t->owner == Session_LocalId();
}

// Writes an attack into collider/element exactly as CollisionCheck_SetATvsAC would, then lets the engine compute
// the damage and effect from the target's damage table.
void InjectHit(Collider* col, ColliderElement* elem, Actor* attacker, uint32_t dmgFlags, uint8_t effect,
               uint8_t damage, uint8_t hitEffect, const Vec3s& hitPos) {
    sFakeAtCollider = {};
    sFakeAtCollider.actor = attacker;
    sFakeAtCollider.atFlags = AT_ON | AT_TYPE_PLAYER | AT_TYPE_ENEMY;
    sFakeAtCollider.shape = COLSHAPE_CYLINDER;
    sFakeAtElement = {};
    sFakeAtElement.atDmgInfo.dmgFlags = dmgFlags;
    sFakeAtElement.atDmgInfo.effect = effect;
    sFakeAtElement.atDmgInfo.damage = damage;
    sFakeAtElement.atElemFlags = ATELEM_ON;

    col->acFlags |= AC_HIT;
    col->ac = attacker;
    elem->acHit = &sFakeAtCollider;
    elem->acHitElem = &sFakeAtElement;
    elem->acElemFlags |= ACELEM_HIT;
    elem->acDmgInfo.hitPos = hitPos;
    if (col->actor != nullptr) {
        col->actor->colChkInfo.acHitEffect = hitEffect;
        CollisionCheck_ApplyDamage(gPlayState, &gPlayState->colChkCtx, col, elem);
    }
}

// Writes a contact (OC) as CollisionCheck_SetOCvsOC does, with a real actor of the kind that touched it: our Link for
// a Link, or an explosive of the same kind made right there (a basket holds a bomb; a bombchu blows up at once).
void InjectContact(Collider* col, ColliderElement* elem, uint16_t attackerId, const Vec3f& pos) {
    Actor* standIn = nullptr;
    if (attackerId == ACTOR_PLAYER) {
        standIn = gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    } else if (attackerId == ACTOR_EN_BOM) {
        standIn = ActorRegistry_SpawnUntracked(ACTOR_EN_BOM, pos, BOMB_TYPE_BODY);
    } else if (attackerId == ACTOR_EN_BOM_CHU) {
        standIn = ActorRegistry_SpawnUntracked(ACTOR_EN_BOM_CHU, pos, 0);
        if (standIn != nullptr) {
            ((EnBomChu*)standIn)->timer = 1; // blows up on its next update, as it did on the guest's screen
        }
    }
    if (standIn == nullptr) {
        return;
    }
    col->ocFlags1 |= OC1_HIT;
    col->oc = standIn;
    elem->ocElemFlags |= OCELEM_HIT;
    if (standIn->category == ACTORCAT_PLAYER) {
        col->ocFlags2 |= OC2_HIT_PLAYER;
    }
}

json DamageFields(const char* type, uint32_t dmgFlags, uint8_t effect, uint8_t damage, uint8_t hitEffect,
                  const Vec3s& pos) {
    json ev = MakeEvent(type);
    ev["dmgFlags"] = dmgFlags;
    ev["effect"] = effect;
    ev["damage"] = damage;
    ev["hitEffect"] = hitEffect;
    ev["pos"] = { pos.x, pos.y, pos.z };
    return ev;
}

} // namespace

void HitSync_ClearMarks(TrackedActor& t) {
    // After a copy's new state is written: the hit flags of its colliders are stale (its own logic never runs to
    // read them); ReportReplicaHits already took the ones from this game's attacks.
    for (size_t c = 0; c < t.colliders.size() && c < 4; c++) {
        Collider* col = t.colliders[c];
        col->acFlags &= ~AC_HIT;
        col->ocFlags1 &= ~OC1_HIT; // the contacts its owner's memory brought are not ours to report
        col->oc = nullptr;
        for (int e = 0; e < ElementCount(col); e++) {
            if (ColliderElement* elem = Element(col, e); elem != nullptr) {
                elem->acElemFlags &= ~ACELEM_HIT;
                elem->acHitElem = nullptr;
                elem->ocElemFlags &= ~OCELEM_HIT;
            }
        }
    }
}

void HitSync_ReportReplicaHits(TrackedActor& t) {
    if (t.cinema) {
        return; // a cutscene actor we watch: nothing of ours reaches the game that shows it
    }
    for (size_t c = 0; c < t.colliders.size() && c < 4; c++) {
        Collider* col = t.colliders[c];
        if (t.hitCooldown[c] > 0) {
            t.hitCooldown[c]--;
        }
        if (!(col->acFlags & AC_HIT)) {
            continue;
        }
        col->acFlags &= ~AC_HIT;
        for (int e = 0; e < ElementCount(col); e++) {
            ColliderElement* elem = Element(col, e);
            if (elem == nullptr || !(elem->acElemFlags & ACELEM_HIT) || elem->acHitElem == nullptr) {
                continue;
            }
            elem->acElemFlags &= ~ACELEM_HIT;
            if (t.hitCooldown[c] > 0) {
                continue;
            }
            t.hitCooldown[c] = kHitCooldownFrames;
            const ColliderElementDamageInfoAT& at = elem->acHitElem->atDmgInfo;
            json ev = DamageFields(ev::kHit, at.dmgFlags, at.effect, at.damage, at.effect, elem->acDmgInfo.hitPos);
            ev["scene"] = gPlayState->sceneId;
            ev["room"] = t.room;
            ev["key"] = t.key;
            ev["root"] = t.rootKey; // the server routes hits by family (a lent NPC is simulated by its lessee)
            ev["col"] = c;
            ev["elem"] = e;
            ev["attackerId"] = (elem->acHit != nullptr && elem->acHit->actor != nullptr) ? elem->acHit->actor->id : 0;
            ev["form"] = GET_PLAYER_FORM;
            NetClient::Get().SendEvent(ev);
            SPDLOG_INFO("[Coop] Hit replica {} of room {} (damage {}, flags {:#x})", t.key, (int)t.room, at.damage,
                        at.dmgFlags);
            break; // one hit per collider and frame, like the engine
        }
    }
    // Contacts (Juntos): our Link or one of our explosives touched a target of the minigame a mate runs here.
    if (!Guest_ContactTarget(t)) {
        return;
    }
    Actor* link = gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    for (size_t c = 0; c < t.colliders.size() && c < 4; c++) {
        Collider* col = t.colliders[c];
        if (t.ocCooldown[c] > 0) {
            t.ocCooldown[c]--;
        }
        if (!(col->ocFlags1 & OC1_HIT) || col->oc == nullptr) {
            if (t.ocCooldown[c] == 0) {
                t.ocLast[c] = nullptr; // nothing touches it any more: the next explosive is a new one
            }
            continue;
        }
        Actor* who = col->oc;
        col->ocFlags1 &= ~OC1_HIT;
        bool ours = who == link || (InList(who, ACTORCAT_EXPLOSIVES) && OwnExplosive(who));
        bool again = who != link && who == t.ocLast[c]; // the same bomb still resting on it: it counted already
        if (!ours || again || t.ocCooldown[c] > 0) {
            continue;
        }
        t.ocCooldown[c] = kHitCooldownFrames;
        t.ocLast[c] = who;
        Vec3s pos = { (s16)who->world.pos.x, (s16)who->world.pos.y, (s16)who->world.pos.z };
        json ev = DamageFields(ev::kHit, 0, 0, 0, 0, pos);
        ev["scene"] = gPlayState->sceneId;
        ev["room"] = t.room;
        ev["key"] = t.key;
        ev["root"] = t.rootKey;
        ev["col"] = c;
        ev["elem"] = 0;
        ev["attackerId"] = who == link ? (int)ACTOR_PLAYER : (int)who->id;
        ev["form"] = GET_PLAYER_FORM;
        ev["oc"] = true;
        NetClient::Get().SendEvent(ev);
        SPDLOG_INFO("[Coop] Contact with replica {} of room {} (actor {:#x})", t.key, (int)t.room,
                    (uint16_t)(who == link ? ACTOR_PLAYER : who->id));
    }
}

void HitSync_InjectPending(TrackedActor& t) {
    std::vector<PendingHit> hits;
    hits.swap(t.pendingHits);
    for (const PendingHit& h : hits) {
        if (h.col >= t.colliders.size()) {
            continue;
        }
        Collider* col = t.colliders[h.col];
        ColliderElement* elem = Element(col, h.elem);
        if (elem == nullptr) {
            continue;
        }
        if (h.oc) {
            if (Director_Running() != nullptr) { // only the game that runs a minigame takes contacts
                InjectContact(col, elem, h.attackerId, { (f32)h.pos[0], (f32)h.pos[1], (f32)h.pos[2] });
            }
            continue;
        }
        Actor* attacker = PuppetManager_Actor(h.from);
        if (attacker == nullptr) {
            attacker = &GET_PLAYER(gPlayState)->actor; // its puppet is not here (yet): the hit still counts
        }
        InjectHit(col, elem, attacker, h.dmgFlags, h.effect, h.damage, h.hitEffect, { h.pos[0], h.pos[1], h.pos[2] });
        t.lastHitFrom = h.from; // if this blow kills it, the mods hear it was theirs (Mods/GameEvents.cpp)
        t.lastHitMs = NowMs();
        SPDLOG_INFO("[Coop] Player {} hit enemy {} of room {}: damage {} -> health {}", (int)h.from, t.key,
                    (int)t.room, h.damage, (int)t.actor->colChkInfo.health);
    }
}

void HitSync_PuppetUpdate(Actor* puppet, uint8_t playerId, PlayState* play) {
    Player* p = (Player*)puppet;
    ColliderCylinder* cyl = &p->cylinder;
    if (!InWorld() || !Authority_Known()) {
        return;
    }
    // Hit by one of our enemies during the last collision pass: tell its player.
    if ((cyl->base.acFlags & AC_HIT) && cyl->elem.acHitElem != nullptr && cyl->base.ac != nullptr &&
        ActorRegistry_Get(cyl->base.ac) != nullptr) {
        const ColliderElementDamageInfoAT& at = cyl->elem.acHitElem->atDmgInfo;
        Vec3s pos = { (s16)cyl->base.ac->world.pos.x, (s16)cyl->base.ac->world.pos.y, (s16)cyl->base.ac->world.pos.z };
        json ev = DamageFields(ev::kHurt, at.dmgFlags, at.effect, at.damage, at.effect, pos);
        ev["to"] = playerId;
        ev["kind"] = "col";
        NetClient::Get().SendEvent(ev);
        SPDLOG_INFO("[Coop] Enemy {:#x} hit player {} (damage {})", (uint16_t)cyl->base.ac->id, (int)playerId,
                    at.damage);
    }
    cyl->base.acFlags &= ~AC_HIT;
    cyl->elem.acElemFlags &= ~ACELEM_HIT;
    // Its body can be hit while we simulate enemies next to it (never while it is invincible or not drawn).
    bool owner = false;
    for (int8_t room = 0; room <= image_limits::kRoomMax && !owner; room++) {
        owner = Authority_IsMine(room);
    }
    if (owner && puppet->draw != nullptr && p->invincibilityTimer <= 0) {
        cyl->base.acFlags |= AC_ON;
        cyl->base.ocFlags1 &= ~OC1_ON; // never pushes our own Link around
        Collider_UpdateCylinder(puppet, cyl);
        CollisionCheck_SetAC(play, &play->colChkCtx, &cyl->base);
    }
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" s32 Coop_OnKnockback(PlayState* play, Actor* actor, f32 speed, s16 yaw, f32 velY, s32 type, u32 damage) {
    Player* local = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (gCoopPlayerOverride == nullptr || gCoopPlayerOverride == local || !InWorld()) {
        return false;
    }
    if (gCoopPlayerOverride->actor.id != ACTOR_EN_COOP_PUPPET) {
        return false;
    }
    uint8_t to = (uint8_t)COOP_PUPPET_GET_PLAYER_ID(&gCoopPlayerOverride->actor);
    // The engine calls this with a NULL source actor too (Majora's whip): fall back to the victim's position.
    const Vec3f& at = actor != nullptr ? actor->world.pos : gCoopPlayerOverride->actor.world.pos;
    Vec3s pos = { (s16)at.x, (s16)at.y, (s16)at.z };
    coop::json ev = DamageFields(coop::ev::kHurt, 0, 0, (uint8_t)std::min<u32>(damage, 255), 0, pos);
    ev["to"] = to;
    ev["kind"] = "knock";
    ev["knock"] = { { "speed", speed }, { "yaw", yaw }, { "velY", velY }, { "type", type } };
    NetClient::Get().SendEvent(ev);
    return true;
}

namespace {

Vec3s PosOf(const json& ev) {
    Vec3s out = { 0, 0, 0 };
    auto it = ev.find("pos");
    if (it != ev.end() && it->is_array() && it->size() == 3) {
        out.x = (s16)std::clamp((*it)[0].get<double>(), -32768.0, 32767.0);
        out.y = (s16)std::clamp((*it)[1].get<double>(), -32768.0, 32767.0);
        out.z = (s16)std::clamp((*it)[2].get<double>(), -32768.0, 32767.0);
    }
    return out;
}

// The owner: a hit another player made on our actor, applied before its next update. The server routes it by the
// family (root key) of a lent NPC; what it names is the key of the collider's actor.
void OnHit(const json& ev) {
    if (!InWorld() || GetInt(ev, "scene", -1) != gPlayState->sceneId) {
        return;
    }
    TrackedActor* t = ActorRegistry_Find((uint32_t)GetInt(ev, "key"));
    if (t == nullptr || t->actor->update == nullptr || !Leases_IsMine(*t) || t->pendingHits.size() >= 8) {
        return;
    }
    PendingHit h;
    h.from = (uint8_t)GetInt(ev, "from");
    h.col = (uint8_t)GetInt(ev, "col");
    h.elem = (uint8_t)GetInt(ev, "elem");
    h.dmgFlags = (uint32_t)GetInt(ev, "dmgFlags");
    h.effect = (uint8_t)GetInt(ev, "effect");
    h.damage = (uint8_t)GetInt(ev, "damage");
    h.hitEffect = (uint8_t)GetInt(ev, "hitEffect");
    Vec3s pos = PosOf(ev);
    h.pos[0] = pos.x;
    h.pos[1] = pos.y;
    h.pos[2] = pos.z;
    h.oc = GetBool(ev, "oc");
    h.attackerId = (uint16_t)GetInt(ev, "attackerId");
    t->pendingHits.push_back(h);
}

// The victim: an enemy of another player's game hit our Link.
void OnHurt(const json& ev) {
    if (!InWorld() || sHurts.size() >= 8) {
        return;
    }
    PendingHurt h;
    h.kind = GetString(ev, "kind");
    h.from = (uint8_t)GetInt(ev, "from");
    h.dmgFlags = (uint32_t)GetInt(ev, "dmgFlags");
    h.effect = (uint8_t)GetInt(ev, "effect");
    h.damage = (uint8_t)std::min<int64_t>(GetInt(ev, "damage"), kMaxHurtDamage);
    h.hitEffect = (uint8_t)GetInt(ev, "hitEffect");
    Vec3s pos = PosOf(ev);
    h.pos = { (f32)pos.x, (f32)pos.y, (f32)pos.z };
    if (auto k = ev.find("knock"); k != ev.end() && k->is_object()) {
        h.knockSpeed = (float)std::clamp(GetNumber(*k, "speed"), -50.0, 50.0);
        h.knockYaw = (int16_t)GetInt(*k, "yaw");
        h.knockVelY = (float)std::clamp(GetNumber(*k, "velY"), -50.0, 50.0);
        h.knockType = (int32_t)std::clamp<int64_t>(GetInt(*k, "type"), 0, 4);
    }
    sHurts.push_back(h);
}

// Before our Link updates (after this frame's collision pass): at most one hit per frame, as the engine does.
void OnShouldPlayerUpdate(Actor* actor, bool* should) {
    if (sHurts.empty() || !InWorld() || actor != gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first) {
        return;
    }
    PendingHurt h = sHurts.front();
    sHurts.pop_front();
    Player* player = (Player*)actor;
    SPDLOG_INFO("[Coop] Hurt from player {}: {} damage {}", (int)h.from, h.kind, h.damage);
    if (h.kind == "knock") {
        func_800B8D10(gPlayState, &sHurtSource, h.knockSpeed, h.knockYaw, h.knockVelY, h.knockType, h.damage);
        return;
    }
    if (player->invincibilityTimer > 0 || (player->cylinder.base.acFlags & AC_HIT)) {
        return; // Link ignores it now (just hit, rolling, invincible): so would a local hit
    }
    sHurtSource = {};
    sHurtSource.id = ACTOR_EN_COOP_PUPPET; // not ACTOR_EN_BOM: the damage is the one the owner computed
    sHurtSource.world.pos = h.pos;
    // Puppets have no shield collider, so a blocked attack reaches us as a body hit: block it here when our shield
    // is up and faces the attacker (what the shield collider would have done).
    s16 towardAttacker = (s16)(Actor_WorldYawTowardPoint(actor, &h.pos) - actor->shape.rot.y);
    if ((player->stateFlags1 & PLAYER_STATE1_400000) && !(h.dmgFlags & DMG_UNBLOCKABLE) && ABS(towardAttacker) < 0x4000) {
        player->shieldQuad.base.acFlags |= AC_BOUNCED;
        return;
    }
    InjectHit(&player->cylinder.base, &player->cylinder.elem, &sHurtSource, h.dmgFlags, h.effect, h.damage,
              h.hitEffect, { (s16)h.pos.x, (s16)h.pos.y, (s16)h.pos.z });
}

} // namespace

static void RegisterHitSync() {
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, OnShouldPlayerUpdate);
    COND_HOOK(OnPlayDestroy, true, []() { sHurts.clear(); });
}

COOP_ON_EVENT(hitEvent, coop::ev::kHit, OnHit);
COOP_ON_EVENT(hurtEvent, coop::ev::kHurt, OnHurt);
COOP_ON_LOST(hitLost, [](const std::string&) { sHurts.clear(); });
static RegisterShipInitFunc sHitSyncInit(RegisterHitSync);
