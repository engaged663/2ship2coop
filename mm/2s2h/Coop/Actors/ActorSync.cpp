// [COOP] Sub-project D3 (spec D §5). Every replicated actor is simulated by one game: the owner of its room, or the
// player it is lent to (Leases.h). That game sends its memory every frame (ActorImage.h: what changed in the last
// few frames, all of it once a second); the others write it into their copy, whose own logic never runs. The actors
// we simulate chase the nearest Link (ours or a puppet) and keep updating while another player is next to them.
#include "ActorSync.h"

#include "ActorMemory.h"
#include "Authority.h"
#include "CoopEngine.h"
#include "HitSync.h"
#include "Leases.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Puppet/PuppetActor.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/Sync/Sync.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <map>
#include <set>

extern "C" {
#include "functions.h"
#include "variables.h"
}

extern "C" Player* gCoopPlayerOverride = nullptr;

namespace coop::client {

namespace {

using Clock = std::chrono::steady_clock;

constexpr float kKeepUpdatingDist = 1200.f; // an actor this close to a puppet updates even off our camera
constexpr float kFarDist = 2000.f;          // farther than this from every Link: sent 5 times a second
constexpr float kSwitchTargetRatio = 0.8f;  // another Link must be 20 % closer to become the target
constexpr uint32_t kResendFrames = 3;       // a changed slot goes out this many frames (a lost packet costs nothing)
constexpr uint32_t kFullEvery = 20;         // all of its memory once a second
constexpr uint32_t kAliveEvery = 10;        // the list of what we simulate twice a second
constexpr int64_t kGoneMemoryMs = 1000;     // an actor we destroyed is listed as gone for this long
constexpr size_t kBufferedFrames = 2;       // copies play a frame once two are queued (absorbs jitter)
constexpr size_t kMaxQueued = 8;            // far behind: skip ahead
constexpr size_t kMaxPendingSpans = 512;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// One frame of one sender for one room (all its parts together).
struct Frame {
    uint16_t seq = 0;
    bool hasAlive = false;
    std::vector<uint32_t> alive;
    std::vector<uint32_t> gone;
    std::vector<ActorImageRecord> records;
};

struct Stream {
    std::deque<Frame> frames;
    bool started = false;  // a frame was applied since we entered: copies may move
    bool listed = false;   // an alive list of this sender was applied
};

std::map<std::pair<int8_t, uint8_t>, Stream> sStreams; // (room, sender)
std::map<int8_t, std::vector<std::pair<uint32_t, int64_t>>> sRecentlyGone; // room -> keys we destroyed, when
std::map<int8_t, uint16_t> sSendSeq;
std::map<uint32_t, int64_t> sCreateTried; // runtime keys: when we last tried to create their copy
TrackedActor* sUpdating = nullptr;        // the actor of ours whose update is running
Actor* sAnyUpdating = nullptr;            // any actor whose update is running (PropSync.cpp)
uint32_t sFrame = 0;

// (The ending does not stop this: the copies of the lair stay frozen until its scene is gone. The ending's scenes
// have nothing replicated: ActorRegistry.cpp tracks nothing while it plays.)
bool Active() {
    return WorldSession_Active() && gPlayState != nullptr && Authority_Known();
}

void Forget() {
    sAnyUpdating = nullptr;
    sStreams.clear();
    sRecentlyGone.clear();
    sCreateTried.clear();
    sUpdating = nullptr;
    gCoopPlayerOverride = nullptr;
}

// ---- Receiving (copies) ----

void OnActors(const uint8_t* data, size_t size) {
    ActorImagePacket p;
    if (!Active() || EndingMode_Active() || !DecodeActorImage(data, size, p) || p.scene != gPlayState->sceneId ||
        p.playerId == 0 || p.playerId == Session_LocalId()) {
        return;
    }
    if (p.room == image_limits::kPlayerRoom && !Sync_On(SyncPart::PlayerObjects)) {
        return; // the players' own objects are off here (Sync/PlayerObjects.cpp)
    }
    Stream& st = sStreams[{ p.room, p.playerId }];
    if (st.frames.empty() || st.frames.back().seq != p.seq) {
        Frame f;
        f.seq = p.seq;
        st.frames.push_back(std::move(f));
    }
    Frame& f = st.frames.back();
    if (p.hasAlive) {
        f.hasAlive = true;
        f.alive = std::move(p.alive);
    }
    f.gone.insert(f.gone.end(), p.gone.begin(), p.gone.end());
    for (ActorImageRecord& r : p.records) {
        f.records.push_back(std::move(r));
    }
    while (st.frames.size() > kMaxQueued) {
        st.frames.pop_front();
    }
}

void KillCopy(TrackedActor& t) {
    t.actor->dropFlag = 0; // its simulator already dropped its items (DropSync.cpp)
    Actor_Kill(t.actor);
}

constexpr u16 kDynaFlags =
    BGACTOR_COLLISION_DISABLED | BGACTOR_CEILING_COLLISION_DISABLED | BGACTOR_FLOOR_COLLISION_DISABLED;

// Its dynamic collision is still its own here (the index is checked: a destroyed mesh frees it).
u16* DynaFlagsOf(const TrackedActor& t) {
    if (t.bgId < 0 || gPlayState == nullptr ||
        DynaPoly_GetActor(&gPlayState->colCtx, t.bgId) != (DynaPolyActor*)t.actor) {
        return nullptr;
    }
    return &gPlayState->colCtx.dyna.bgActorFlags[t.bgId];
}

// Another game created this actor at run time: we create our copy, once a second at most until it works.
TrackedActor* CreateCopy(const ActorImageRecord& r, int8_t room, uint8_t sender) {
    int64_t now = NowMs();
    auto tried = sCreateTried.find(r.key);
    if (tried != sCreateTried.end() && now - tried->second < 1000) {
        return nullptr;
    }
    sCreateTried[r.key] = now;
    const SpawnInfo& s = r.spawn;
    TrackedActor* parent = s.parentKey != 0 ? ActorRegistry_Find(s.parentKey) : nullptr;
    uint32_t root = parent != nullptr ? parent->rootKey : r.key;
    ActorRegistry_ExpectReplica(r.key, s.parentKey, root, room, s, room == image_limits::kPlayerRoom ? sender : 0);
    Actor* a = parent != nullptr
                   ? Actor_SpawnAsChild(&gPlayState->actorCtx, parent->actor, gPlayState, (s16)s.actorId, s.pos[0],
                                        s.pos[1], s.pos[2], s.rot[0], s.rot[1], s.rot[2], s.params)
                   : Actor_Spawn(&gPlayState->actorCtx, gPlayState, (s16)s.actorId, s.pos[0], s.pos[1], s.pos[2],
                                 s.rot[0], s.rot[1], s.rot[2], s.params);
    ActorRegistry_EndExpect();
    return a != nullptr ? ActorRegistry_Get(a) : nullptr;
}

// A copy is created only from the frames of the game that simulates it: what a cutscene made at run time, for who
// watches that cutscene; the rest, from the lessee of its family or the owner of its room (a game showing a cutscene
// may send frames of a room that is not its own: ActorHandlers.cpp).
bool MayCreate(const ActorImageRecord& r, int8_t room, uint8_t sender) {
    if (room == image_limits::kPlayerRoom) {
        return PlayerObjects_MayCreate(r.key, r.spawn.parentKey, sender); // the sender's own object (or its child)
    }
    if (ActorRegistry_IsCinemaSpawn(r.spawn.parentKey, r.spawn)) {
        return Rules_CinemaActorsOn() && Cinema_WatchingFrom(sender);
    }
    TrackedActor* parent = r.spawn.parentKey != 0 ? ActorRegistry_Find(r.spawn.parentKey) : nullptr;
    uint8_t lessee = Leases_Holder(room, parent != nullptr ? parent->rootKey : r.key);
    return (lessee != 0 ? lessee : Authority_Owner(room)) == sender;
}

// One frame of `sender` for `room`: its records go to the copies it simulates (queued for their next update).
void DispatchFrame(int8_t room, uint8_t sender, Frame& f, Stream& st) {
    int64_t now = NowMs();
    std::set<uint32_t> seen;
    for (ActorImageRecord& r : f.records) {
        TrackedActor* t = ActorRegistry_Find(r.key);
        if (t == nullptr && !r.continuation && r.hasSpawn && (r.key & kRuntimeKeyBit) != 0 &&
            MayCreate(r, room, sender)) {
            t = CreateCopy(r, room, sender);
        }
        if (t == nullptr && room == image_limits::kPlayerRoom && !r.continuation && IsListKey(r.key)) {
            t = PlayerObjects_AdoptFor(r.key, r.actorId, sender); // a prop of the room it carries
        }
        if (t == nullptr || t->actor->id != (s16)r.actorId || t->room != room || Leases_Owner(*t) != sender) {
            continue; // unknown here, another actor with that key, or not (or no longer) simulated by the sender
        }
        seen.insert(r.key);
        for (SlotSpan& s : r.spans) {
            t->pendingSpans.push_back(std::move(s));
        }
        while (t->pendingSpans.size() > kMaxPendingSpans) {
            t->pendingSpans.erase(t->pendingSpans.begin());
        }
        if (!r.continuation) {
            t->remoteAc = r.acMask;
            t->remoteOc = r.ocMask;
            t->pendingSounds.insert(t->pendingSounds.end(), r.sfx.begin(), r.sfx.end());
            if (r.dyna >= 0) {
                t->pendingDyna = r.dyna; // its collision switched on or off there (spec §6.3)
            }
        }
        t->hasState = true;
        t->lastSeenMs = now;
    }
    for (uint32_t key : f.gone) {
        TrackedActor* t = ActorRegistry_Find(key);
        if (t != nullptr && t->room == room && t->actor->update != nullptr && Leases_Owner(*t) == sender) {
            KillCopy(*t);
        }
        sCreateTried[key] = now + 60000; // never created again from a late packet
    }
    if (!f.hasAlive) {
        return;
    }
    std::set<uint32_t> alive(f.alive.begin(), f.alive.end());
    bool first = !st.listed;
    st.listed = true;
    int named = 0;
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->room != room || t->actor->update == nullptr || Leases_Owner(*t) != sender) {
            continue;
        }
        if (alive.count(t->key) != 0) {
            t->missedAlive = 0;
            named++;
        } else {
            if (t->adopted) {
                if (++t->missedAlive >= 2) {
                    ActorRegistry_ReleaseAdopted(t->actor); // put down: ours again, where it was left
                }
                continue;
            }
            // Its simulator has no such actor (already killed there). NPCs of the room's list (the Moon's children)
            // are removed only after several lists in a row: a lease changing hands must never delete them.
            if (t->cinema && !t->runtime) {
                continue; // our own cutscene actor, only driven while we watch: a list never removes it
            }
            // The props of a minigame wait like the NPCs: its director takes them a moment after arriving (the rings
            // of a race only exist in the games that are in it, not in the room's owner's).
            bool npc = !t->runtime && (t->actor->category == ACTORCAT_NPC || Activity_IsPropId(t->actor->id));
            // What a cutscene made at run time waits longer: its lists stop a moment before we stop watching
            bool patient = npc || t->cinema;
            if ((first && !patient) || ++t->missedAlive >= (npc ? 8 : (t->cinema ? 4 : 2))) {
                KillCopy(*t);
            }
        }
    }
    if (first) {
        SPDLOG_INFO("[Coop] Replicas of room {}: {} actors from player {}", (int)room, named, (int)sender);
    }
}

// Once per frame, before actors update: the next frame of every sender goes to its copies.
void ApplyFrames() {
    for (auto& [id, st] : sStreams) {
        if (st.frames.empty() || (!st.started && st.frames.size() < kBufferedFrames)) {
            continue;
        }
        st.started = true;
        // Behind (a burst arrived together): apply the extra ones now to catch up.
        size_t n = st.frames.size() > kBufferedFrames + 2 ? st.frames.size() - kBufferedFrames : 1;
        for (size_t i = 0; i < n && !st.frames.empty(); i++) {
            Frame f = std::move(st.frames.front());
            st.frames.pop_front();
            DispatchFrame(id.first, id.second, f, st);
        }
    }
}

// A copy's turn to update: write what arrived, register the same colliders its simulator did, play its sounds.
void ApplyPending(TrackedActor& t) {
    for (const SlotSpan& s : t.pendingSpans) {
        ActorMemory_Apply(t, s);
    }
    t.pendingSpans.clear();
    HitSync_ClearMarks(t);
    for (size_t i = 0; i < t.colliders.size() && t.owner == 0; i++) { // a player's object is only seen here
        if (t.remoteAc & (1u << i)) {
            CollisionCheck_SetAC(gPlayState, &gPlayState->colChkCtx, t.colliders[i]);
        }
        if (t.remoteOc & (1u << i)) {
            CollisionCheck_SetOC(gPlayState, &gPlayState->colChkCtx, t.colliders[i]);
        }
    }
    for (const SoundEntry& s : t.pendingSounds) {
        SoundEcho_PlayOn(t.actor, s); // what its simulator heard it make (Sync/SoundEcho.cpp)
    }
    t.pendingSounds.clear();
    Actor_UpdateBgCheckInfo(gPlayState, t.actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4); // floor for the shadow
    if (t.pendingDyna >= 0) {
        if (u16* f = DynaFlagsOf(t)) {
            *f = (u16)((*f & ~kDynaFlags) | ((u16)t.pendingDyna & kDynaFlags)); // its owner's collision on/off
        }
        t.pendingDyna = -1;
    }
}

// Copies never run their own logic. Every replicated actor (ours too) first gets the hits of the last frame.
void OnShouldActorUpdate(Actor* actor, bool* should) {
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t == nullptr || !Active()) {
        return;
    }
    if (Leases_IsMine(*t)) {
        HitSync_InjectPending(*t);
        return;
    }
    if (!Leases_IsRemote(*t)) {
        return; // nobody known simulates it: ours for now, sent to nobody
    }
    *should = false;
    if (t->owner == 0) {
        HitSync_ReportReplicaHits(*t); // reads the hit flags before its new state overwrites them
    }
    if (t->hasState && t->actor->update != nullptr) {
        ApplyPending(*t);
    }
}

// ---- Simulating (ours) ----

Player* LocalLink() {
    return (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

// Every Link an actor of ours may go for. Ours first, except on the server's host: its Link is a ghost, so only
// the puppets count there (it stays alone in the list when no puppet is around).
std::vector<Player*> Links() {
    std::vector<Player*> out;
    out.push_back(LocalLink());
    for (const auto& [id, remote] : Session_Players()) {
        Actor* puppet = PuppetManager_Actor(id);
        if (puppet != nullptr && puppet->update != nullptr && puppet->draw != nullptr) {
            out.push_back((Player*)puppet);
        }
    }
    if (HostMode_Enabled() && out.size() > 1) {
        out.erase(out.begin());
    }
    return out;
}

Player* ChooseTarget(TrackedActor& t, bool* nearPuppet) {
    std::vector<Player*> links = Links();
    Player* local = LocalLink();
    Player* best = links[0];
    float bestDist = Actor_WorldDistXYZToActor(t.actor, &best->actor);
    float currentDist = -1.f;
    *nearPuppet = false;
    for (Player* link : links) {
        float d = Actor_WorldDistXYZToActor(t.actor, &link->actor);
        if (link != local && d < kKeepUpdatingDist) {
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

// A lent NPC's family (talking, shops, minigames) and the minigame we run deal with our Link only: never with the
// puppet of whoever stands closer (it cannot talk, pay or play).
bool OnOurLinkOnly(const TrackedActor& t) {
    const TrackedActor* root = t.rootKey == t.key ? &t : ActorRegistry_Find(t.rootKey);
    if (root == nullptr || root->actor == nullptr) {
        return false;
    }
    return (root->actor->category == ACTORCAT_NPC && Leases_LentToMe(*root)) || Director_PinsFamily(root->actor) ||
           (SceneObjects_Policy(root->actor->id) != TouchPolicy::None && Leases_LentToMe(*root)); // what we touch
}

// Farther than kFarDist from every Link (ours and the puppets): nobody sees it up close.
bool FarFromEveryone(const TrackedActor& t) {
    for (Player* link : Links()) {
        if (Actor_WorldDistXYZToActor(t.actor, &link->actor) < kFarDist) {
            return false;
        }
    }
    return true;
}

// What goes out for one of our actors this frame (false: nothing). Slots are classified every frame (a change is
// remembered even when it is not sent); a changed slot goes out kResendFrames frames, all of them in a full frame.
bool MakeRecord(TrackedActor& t, ActorImageRecord& r) {
    bool full = (sFrame + t.fullPhase) % kFullEvery == 0;
    bool send = full || !FarFromEveryone(t) || sFrame % 4 == 0;
    r.key = t.key;
    r.actorId = (uint16_t)t.actor->id;
    r.acMask = t.acMask;
    r.ocMask = t.ocMask;
    r.sfx.swap(t.sounds);
    t.sounds.clear(); // sent now (or dropped: nobody is near): never sent twice
    constexpr uint8_t kSpawnFrames = 3; // a new one's first records always say how to create it
    r.hasSpawn = t.runtime && (full || t.spawnSent < kSpawnFrames);
    r.spawn = t.spawn;
    // A flagged sound (sfxId/audioFlags) is cleared by the engine before every update: while it sounds its slots go
    // every frame, so the copy has it again before drawing (Actor_UpdateFlaggedAudio plays it there too)
    bool flagged = t.actor->sfxId != 0 || t.actor->audioFlags != 0;
    constexpr size_t kSfxSlot = offsetof(Actor, sfxId) / 8;
    constexpr size_t kAudioFlagsSlot = offsetof(Actor, audioFlags) / 8;
    for (size_t rg = 0; rg < t.regions.size(); rg++) {
        size_t slots = (t.regions[rg].size + 7) / 8;
        auto& sent = t.sentSlots[rg];
        auto& changed = t.changedFrame[rg];
        if (sent.size() != slots) {
            sent.assign(slots, Slot{ SlotKind::Raw, 0xFFFFFFFFFFFFFFFFull }); // never matches: all "changed"
            changed.assign(slots, 0);
        }
        SlotSpan span;
        for (size_t i = 0; i <= slots; i++) {
            bool include = false;
            Slot now;
            if (i < slots && !ActorMemory_IsLocalSlot(t, rg, i)) {
                now = ActorMemory_Capture(t, rg, i);
                if (now != sent[i]) {
                    sent[i] = now;
                    changed[i] = sFrame;
                }
                include = full || sFrame - changed[i] < kResendFrames ||
                          (rg == 0 && flagged && (i == kSfxSlot || i == kAudioFlagsSlot));
            }
            bool contiguous = !span.slots.empty() && span.first + span.slots.size() == i &&
                              span.slots.size() < (size_t)image_limits::kSpanSlots;
            if (include && (span.slots.empty() || contiguous)) {
                if (span.slots.empty()) {
                    span.region = (uint8_t)rg;
                    span.first = (uint16_t)i;
                }
                span.slots.push_back(now);
                continue;
            }
            if (!span.slots.empty()) {
                r.spans.push_back(std::move(span));
                span = SlotSpan{};
            }
            if (include) {
                span.region = (uint8_t)rg;
                span.first = (uint16_t)i;
                span.slots.push_back(now);
            }
        }
    }
    if (u16* f = DynaFlagsOf(t)) {
        int16_t d = (int16_t)(*f & kDynaFlags);
        if (full || d != t.sentDyna) {
            r.dyna = d; // its collision on/off (a platform that vanishes)
        }
    }
    bool masksChanged = t.acMask != t.sentAc || t.ocMask != t.sentOc;
    if (!send || (r.spans.empty() && r.sfx.empty() && !masksChanged && !r.hasSpawn && r.dyna < 0)) {
        return false;
    }
    t.sentAc = t.acMask;
    t.sentOc = t.ocMask;
    if (r.dyna >= 0) {
        t.sentDyna = r.dyna;
    }
    if (r.hasSpawn && t.spawnSent < 255) {
        t.spawnSent++;
    }
    return true;
}

} // namespace

TrackedActor* ActorSync_Updating() {
    return sUpdating;
}

Actor* ActorSync_AnyUpdating() {
    return sAnyUpdating;
}

void ActorSync_OnDestroyed(TrackedActor& t) {
    if (Active() && Leases_IsMine(t)) {
        sRecentlyGone[t.room].push_back({ t.key, NowMs() });
    }
    if (sUpdating == &t) {
        sUpdating = nullptr;
        gCoopPlayerOverride = nullptr;
    }
}

void ActorSync_ForgetPointersTo(const Actor* actor) {
    if (!Active() || actor->overlayEntry == nullptr || actor->overlayEntry->profile == nullptr) {
        return;
    }
    size_t size = actor->overlayEntry->profile->instanceSize;
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->actor != actor && Leases_IsRemote(*t)) {
            ActorMemory_ForgetPointersTo(*t, actor, size); // copies only: ours handle their own pointers
        }
    }
}

void ActorSync_FrameEnd() {
    if (!Active() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    sFrame++;
    Leases_Tick();
    ActorMemory_RebuildResolver();
    int64_t now = NowMs();
    bool alive = sFrame % kAliveEvery == 0;
    std::map<int8_t, ActorImagePacket> frames;
    bool showing = Cinema_DirectingShared(); // our cutscene actors travel only while others watch our cutscene
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->actor->update == nullptr || !Leases_IsMine(*t) || (t->cinema && !showing)) {
            continue;
        }
        ActorImagePacket& f = frames[t->room];
        if (alive) {
            f.hasAlive = true;
            f.alive.push_back(t->key);
        }
        ActorImageRecord r;
        if (MakeRecord(*t, r)) {
            f.records.push_back(std::move(r));
        }
    }
    // Keys we destroyed in the last second travel with the frame of their room (the games remove their copies).
    for (auto& [room, gone] : sRecentlyGone) {
        gone.erase(std::remove_if(gone.begin(), gone.end(),
                                  [&](const auto& g) { return now - g.second > kGoneMemoryMs; }),
                   gone.end());
        if (gone.empty() || !(room == image_limits::kPlayerRoom || Authority_IsMine(room)) || !frames.count(room)) {
            continue; // only with a frame: the alive list says the rest
        }
        ActorImagePacket& f = frames[room];
        for (const auto& g : gone) {
            if (f.gone.size() < (size_t)image_limits::kKeys) {
                f.gone.push_back(g.first);
            }
        }
    }
    for (auto& [room, frame] : frames) {
        frame.scene = gPlayState->sceneId;
        frame.room = room;
        frame.seq = ++sSendSeq[room];
        for (std::vector<uint8_t>& packet : EncodeActorImage(frame)) {
            NetClient::Get().SendStream(std::move(packet));
        }
    }
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" Player* Coop_ActorUpdateBegin(PlayState* play, Actor* actor, s32* forceUpdate) {
    sAnyUpdating = actor;
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t == nullptr || !Active()) {
        return nullptr;
    }
    if (Leases_IsRemote(*t)) {
        // A copy follows its simulator every frame, on screen or not: its "update" only writes the received state.
        *forceUpdate = true;
        return nullptr;
    }
    if (!Leases_IsMine(*t)) {
        return nullptr; // nobody known simulates it yet: it stays local and quiet
    }
    if (t->owner != 0) {
        sUpdating = t; // our own object: it plays with our Link, as always
        return nullptr;
    }
    if (t->cinema) {
        // A cutscene actor of ours plays with our own Link; what it creates travels only while others watch
        if (Cinema_DirectingShared()) {
            sUpdating = t;
        }
        return nullptr;
    }
    bool nearPuppet = false;
    Player* target = ChooseTarget(*t, &nearPuppet);
    *forceUpdate = nearPuppet;
    sUpdating = t;
    Player* local = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (OnOurLinkOnly(*t)) {
        target = local;
    }
    gCoopPlayerOverride = target != local ? target : nullptr;
    return target;
}

extern "C" void Coop_ActorUpdateEnd(PlayState* play, Actor* actor) {
    sAnyUpdating = nullptr;
    if (sUpdating != nullptr && sUpdating->actor == actor) {
        sUpdating = nullptr;
    }
    gCoopPlayerOverride = nullptr;
}

static void RegisterActorSync() {
    COND_HOOK(ShouldActorUpdate, true, OnShouldActorUpdate);
    COND_HOOK(OnGameStateMainStart, true, []() {
        for (TrackedActor* t : ActorRegistry_All()) {
            t->sounds.clear(); // a record that was not sent takes nothing late
        }
        if (Active()) {
            ApplyFrames();
        }
    });
    COND_HOOK(OnPlayDestroy, true, Forget);
}

COOP_ON_STREAM(actorStream, coop::kStreamActors, OnActors);
COOP_ON_LOST(actorLost, [](const std::string&) { Forget(); });
static RegisterShipInitFunc sActorSyncInit(RegisterActorSync);
