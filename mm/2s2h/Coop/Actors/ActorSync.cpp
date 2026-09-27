// [COOP] Shared enemies (spec §4). The owner of a room (Authority.h) runs its enemies and sends their state once per
// frame; every other game in the room keeps them as replicas: their update never runs (ShouldActorUpdate), and they
// copy the owner's position, pose, health and draw state, register their colliders so our attacks can hit them
// (HitSync.cpp reports those hits) and play their sounds. The owner's enemies chase the nearest Link, local or a
// puppet (Coop_ActorUpdateBegin), and keep updating while another player is next to them.
#include "ActorSync.h"

#include "Authority.h"
#include "CoopEngine.h"
#include "HitSync.h"

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
#include <deque>
#include <map>

extern "C" {
#include "functions.h"
#include "variables.h"
}

extern "C" Player* gCoopPlayerOverride = nullptr;

namespace coop::client {

namespace {

using Clock = std::chrono::steady_clock;

constexpr float kKeepUpdatingDist = 1200.f; // an enemy this close to a puppet updates even off our camera
constexpr float kSwitchTargetRatio = 0.8f;  // another Link must be 20 % closer to become the target
constexpr int64_t kGoneAfterMs = 1000;      // a replica the owner stopped listing for this long is gone
constexpr int64_t kGoneMemoryMs = 1000;     // the owner lists a destroyed enemy as gone for this long
constexpr size_t kBufferedFrames = 2;       // replicas play a frame once two are queued (absorbs jitter)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// A frame of one room from its owner (all its parts together).
struct RoomFrame {
    uint8_t owner = 0;
    uint16_t seq = 0;
    std::vector<ActorRecord> actors;
    std::vector<uint16_t> gone;
};

struct RoomState {
    std::deque<RoomFrame> ready;   // complete frames not applied yet
    RoomFrame building;            // parts of the frame being received
    uint8_t partsSeen = 0;
    uint8_t partsNeeded = 0;
    bool listed = false;           // a complete frame arrived since we entered: replicas may move
    std::vector<std::pair<uint16_t, int64_t>> recentlyGone; // owner: keys destroyed and when
    uint16_t sendSeq = 0;
};

std::map<int8_t, RoomState> sRooms;
TrackedActor* sUpdating = nullptr; // the owner's enemy whose update is running

bool Active() {
    return WorldSession_Active() && gPlayState != nullptr && Authority_Known();
}

void Forget() {
    sRooms.clear();
    sUpdating = nullptr;
    gCoopPlayerOverride = nullptr;
}

// ---- Receiving (replicas) ----

void OnActors(const uint8_t* data, size_t size) {
    ActorPacket p;
    if (!Active() || !DecodeActorPacket(data, size, p) || p.scene != gPlayState->sceneId ||
        Authority_Owner(p.room) != p.playerId || p.playerId == Session_LocalId()) {
        return; // another scene, a stale owner (a handover in flight), or not ours to show
    }
    RoomState& room = sRooms[p.room];
    if (p.part == 0 || p.seq != room.building.seq || p.playerId != room.building.owner) {
        room.building = RoomFrame{ p.playerId, p.seq, {}, {} };
        room.partsSeen = 0;
        room.partsNeeded = p.parts;
    }
    room.building.gone.insert(room.building.gone.end(), p.gone.begin(), p.gone.end());
    for (ActorRecord& a : p.actors) {
        room.building.actors.push_back(std::move(a));
    }
    if (++room.partsSeen < room.partsNeeded) {
        return; // a lost part only costs this frame: the next one is complete again
    }
    room.ready.push_back(std::move(room.building));
    room.building = RoomFrame{};
    room.partsSeen = 0;
    while (room.ready.size() > kBufferedFrames + 3) {
        room.ready.pop_front(); // far behind: skip ahead
    }
}

void KillReplica(TrackedActor& t) {
    t.actor->dropFlag = 0; // the owner's enemy already dropped its items (DropSync.cpp): nothing here
    SPDLOG_INFO("[Coop] Replica {} (actor {:#x}) of room {} is gone", t.key, (uint16_t)t.actor->id, (int)t.room);
    Actor_Kill(t.actor);
}

// Once per frame, before actors update: the next frame of every room goes to its replicas.
void ApplyFrames() {
    int64_t now = NowMs();
    for (auto& [roomNum, room] : sRooms) {
        if (room.ready.empty() || (!room.listed && room.ready.size() < kBufferedFrames)) {
            continue;
        }
        RoomFrame frame = std::move(room.ready.front());
        room.ready.pop_front();
        bool first = !room.listed;
        room.listed = true;
        for (const ActorRecord& a : frame.actors) {
            TrackedActor* t = ActorRegistry_Find(roomNum, a.key);
            if (t != nullptr && t->actor->id == a.actorId) {
                t->record = a;
                t->hasRecord = true;
                t->lastSeenMs = now;
            }
        }
        for (uint16_t key : frame.gone) {
            if (TrackedActor* t = ActorRegistry_Find(roomNum, key); t != nullptr && t->actor->update != nullptr) {
                KillReplica(*t);
            }
        }
        if (first) {
            SPDLOG_INFO("[Coop] Replicas of room {}: {} enemies from player {}", (int)roomNum, frame.actors.size(),
                        (int)frame.owner);
            for (TrackedActor* t : ActorRegistry_All()) {
                if (t->room == roomNum && t->actor->update != nullptr && t->lastSeenMs != now) {
                    KillReplica(*t); // the owner has no such enemy: already killed there
                }
            }
        }
    }
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->actor->update != nullptr && Authority_IsRemote(t->room) && t->hasRecord &&
            now - t->lastSeenMs > kGoneAfterMs) {
            KillReplica(*t);
        }
    }
}

// Where a collider sits, as sent: cylinders their base, spheres their center (the first one of a JntSph). The
// spheres of a JntSph that follow limbs are placed again by the enemy's own draw.
Vec3s* DimPos(Collider* col) {
    switch (col->shape) {
        case COLSHAPE_CYLINDER:
            return &((ColliderCylinder*)col)->dim.pos;
        case COLSHAPE_SPHERE:
            return &((ColliderSphere*)col)->dim.worldSphere.center;
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jnt = (ColliderJntSph*)col;
            return (jnt->count > 0 && jnt->elements != nullptr) ? &jnt->elements[0].dim.worldSphere.center : nullptr;
        }
        default:
            return nullptr;
    }
}

void ApplyRecord(TrackedActor& t) {
    Actor* actor = t.actor;
    const ActorRecord& r = t.record;
    actor->prevPos = actor->world.pos;
    actor->world.pos = { r.pos[0], r.pos[1], r.pos[2] };
    actor->focus.pos = { r.focus[0], r.focus[1], r.focus[2] };
    actor->shape.rot = { r.rot.x, r.rot.y, r.rot.z };
    actor->world.rot.y = r.worldRotY;
    actor->scale = { r.scale[0], r.scale[1], r.scale[2] };
    actor->colChkInfo.health = r.health;
    actor->colorFilterParams = r.colorFilterParams;
    actor->colorFilterTimer = r.colorFilterTimer;
    actor->shape.shadowAlpha = r.shadowAlpha;
    actor->shape.yOffset = r.yOffset;
    actor->shape.shadowScale = r.shadowScale;
    // Its own logic would switch its body on and off (a Leever coming out of the ground): follow the owner.
    actor->draw = r.visible ? t.drawFunc : nullptr;
    actor->sfxId = r.loopSfx;
    actor->audioFlags = r.loopSfxFlags;
    if (t.skel != nullptr && t.skel->jointTable != nullptr) {
        int n = std::min<int>((int)r.joints.size(), t.skel->limbCount);
        for (int i = 0; i < n; i++) {
            t.skel->jointTable[i] = { r.joints[i].x, r.joints[i].y, r.joints[i].z };
        }
    }
    for (uint16_t sfx : r.sfx) {
        Actor_PlaySfx(actor, sfx);
    }
    if (t.def->readExtras != nullptr) {
        t.def->readExtras(actor, r.extras);
    }
    for (size_t i = 0; i < t.colliders.size() && i < r.colliders.size(); i++) {
        Collider* col = t.colliders[i];
        if (Vec3s* pos = DimPos(col)) {
            *pos = { r.colliders[i].dimPos.x, r.colliders[i].dimPos.y, r.colliders[i].dimPos.z };
        }
        if (r.colliders[i].flags & ActorCollider::kAc) {
            CollisionCheck_SetAC(gPlayState, &gPlayState->colChkCtx, col);
        }
        if (r.colliders[i].flags & ActorCollider::kOc) {
            CollisionCheck_SetOC(gPlayState, &gPlayState->colChkCtx, col);
        }
    }
    Actor_UpdateBgCheckInfo(gPlayState, actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4); // floor for the shadow
}

bool RoomListed(int8_t room) {
    auto it = sRooms.find(room);
    return it != sRooms.end() && it->second.listed;
}

// Replicas never run their own logic: they show the owner's state. Every shared enemy (ours too) first gets the
// hits that happened since the last frame (HitSync.cpp).
void OnShouldActorUpdate(Actor* actor, bool* should) {
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t == nullptr || !Active()) {
        return;
    }
    if (Authority_IsMine(t->room)) {
        HitSync_InjectPending(*t);
        return;
    }
    if (!Authority_IsRemote(t->room)) {
        return;
    }
    *should = false;
    HitSync_ReportReplicaHits(*t);
    if (RoomListed(t->room) && t->hasRecord) {
        ApplyRecord(*t);
    }
}


// ---- Owner ----

// The Links an enemy of ours may chase: our own and every drawn puppet of our scene.
std::vector<Player*> Links() {
    std::vector<Player*> out;
    out.push_back((Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first);
    for (const auto& [id, remote] : Session_Players()) {
        Actor* puppet = PuppetManager_Actor(id);
        if (puppet != nullptr && puppet->update != nullptr && puppet->draw != nullptr) {
            out.push_back((Player*)puppet);
        }
    }
    return out;
}

Player* ChooseTarget(TrackedActor& t, bool* nearPuppet) {
    std::vector<Player*> links = Links();
    Player* best = links[0];
    float bestDist = Actor_WorldDistXYZToActor(t.actor, &best->actor);
    float currentDist = -1.f;
    *nearPuppet = false;
    for (Player* link : links) {
        float d = Actor_WorldDistXYZToActor(t.actor, &link->actor);
        if (link != links[0] && d < kKeepUpdatingDist) {
            *nearPuppet = true;
        }
        if (&link->actor == t.target) {
            currentDist = d;
        }
        if (d < bestDist) {
            best = link;
            bestDist = d;
        }
    }
    // Keep the last target unless another Link is clearly closer (no flickering between two players).
    if (currentDist >= 0.f && bestDist > currentDist * kSwitchTargetRatio) {
        return (Player*)t.target;
    }
    t.target = &best->actor;
    return best;
}

ActorRecord MakeRecord(TrackedActor& t) {
    Actor* actor = t.actor;
    ActorRecord r;
    r.key = t.key;
    r.actorId = (uint16_t)actor->id;
    r.params = actor->params;
    r.visible = actor->draw != nullptr;
    r.pos[0] = actor->world.pos.x;
    r.pos[1] = actor->world.pos.y;
    r.pos[2] = actor->world.pos.z;
    r.focus[0] = actor->focus.pos.x;
    r.focus[1] = actor->focus.pos.y;
    r.focus[2] = actor->focus.pos.z;
    r.rot = { actor->shape.rot.x, actor->shape.rot.y, actor->shape.rot.z };
    r.worldRotY = actor->world.rot.y;
    r.scale[0] = actor->scale.x;
    r.scale[1] = actor->scale.y;
    r.scale[2] = actor->scale.z;
    r.health = actor->colChkInfo.health;
    r.colorFilterParams = actor->colorFilterParams;
    r.colorFilterTimer = actor->colorFilterTimer;
    r.shadowAlpha = actor->shape.shadowAlpha;
    r.yOffset = actor->shape.yOffset;
    r.shadowScale = actor->shape.shadowScale;
    r.loopSfx = actor->sfxId;
    r.loopSfxFlags = (uint8_t)actor->audioFlags;
    if (t.skel != nullptr && t.skel->jointTable != nullptr) {
        int n = std::min<int>(t.skel->limbCount, actor_limits::kJoints);
        for (int i = 0; i < n; i++) {
            const Vec3s& j = t.skel->jointTable[i];
            r.joints.push_back({ j.x, j.y, j.z });
        }
    }
    for (Collider* col : t.colliders) {
        ActorCollider c;
        c.flags = ((col->acFlags & AC_ON) ? ActorCollider::kAc : 0) | ((col->ocFlags1 & OC1_ON) ? ActorCollider::kOc : 0);
        if (Vec3s* pos = DimPos(col)) {
            c.dimPos = { pos->x, pos->y, pos->z };
        }
        r.colliders.push_back(c);
    }
    r.sfx = t.oneShotSfx;
    t.oneShotSfx.clear();
    if (t.def->writeExtras != nullptr) {
        t.def->writeExtras(actor, r.extras);
    }
    return r;
}

} // namespace

void ActorSync_OnDestroyed(TrackedActor& t) {
    if (Active() && Authority_IsMine(t.room)) {
        sRooms[t.room].recentlyGone.push_back({ t.key, NowMs() });
    }
    if (sUpdating == &t) {
        sUpdating = nullptr;
        gCoopPlayerOverride = nullptr;
    }
}

void ActorSync_FrameEnd() {
    if (!Active() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    int64_t now = NowMs();
    std::map<int8_t, ActorPacket> frames;
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->actor->update != nullptr && Authority_IsMine(t->room)) {
            frames[t->room].actors.push_back(MakeRecord(*t));
        }
    }
    for (auto& [roomNum, room] : sRooms) {
        auto& gone = room.recentlyGone;
        gone.erase(std::remove_if(gone.begin(), gone.end(),
                                  [&](const auto& g) { return now - g.second > kGoneMemoryMs; }),
                   gone.end());
        if (!gone.empty() && Authority_IsMine(roomNum)) {
            for (const auto& g : gone) {
                frames[roomNum].gone.push_back(g.first);
            }
        }
    }
    for (auto& [roomNum, frame] : frames) {
        frame.scene = gPlayState->sceneId;
        frame.room = roomNum;
        frame.seq = ++sRooms[roomNum].sendSeq;
        for (std::vector<uint8_t>& packet : EncodeActorPackets(frame)) {
            NetClient::Get().SendStream(std::move(packet));
        }
    }
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" Player* Coop_ActorUpdateBegin(PlayState* play, Actor* actor, s32* forceUpdate) {
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t == nullptr || !Active() || !Authority_IsMine(t->room)) {
        return nullptr;
    }
    bool nearPuppet = false;
    Player* target = ChooseTarget(*t, &nearPuppet);
    *forceUpdate = nearPuppet;
    sUpdating = t;
    Player* local = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    gCoopPlayerOverride = target != local ? target : nullptr;
    return target;
}

extern "C" void Coop_ActorUpdateEnd(PlayState* play, Actor* actor) {
    if (sUpdating != nullptr && sUpdating->actor == actor) {
        sUpdating = nullptr;
    }
    gCoopPlayerOverride = nullptr;
}

extern "C" void Coop_OnActorSfx(Actor* actor, u16 sfxId) {
    if (sUpdating != nullptr && sUpdating->actor == actor && sUpdating->oneShotSfx.size() < coop::actor_limits::kSfx) {
        sUpdating->oneShotSfx.push_back(sfxId);
    }
}

TrackedActor* coop::client::ActorSync_Updating() {
    return sUpdating;
}

static void RegisterActorSync() {
    COND_HOOK(ShouldActorUpdate, true, OnShouldActorUpdate);
    COND_HOOK(OnGameStateMainStart, true, []() {
        if (Active()) {
            ApplyFrames();
        }
    });
    COND_HOOK(OnPlayDestroy, true, Forget);
}

COOP_ON_STREAM(actorStream, coop::kStreamActors, OnActors);
COOP_ON_LOST(actorLost, [](const std::string&) { Forget(); });
static RegisterShipInitFunc sActorSyncInit(RegisterActorSync);
