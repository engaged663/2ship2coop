// [COOP] See ActorMemory.h.
#include "ActorMemory.h"

#include "ProcessMemory.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

// Bytes of the base Actor that belong to each game's engine (1) and never travel. `flags` is merged bit by bit.
std::array<uint8_t, sizeof(Actor)> BuildLocalMask() {
    std::array<uint8_t, sizeof(Actor)> m{};
    auto mark = [&](size_t off, size_t size) { std::fill(m.begin() + off, m.begin() + off + size, (uint8_t)1); };
#define LOCAL(field) mark(offsetof(Actor, field), sizeof(((Actor*)nullptr)->field))
    LOCAL(id);
    LOCAL(category);
    LOCAL(room);
    LOCAL(objectSlot);
    LOCAL(wallPoly);
    LOCAL(floorPoly);
    LOCAL(yawTowardsPlayer);
    LOCAL(xyzDistToPlayerSq);
    LOCAL(xzDistToPlayer);
    LOCAL(playerHeightRel);
    LOCAL(projectedPos);
    LOCAL(projectedW);
    LOCAL(isLockedOn);
    LOCAL(targetPriority);
    LOCAL(freezeTimer);
    LOCAL(isDrawn);
    LOCAL(dropFlag);
    LOCAL(prev);
    LOCAL(next);
    LOCAL(init);
    LOCAL(destroy);
    LOCAL(overlayEntry);
#undef LOCAL
    return m;
}

const std::array<uint8_t, sizeof(Actor)> kLocalMask = BuildLocalMask();
constexpr size_t kFlagsOff = offsetof(Actor, flags);
constexpr size_t kUpdateOff = offsetof(Actor, update);
// Flags each game's engine sets for itself (culling, what its own Link attached to it).
constexpr uint32_t kLocalFlags =
    ACTOR_FLAG_INSIDE_CULLING_VOLUME | ACTOR_FLAG_HOOKSHOT_ATTACHED | ACTOR_FLAG_ATTACHED_TO_ARROW;

// 0: shared, 1: some bytes local, 2: all local (region 0 only).
uint8_t SlotLocality(const TrackedActor& t, size_t slot) {
    const std::vector<uint8_t>& m = t.localMask;
    size_t start = slot * 8;
    if (start >= m.size()) {
        return 0;
    }
    size_t end = std::min(start + 8, m.size());
    size_t n = 0;
    for (size_t b = start; b < end; b++) {
        n += m[b];
    }
    return n == 0 ? 0 : (n == end - start ? 2 : 1);
}

struct Range {
    uintptr_t start;
    uintptr_t end;
    uint32_t key;    // Actor: its key; Link: player id
    uint8_t region;
};

// The scene's path list: a run of 8-byte Path entries starting at setupPathList. The same scene in every game of
// the world lays it out the same, so a Path* travels as its index in that run.
struct ScenePaths {
    const Path* base = nullptr;
    size_t count = 0;

    void Rebuild() {
        base = nullptr;
        count = 0;
        if (gPlayState == nullptr || gPlayState->setupPathList == nullptr) {
            return;
        }
        // The list ends where an entry does not look like a Path (count beyond any path, a wild index).
        for (size_t i = 0; i < 512; i++) {
            const Path* p = gPlayState->setupPathList + i;
            if (p->count == 0 || p->count > 100 ||
                (p->additionalPathIndex != ADDITIONAL_PATH_INDEX_NONE && p->additionalPathIndex > 250)) {
                break;
            }
            count = i + 1;
        }
        base = gPlayState->setupPathList;
    }

    bool Index(const void* p, uint64_t& index) const {
        if (base == nullptr || p < base) {
            return false;
        }
        uintptr_t off = (uintptr_t)p - (uintptr_t)base;
        if (off >= count * sizeof(Path)) {
            return false;
        }
        index = off / sizeof(Path);
        return true;
    }

    const Path* At(uint64_t index) const {
        return base != nullptr && index < count ? base + index : nullptr;
    }
};

ScenePaths sPaths;

// What the sender's process knows about pointers, rebuilt every frame (ActorMemory_RebuildResolver).
class GameResolver : public PointerResolver {
  public:
    uint64_t exeBase = 0;
    uint64_t exeSize = 0;
    std::vector<Range> actors; // sorted by start
    std::vector<Range> links;

    bool ExeOffset(uint64_t a, uint64_t& off) const override {
        if (exeSize != 0 && a >= exeBase && a < exeBase + exeSize) {
            off = a - exeBase;
            return true;
        }
        return false;
    }
    bool ActorRef(uint64_t a, uint64_t& v) const override {
        auto it = std::upper_bound(actors.begin(), actors.end(), (uintptr_t)a,
                                   [](uintptr_t x, const Range& r) { return x < r.start; });
        if (it == actors.begin()) {
            return false;
        }
        --it;
        if (a >= it->end) {
            return false;
        }
        v = ActorRefValue(it->key, it->region, (uint16_t)(a - it->start));
        return true;
    }
    bool LinkRef(uint64_t a, uint64_t& v) const override {
        for (const Range& r : links) {
            if (a >= r.start && a < r.end) {
                v = LinkRefValue((uint8_t)r.key, (uint32_t)(a - r.start));
                return true;
            }
        }
        return false;
    }
    bool SceneOffset(uint64_t a, uint64_t& off) const override {
        return sPaths.Index((const void*)a, off);
    }
    bool IsMapped(uint64_t a) const override {
        return ProcessMemory_IsMapped(a);
    }
};

GameResolver sResolver;
uint32_t sResolverVersion = 0xFFFFFFFFu; // ActorRegistry_Version() when it was rebuilt
// gCoop.Sync.ListPointers (read with the resolver): pointers into the room's list actors nobody replicates travel.
// Off: as before (the copy keeps its own value from its Init).
bool sListPointers = true;

Player* LocalLink() {
    return gPlayState != nullptr ? (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
}

uint32_t InstanceBytes(const Actor* a) {
    uint32_t size = (a->overlayEntry != nullptr && a->overlayEntry->profile != nullptr)
                        ? a->overlayEntry->profile->instanceSize
                        : 0;
    return std::min<uint32_t>(size, image_limits::kRegionBytes);
}

// An actor of the room's list that nobody replicates here (each game has its own: the balloon Bomber Jim shoots at):
// its LocalListKey, 0 if it is not one. Never a Link: ours and the puppets travel as Link slots.
uint32_t LocalListKeyOf(const Actor* a) {
    int16_t index = GetActorListIndex(a);
    if (a->update == nullptr || a->category == ACTORCAT_PLAYER || index < 0 || index > 0xFF || a->room < 0 ||
        a->room > image_limits::kRoomMax || a->id < 0 || InstanceBytes(a) == 0 || ActorRegistry_Get(a) != nullptr) {
        return 0;
    }
    return LocalListKey((uint16_t)a->id, (uint8_t)a->room, (uint8_t)index);
}

// Our own actor of the list entry a LocalListKey names (nullptr: not here, or another actor there).
Actor* FindLocalListActor(uint32_t key) {
    uint16_t id = 0;
    uint8_t room = 0;
    uint8_t index = 0;
    if (gPlayState == nullptr || !ParseLocalListKey(key, id, room, index)) {
        return nullptr;
    }
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->update != nullptr && a->id == (s16)id && a->room == (s8)room && GetActorListIndex(a) == index) {
                return a;
            }
        }
    }
    return nullptr;
}

// Where a received pointer points in this game; nullptr: nowhere here (the slot keeps its own value).
uint8_t* Resolve(const Slot& s) {
    switch (s.kind) {
        case SlotKind::Exe:
            return (sResolver.exeSize != 0 && s.value < sResolver.exeSize) ? (uint8_t*)(sResolver.exeBase + s.value)
                                                                           : nullptr;
        case SlotKind::Actor: {
            uint32_t key = (uint32_t)(s.value >> 32);
            size_t rg = (s.value >> 16) & 0xFF;
            size_t off = s.value & 0xFFFF;
            if (TrackedActor* x = ActorRegistry_Find(key)) {
                return (rg < x->regions.size() && off < x->regions[rg].size) ? x->regions[rg].ptr + off : nullptr;
            }
            Actor* own = (rg == 0 && sListPointers) ? FindLocalListActor(key) : nullptr; // a list actor: ours
            return (own != nullptr && off < InstanceBytes(own)) ? (uint8_t*)own + off : nullptr;
        }
        case SlotKind::Link: {
            uint8_t pid = (uint8_t)(s.value >> 32);
            size_t off = (uint32_t)s.value;
            if (off >= sizeof(Player)) {
                return nullptr;
            }
            Actor* who = pid == Session_LocalId() ? nullptr : PuppetManager_Actor(pid);
            if (who == nullptr) {
                who = (Actor*)LocalLink(); // the host's own Link, or a player not here: ours
            }
            return who != nullptr ? (uint8_t*)who + off : nullptr;
        }
        case SlotKind::Scene: {
            const Path* p = sPaths.At(s.value);
            return p != nullptr ? (uint8_t*)p : nullptr;
        }
        default:
            return nullptr;
    }
}

uint64_t ReadBytes(const uint8_t* p, size_t n) {
    uint64_t v = 0;
    std::memcpy(&v, p, n);
    return v;
}

size_t SlotBytes(const Region& r, size_t slot) {
    size_t off = slot * 8;
    return off >= r.size ? 0 : std::min<size_t>(8, r.size - off);
}

// Writes 8 received bytes into slot `slot` of region `region`, leaving the engine's own bytes alone.
void WriteSlot(TrackedActor& t, size_t region, size_t slot, uint64_t value) {
    Region& r = t.regions[region];
    size_t n = SlotBytes(r, slot);
    size_t off = slot * 8;
    if (n == 0) {
        return;
    }
    uint8_t bytes[8];
    std::memcpy(bytes, &value, 8);
    if (region != 0 || off >= t.localMask.size()) {
        std::memcpy(r.ptr + off, bytes, n);
        return;
    }
    if (off == kUpdateOff && value == 0) {
        return; // the owner killed it: the alive list removes it here (a null update would delete it at once)
    }
    for (size_t b = 0; b < n; b++) {
        size_t at = off + b;
        bool local = at < t.localMask.size() && t.localMask[at] != 0;
        if (!local && (at < kFlagsOff || at >= kFlagsOff + 4)) {
            r.ptr[at] = bytes[b];
        }
    }
    if (kFlagsOff >= off && kFlagsOff + 4 <= off + n) {
        uint32_t remote = 0;
        std::memcpy(&remote, bytes + (kFlagsOff - off), 4);
        t.actor->flags = (t.actor->flags & kLocalFlags) | (remote & ~kLocalFlags);
    }
}

} // namespace

void ActorMemory_RebuildResolver() {
    ProcessMemory_ExeRange(sResolver.exeBase, sResolver.exeSize);
    sPaths.Rebuild();
    sResolverVersion = ActorRegistry_Version();
    sResolver.actors.clear();
    for (TrackedActor* t : ActorRegistry_All()) {
        for (size_t i = 0; i < t->regions.size(); i++) {
            const Region& r = t->regions[i];
            sResolver.actors.push_back({ (uintptr_t)r.ptr, (uintptr_t)r.ptr + r.size, t->key, (uint8_t)i });
        }
    }
    // The room's list actors nobody replicates: a copy of one of ours that points at one gets its own game's (without
    // this it kept the null of its Init, and crashed when it became ours: Bomber Jim and his balloon)
    sListPointers = CVarGetInteger("gCoop.Sync.ListPointers", 1) != 0;
    for (int cat = 0; sListPointers && gPlayState != nullptr && cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (uint32_t key = LocalListKeyOf(a)) {
                sResolver.actors.push_back({ (uintptr_t)a, (uintptr_t)a + InstanceBytes(a), key, 0 });
            }
        }
    }
    std::sort(sResolver.actors.begin(), sResolver.actors.end(),
              [](const Range& a, const Range& b) { return a.start < b.start; });
    sResolver.links.clear();
    if (Player* link = LocalLink()) {
        sResolver.links.push_back({ (uintptr_t)link, (uintptr_t)link + sizeof(Player), Session_LocalId(), 0 });
    }
    for (const auto& [id, remote] : Session_Players()) {
        if (Actor* p = PuppetManager_Actor(id)) {
            sResolver.links.push_back({ (uintptr_t)p, (uintptr_t)p + sizeof(Player), id, 0 });
        }
    }
}

void ActorMemory_BuildLocalMask(TrackedActor& t) {
    const Region& r = t.regions[0];
    t.localMask.assign(r.size, 0);
    std::copy(kLocalMask.begin(), kLocalMask.begin() + std::min<size_t>(r.size, sizeof(Actor)), t.localMask.begin());
    if (r.size < sizeof(DynaPolyActor) || gPlayState == nullptr) {
        return;
    }
    s32 bgId = ((DynaPolyActor*)t.actor)->bgId;
    if (DynaPoly_GetActor(&gPlayState->colCtx, bgId) != (DynaPolyActor*)t.actor) {
        return; // not a DynaPolyActor (DynaPoly_GetActor checks the index is valid and in use)
    }
    t.bgId = bgId; // its collision flags travel (kRecDyna, ActorSync.cpp)
    auto mark = [&](size_t off, size_t size) {
        std::fill(t.localMask.begin() + off, t.localMask.begin() + off + size, (uint8_t)1);
    };
    mark(offsetof(DynaPolyActor, bgId), sizeof(s32));
    mark(offsetof(DynaPolyActor, interactFlags), sizeof(u8));
}

bool ActorMemory_IsLocalSlot(const TrackedActor& t, size_t region, size_t slot) {
    return region == 0 && SlotLocality(t, slot) == 2;
}

Slot ActorMemory_Capture(const TrackedActor& t, size_t region, size_t slot) {
    const Region& r = t.regions[region];
    size_t n = SlotBytes(r, slot);
    uint64_t raw = n == 0 ? 0 : ReadBytes(r.ptr + slot * 8, n);
    if (region == 0 && SlotLocality(t, slot) == 1) {
        // Shares its 8 bytes with engine fields: sent as plain data with those bytes cleared (never a pointer).
        uint8_t bytes[8];
        std::memcpy(bytes, &raw, 8);
        size_t off = slot * 8;
        for (size_t b = 0; b < 8 && off + b < t.localMask.size(); b++) {
            if (t.localMask[off + b] != 0) {
                bytes[b] = 0;
            }
        }
        if (kFlagsOff >= off && kFlagsOff + 4 <= off + 8) {
            uint32_t flags = 0;
            std::memcpy(&flags, bytes + (kFlagsOff - off), 4);
            flags &= ~kLocalFlags;
            std::memcpy(bytes + (kFlagsOff - off), &flags, 4);
        }
        std::memcpy(&raw, bytes, 8);
        return raw == 0 ? Slot{ SlotKind::Zero, 0 } : Slot{ SlotKind::Raw, raw };
    }
    if (slot < r.rawOnly.size() && r.rawOnly[slot]) {
        return raw == 0 ? Slot{ SlotKind::Zero, 0 } : Slot{ SlotKind::Raw, raw };
    }
    return ClassifySlot(raw, sResolver);
}

void ActorMemory_Apply(TrackedActor& t, const SlotSpan& span) {
    if (span.region >= t.regions.size()) {
        return;
    }
    Region& r = t.regions[span.region];
    for (size_t i = 0; i < span.slots.size(); i++) {
        size_t slot = span.first + i;
        size_t n = SlotBytes(r, slot);
        if (n == 0) {
            return; // past the end of our copy (should not happen with the same exe)
        }
        if (ActorMemory_IsLocalSlot(t, span.region, slot)) {
            continue;
        }
        const Slot& s = span.slots[i];
        uint64_t value = 0;
        switch (s.kind) {
            case SlotKind::Raw:
                value = s.value;
                break;
            case SlotKind::Zero:
                value = 0;
                break;
            case SlotKind::Keep:
                value = ReadBytes(r.init.data() + slot * 8, n);
                break;
            default: {
                uint8_t* p = Resolve(s);
                value = p != nullptr ? (uint64_t)(uintptr_t)p : ReadBytes(r.init.data() + slot * 8, n);
                break;
            }
        }
        WriteSlot(t, span.region, slot, value);
    }
}

// In the middle of a frame (an effect being made) the actors may have changed since the resolver was rebuilt (another
// scene, an actor created or gone): a stale range would name the wrong actor.
static void RefreshResolver() {
    if (sResolverVersion != ActorRegistry_Version()) {
        ActorMemory_RebuildResolver();
    }
}

Slot ActorMemory_Classify(uint64_t raw) {
    RefreshResolver();
    Slot s = ClassifySlot(raw, sResolver);
    uint16_t id = 0;
    uint8_t room = 0;
    uint8_t index = 0;
    if (s.kind == SlotKind::Actor && ParseLocalListKey((uint32_t)(s.value >> 32), id, room, index)) {
        return { SlotKind::Keep, 0 }; // an effect or a sound on an actor every game has itself stays in its game
    }
    return s;
}

uint8_t* ActorMemory_Resolve(const Slot& s) {
    RefreshResolver();
    return Resolve(s);
}

void ActorMemory_ForgetPointersTo(TrackedActor& copy, const void* start, size_t size) {
    uintptr_t lo = (uintptr_t)start;
    uintptr_t hi = lo + size;
    TrackedActor* t = &copy;
    {
        for (size_t rg = 0; rg < t->regions.size(); rg++) {
            Region& r = t->regions[rg];
            for (size_t slot = 0; slot * 8 + 8 <= r.size; slot++) {
                if (ActorMemory_IsLocalSlot(*t, rg, slot)) {
                    continue; // the engine's own lists: it unlinks the actor itself
                }
                uint64_t v = ReadBytes(r.ptr + slot * 8, 8);
                if (v >= lo && v < hi) {
                    uint64_t init = ReadBytes(r.init.data() + slot * 8, 8);
                    WriteSlot(*t, rg, slot, (init >= lo && init < hi) ? 0 : init);
                }
            }
        }
    }
}

} // namespace coop::client
