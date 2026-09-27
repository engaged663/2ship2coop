// [COOP] Tracks the shared enemies as they are created: the engine tells us when an actor's Init starts and ends and
// which skeletons and colliders are set up in between (CoopEngine.h). Only pieces that lie inside the actor's own
// memory are kept, so a helper that initializes some other actor's collider can never be mistaken for ours.
#include "ActorRegistry.h"

#include "ActorSync.h"
#include "CoopEngine.h"

#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <unordered_map>

extern "C" {
#include "z64.h"
}

namespace coop::client {

namespace {

std::unordered_map<const Actor*, std::unique_ptr<TrackedActor>> sTracked;
// The Inits running now, innermost last (an Init can spawn other actors, whose Init runs inside it). nullptr:
// that actor is not a shared enemy.
std::vector<TrackedActor*> sInitializing;

TrackedActor* Current() {
    return sInitializing.empty() ? nullptr : sInitializing.back();
}

bool Inside(const Actor* actor, const void* p, size_t size) {
    uintptr_t start = (uintptr_t)actor;
    uintptr_t end = start + actor->overlayEntry->profile->instanceSize;
    return (uintptr_t)p >= start && (uintptr_t)p + size <= end;
}

} // namespace

TrackedActor* ActorRegistry_Get(const Actor* actor) {
    auto it = sTracked.find(actor);
    return it == sTracked.end() ? nullptr : it->second.get();
}

TrackedActor* ActorRegistry_Find(int8_t room, uint16_t key) {
    for (auto& [actor, tracked] : sTracked) {
        if (tracked->room == room && tracked->key == key) {
            return tracked.get();
        }
    }
    return nullptr;
}

std::vector<TrackedActor*> ActorRegistry_All() {
    std::vector<TrackedActor*> out;
    for (auto& [actor, tracked] : sTracked) {
        out.push_back(tracked.get());
    }
    return out;
}

void ActorRegistry_Clear() {
    sTracked.clear();
    sInitializing.clear();
}

} // namespace coop::client

using coop::client::TrackedActor;

extern "C" void Coop_ActorInitBegin(Actor* actor) {
    using namespace coop::client;
    sInitializing.push_back(nullptr);
    if (!WorldSession_InWorld() || actor->room < 0 || actor->overlayEntry == nullptr ||
        actor->overlayEntry->profile == nullptr) {
        return;
    }
    int16_t index = GetActorListIndex(actor);
    const SharedActorDef* def = SharedActors_Find(actor->id);
    if (def == nullptr || index < 0) {
        return;
    }
    auto tracked = std::make_unique<TrackedActor>();
    tracked->actor = actor;
    tracked->key = (uint16_t)index;
    tracked->room = actor->room;
    tracked->def = def;
    sInitializing.back() = tracked.get();
    sTracked[actor] = std::move(tracked);
}

extern "C" void Coop_ActorInitEnd(Actor* actor) {
    using namespace coop::client;
    if (sInitializing.empty()) {
        return;
    }
    TrackedActor* tracked = sInitializing.back();
    sInitializing.pop_back();
    if (tracked == nullptr || tracked->actor != actor) {
        return;
    }
    if (actor->update == nullptr || tracked->skel == nullptr) {
        // Killed itself in Init (a flag says it is gone) or has no skeleton to send: not shared.
        sTracked.erase(actor);
        return;
    }
    // Enemies that start hidden (a Leever underground) clear their draw function in Init: use the profile's.
    tracked->drawFunc = actor->draw != nullptr ? actor->draw : actor->overlayEntry->profile->draw;
    SPDLOG_INFO("[Coop] Shared enemy {} (actor {:#x}) of room {} at ({:.0f}, {:.0f}, {:.0f}), {} colliders",
                tracked->key, (uint16_t)actor->id, (int)tracked->room, actor->world.pos.x, actor->world.pos.y,
                actor->world.pos.z, tracked->colliders.size());
}

extern "C" void Coop_OnSkelAnimeInit(SkelAnime* skelAnime) {
    using namespace coop::client;
    TrackedActor* t = Current();
    if (t != nullptr && t->skel == nullptr && Inside(t->actor, skelAnime, sizeof(SkelAnime))) {
        t->skel = skelAnime;
    }
}

extern "C" void Coop_OnColliderSet(Collider* collider) {
    using namespace coop::client;
    TrackedActor* t = Current();
    if (t == nullptr || collider->shape == COLSHAPE_TRIS || t->colliders.size() >= 4 ||
        !Inside(t->actor, collider, sizeof(Collider)) ||
        std::find(t->colliders.begin(), t->colliders.end(), collider) != t->colliders.end()) {
        return;
    }
    t->colliders.push_back(collider);
}

static void RegisterActorRegistry() {
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) {
        auto it = coop::client::sTracked.find(actor);
        if (it != coop::client::sTracked.end()) {
            coop::client::ActorSync_OnDestroyed(*it->second);
            coop::client::sTracked.erase(it);
        }
    });
    COND_HOOK(OnPlayDestroy, true, coop::client::ActorRegistry_Clear);
}

static RegisterShipInitFunc sActorRegistryInit(RegisterActorRegistry);
