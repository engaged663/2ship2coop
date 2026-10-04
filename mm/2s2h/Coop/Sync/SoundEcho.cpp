// [COOP] Sincronización total S1 (spec §2): the sounds of our Link and of what we simulate travel to the other games;
// theirs play here on their puppet or copy. Every sound of the game passes through AudioSfx_PlaySfx (Coop_OnSfx) and
// SoundSource_Add (Coop_OnWorldSfx). Whose a sound is: where it plays (inside our Link, inside an actor we simulate),
// else who is updating (our Link, an actor we simulate). A copy's own code is silent (CopyCode.cpp): its owner sends
// its sounds. gCoop.Sync.Sounds = 0 turns it all off (sending, receiving and the silence); the server too ("sounds").
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorMemory.h"
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <array>
#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64sound_source.h"
}

namespace coop::client {

namespace {

bool sOn = false;      // this frame's switch (this runs for every sound of the game)
int sSilence = 0;      // a copy's own code is running
bool sPlaying = false; // we are playing a sound another game sent
bool sWorld = false;   // inside SoundSource_Add: its own AudioSfx_PlaySfx is not captured again
bool sFlagged = false; // inside Actor_UpdateFlaggedAudio
std::vector<SoundEntry> sLinkSounds;

// The engine reads freq, vol and reverb through pointers while a sound lasts: a fixed pool, one per (body, sound).
struct Values {
    const void* who = nullptr;
    uint16_t sfx = 0;
    f32 freq = 1.f;
    f32 vol = 1.f;
    s8 reverb = 0;
};
std::array<Values, 128> sValues;
size_t sNextValues = 0;

Values* ValuesFor(const void* who, const SoundEntry& s) {
    Values* v = nullptr;
    for (Values& e : sValues) {
        if (e.who == who && e.sfx == s.sfx) {
            v = &e;
            break;
        }
    }
    if (v == nullptr) {
        v = &sValues[sNextValues++ % sValues.size()];
        v->who = who;
        v->sfx = s.sfx;
    }
    float scale = std::clamp(CVarGetInteger("gCoop.Sync.RemoteVolume", 100), 0, 100) / 100.f;
    v->freq = SoundFreq(s);
    v->vol = SoundVol(s) * scale;
    v->reverb = s.reverb;
    return v;
}

Player* LocalLink() {
    return gPlayState != nullptr ? (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
}

bool Inside(const void* p, const void* start, size_t size) {
    return (uintptr_t)p >= (uintptr_t)start && (uintptr_t)p < (uintptr_t)start + size;
}

// Not the menus' and texts' (system) nor the ocarina's (OcarinaEcho.cpp).
bool LinkBank(uint16_t sfx) {
    uint32_t bank = SFX_BANK(sfx);
    return bank != BANK_SYSTEM && bank != BANK_OCARINA;
}

// Where it plays, when that is a pointer the other games can find.
void AddPlace(SoundEntry& e, const void* pos) {
    if (pos == nullptr) {
        return;
    }
    Slot s = ActorMemory_Classify((uint64_t)(uintptr_t)pos);
    if (s.kind == SlotKind::Exe || s.kind == SlotKind::Actor || s.kind == SlotKind::Link) {
        e.flags |= SoundEntry::kPos;
        e.pos = s;
    }
}

void ToLink(SoundEntry e, const void* pos, Player* link, bool world) {
    if (!LinkBank(e.sfx) || sLinkSounds.size() >= (size_t)sound_limits::kPerPose) {
        return;
    }
    if (!world && pos != &link->actor.projectedPos) {
        AddPlace(e, pos); // not its own body: a copy it hit, the screen's centre...
    }
    sLinkSounds.push_back(e);
}

void ToActor(TrackedActor& t, SoundEntry e, const void* pos, bool world) {
    if (t.sounds.size() >= (size_t)sound_limits::kPerRecord) {
        return;
    }
    if (!world && pos != &t.actor->projectedPos) {
        AddPlace(e, pos);
    }
    t.sounds.push_back(e);
}

// An actor we simulate (a cutscene one only while others watch our cutscene).
TrackedActor* Ours(TrackedActor* t) {
    if (t == nullptr || t->actor == nullptr || t->actor->update == nullptr || !Leases_IsMine(*t)) {
        return nullptr;
    }
    return (t->cinema && !Cinema_DirectingShared()) ? nullptr : t;
}

TrackedActor* TrackedAt(const void* p) {
    if (p == nullptr) {
        return nullptr;
    }
    Slot s = ActorMemory_Classify((uint64_t)(uintptr_t)p);
    return s.kind == SlotKind::Actor ? ActorRegistry_Find((uint32_t)(s.value >> 32)) : nullptr;
}

// Whose a sound of this game is (spec §2.1). The server's own game never sends its ghost Link's.
void Capture(const SoundEntry& e, const void* pos, bool world) {
    Player* link = LocalLink();
    if (link == nullptr) {
        return;
    }
    bool ghost = HostMode_Enabled();
    Actor* updating = ActorSync_AnyUpdating();
    if (Inside(pos, link, sizeof(Player))) {
        if (!ghost) {
            ToLink(e, pos, link, world);
        }
        return;
    }
    if (TrackedActor* at = TrackedAt(pos)) {
        if (Ours(at) != nullptr) {
            ToActor(*at, e, pos, world);
        } else if (updating == &link->actor && !ghost) {
            ToLink(e, pos, link, world); // our Link at another game's copy (its sword on it)
        }
        return;
    }
    if (updating == nullptr) {
        return;
    }
    if (updating == &link->actor) {
        if (!ghost) {
            ToLink(e, pos, link, world);
        }
        return;
    }
    if (TrackedActor* t = Ours(ActorRegistry_Get(updating))) {
        ToActor(*t, e, pos, world);
    }
}

// The place where a received sound plays: a live actor's projectedPos (the engine stops those sounds when the actor
// goes) or a global of the executable; never another pointer into an actor (it could be freed while it plays).
Vec3f* Place(Actor* body, const SoundEntry& s) {
    if (!(s.flags & SoundEntry::kPos)) {
        return &body->projectedPos;
    }
    switch (s.pos.kind) {
        case SlotKind::Exe: {
            uint8_t* p = ActorMemory_Resolve(s.pos);
            return p != nullptr ? (Vec3f*)p : &body->projectedPos;
        }
        case SlotKind::Actor: {
            TrackedActor* t = ActorRegistry_Find((uint32_t)(s.pos.value >> 32));
            return (t != nullptr && t->actor != nullptr) ? &t->actor->projectedPos : &body->projectedPos;
        }
        case SlotKind::Link: {
            uint8_t pid = (uint8_t)(s.pos.value >> 32);
            Actor* who = pid == Session_LocalId() ? (Actor*)LocalLink() : PuppetManager_Actor(pid);
            return who != nullptr ? &who->projectedPos : &body->projectedPos;
        }
        default:
            return &body->projectedPos;
    }
}

void FrameStart() {
    sOn = gPlayState != nullptr && Sync_On(SyncPart::Sounds);
    sLinkSounds.clear(); // a pose that was not sent takes nothing late
}

void Forget() {
    sLinkSounds.clear();
    sSilence = 0;
    for (Values& v : sValues) {
        v.who = nullptr;
    }
}

void RegisterSoundEcho() {
    COND_HOOK(OnGameStateMainStart, true, FrameStart);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

std::vector<SoundEntry> SoundEcho_TakeLinkSounds() {
    std::vector<SoundEntry> out;
    out.swap(sLinkSounds);
    return out;
}

void SoundEcho_PlayOn(Actor* body, const SoundEntry& s) {
    if (!sOn || body == nullptr || gPlayState == nullptr || !Sfx_Valid(s.sfx)) {
        return;
    }
    sPlaying = true;
    if (s.flags & SoundEntry::kWorld) {
        Vec3f w = { s.world[0], s.world[1], s.world[2] };
        if (s.flags & SoundEntry::kEachFrame) {
            SoundSource_PlaySfxEachFrameAtFixedWorldPos(gPlayState, &w, s.duration, s.sfx);
        } else {
            SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &w, s.duration, s.sfx);
        }
    } else {
        Values* v = ValuesFor(body, s);
        AudioSfx_PlaySfx(s.sfx, Place(body, s), s.token, &v->freq, &v->vol, &v->reverb);
    }
    sPlaying = false;
}

void SoundEcho_SilenceBegin() {
    sSilence++;
}

void SoundEcho_SilenceEnd() {
    if (sSilence > 0) {
        sSilence--;
    }
}

static RegisterShipInitFunc sSoundEchoInit(RegisterSoundEcho);

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" s32 Coop_OnSfx(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* volume, s8* reverbAdd) {
    if (!sOn) {
        return 0;
    }
    if (sSilence > 0) {
        return 1;
    }
    if (sPlaying || sWorld || sFlagged) {
        return 0;
    }
    SoundEntry e = MakeSound(sfxId, freqScale != nullptr ? *freqScale : 1.f, volume != nullptr ? *volume : 1.f,
                             reverbAdd != nullptr ? *reverbAdd : 0, token);
    Capture(e, pos, false);
    return 0;
}

extern "C" void Coop_OnWorldSfx(PlayState* play, Vec3f* worldPos, u32 duration, u16 sfxId, u32 eachFrame,
                                s32 begin) {
    if (!begin) {
        sWorld = false;
        return;
    }
    sWorld = true;
    if (!sOn || sPlaying || sSilence > 0 || worldPos == nullptr || !std::isfinite(worldPos->x) ||
        !std::isfinite(worldPos->y) || !std::isfinite(worldPos->z) ||
        std::fabs(worldPos->x) > sound_limits::kWorldLimit || std::fabs(worldPos->y) > sound_limits::kWorldLimit ||
        std::fabs(worldPos->z) > sound_limits::kWorldLimit) {
        return;
    }
    SoundEntry e = MakeSound(sfxId, 1.f, 1.f, 0, 4);
    e.flags |= SoundEntry::kWorld | (eachFrame ? SoundEntry::kEachFrame : 0);
    e.world[0] = worldPos->x;
    e.world[1] = worldPos->y;
    e.world[2] = worldPos->z;
    e.duration = (uint16_t)std::min<u32>(duration, 0xFFFF);
    Capture(e, worldPos, true);
}

extern "C" void Coop_FlaggedAudio(s32 begin) {
    sFlagged = begin != 0;
}
