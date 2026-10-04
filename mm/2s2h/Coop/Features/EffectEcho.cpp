// [COOP] Effects echo (spec 2026-09-30-coop-grupos-limites §8). The particles (EffectSs) that our Link, an actor we
// simulate, our attacks (the hit marks of this game's collision pass) or a cutscene actor of the cutscene we show to
// others create go to the other games of the scene (stream kStreamEffects), which create them too: their copies of
// those actors never run the code that makes them. Their init data travels as 8-byte slots translated like an actor's
// memory (ActorMemory.h); one that points to something the other game cannot find is not created there. Cutscene ones
// are created only by who watches that cutscene; the rest within kNearDist of our Link (or while we watch its maker's
// cutscene). gCoop.Effects = 0 turns it off (sending and receiving).
#include "Cinema.h"
#include "Ending.h"
#include "2s2h/Coop/Actors/ActorMemory.h"
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/Authority.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Sync/Sync.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/EffectImage.h"
#include "common/StreamIds.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/effects/ovl_Effect_En_Ice_Block/z_eff_en_ice_block.h"
#include "overlays/effects/ovl_Effect_Ss_Blast/z_eff_ss_blast.h"
#include "overlays/effects/ovl_Effect_Ss_Bomb2/z_eff_ss_bomb2.h"
#include "overlays/effects/ovl_Effect_Ss_Bubble/z_eff_ss_bubble.h"
#include "overlays/effects/ovl_Effect_Ss_D_Fire/z_eff_ss_d_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Db/z_eff_ss_dead_db.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Dd/z_eff_ss_dead_dd.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Ds/z_eff_ss_dead_ds.h"
#include "overlays/effects/ovl_Effect_Ss_Dt_Bubble/z_eff_ss_dt_bubble.h"
#include "overlays/effects/ovl_Effect_Ss_Dust/z_eff_ss_dust.h"
#include "overlays/effects/ovl_Effect_Ss_En_Fire/z_eff_ss_en_fire.h"
#include "overlays/effects/ovl_Effect_Ss_En_Ice/z_eff_ss_en_ice.h"
#include "overlays/effects/ovl_Effect_Ss_Extra/z_eff_ss_extra.h"
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h"
#include "overlays/effects/ovl_Effect_Ss_Fire_Tail/z_eff_ss_fire_tail.h"
#include "overlays/effects/ovl_Effect_Ss_G_Fire/z_eff_ss_g_fire.h"
#include "overlays/effects/ovl_Effect_Ss_G_Ripple/z_eff_ss_g_ripple.h"
#include "overlays/effects/ovl_Effect_Ss_G_Spk/z_eff_ss_g_spk.h"
#include "overlays/effects/ovl_Effect_Ss_G_Splash/z_eff_ss_g_splash.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "overlays/effects/ovl_Effect_Ss_Hitmark/z_eff_ss_hitmark.h"
#include "overlays/effects/ovl_Effect_Ss_Ice_Piece/z_eff_ss_ice_piece.h"
#include "overlays/effects/ovl_Effect_Ss_Ice_Smoke/z_eff_ss_ice_smoke.h"
#include "overlays/effects/ovl_Effect_Ss_K_Fire/z_eff_ss_k_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "overlays/effects/ovl_Effect_Ss_Kirakira/z_eff_ss_kirakira.h"
#include "overlays/effects/ovl_Effect_Ss_Lightning/z_eff_ss_lightning.h"
#include "overlays/effects/ovl_Effect_Ss_Sbn/z_eff_ss_sbn.h"
#include "overlays/effects/ovl_Effect_Ss_Sibuki/z_eff_ss_sibuki.h"
#include "overlays/effects/ovl_Effect_Ss_Solder_Srch_Ball/z_eff_ss_solder_srch_ball.h"
#include "overlays/effects/ovl_Effect_Ss_Stick/z_eff_ss_stick.h"
#include "overlays/effects/ovl_Effect_Ss_Stone1/z_eff_ss_stone1.h"
}

namespace coop::client {

namespace {

constexpr float kNearDist = 3000.f; // effects farther than this from our Link are not created
constexpr int kMaxPerSecond = 40;   // effects sent per second

struct InitInfo {
    int type;
    size_t bytes; // of its init data (EffectSs_Spawn passes only a pointer)
    size_t pos;   // where its position (a Vec3f) is in it
};

#define INIT(type, Params) { type, sizeof(Params), offsetof(Params, pos) }

// The init data of every effect of the game. ADD A LINE for a new effect type.
constexpr InitInfo kInits[] = {
    INIT(EFFECT_SS_DUST, EffectSsDustInitParams),
    INIT(EFFECT_SS_KIRAKIRA, EffectSsKirakiraInitParams),
    INIT(EFFECT_SS_BOMB2, EffectSsBomb2InitParams),
    INIT(EFFECT_SS_BLAST, EffectSsBlastInitParams),
    INIT(EFFECT_SS_G_SPK, EffectSsGSpkInitParams),
    INIT(EFFECT_SS_D_FIRE, EffectSsDFireInitParams),
    INIT(EFFECT_SS_BUBBLE, EffectSsBubbleInitParams),
    INIT(EFFECT_SS_G_RIPPLE, EffectSsGRippleInitParams),
    INIT(EFFECT_SS_G_SPLASH, EffectSsGSplashInitParams),
    INIT(EFFECT_SS_G_FIRE, EffectSsGFireInitParams),
    INIT(EFFECT_SS_LIGHTNING, EffectSsLightningInitParams),
    INIT(EFFECT_SS_DT_BUBBLE, EffectSsDtBubbleInitParams),
    INIT(EFFECT_SS_HAHEN, EffectSsHahenInitParams),
    INIT(EFFECT_SS_STICK, EffectSsStickInitParams),
    INIT(EFFECT_SS_SIBUKI, EffectSsSibukiInitParams),
    INIT(EFFECT_SS_STONE1, EffectSsStone1InitParams),
    INIT(EFFECT_SS_HITMARK, EffectSsHitmarkInitParams),
    INIT(EFFECT_SS_FHG_FLASH, EffectSsFhgFlashInitParams),
    INIT(EFFECT_SS_K_FIRE, EffectSsKFireInitParams),
    INIT(EFFECT_SS_SOLDER_SRCH_BALL, EffectSsSolderSrchBallInitParams),
    INIT(EFFECT_SS_KAKERA, EffectSsKakeraInitParams),
    INIT(EFFECT_SS_ICE_PIECE, EffectSsIcePieceInitParams),
    INIT(EFFECT_SS_EN_ICE, EffectSsEnIceInitParams),
    INIT(EFFECT_SS_FIRE_TAIL, EffectSsFireTailInitParams),
    INIT(EFFECT_SS_EN_FIRE, EffectSsEnFireInitParams),
    INIT(EFFECT_SS_EXTRA, EffectSsExtraInitParams),
    INIT(EFFECT_SS_DEAD_DB, EffectSsDeadDbInitParams),
    INIT(EFFECT_SS_DEAD_DD, EffectSsDeadDdInitParams),
    INIT(EFFECT_SS_DEAD_DS, EffectSsDeadDsInitParams),
    INIT(EFFECT_SS_ICE_SMOKE, EffectSsIceSmokeInitParams),
    INIT(EFFECT_EN_ICE_BLOCK, EffectEnIceBlockInitParams),
    INIT(EFFECT_SS_SBN, EffectSsSbnInitParams),
};

#undef INIT

constexpr bool InitsFit() {
    for (const InitInfo& s : kInits) {
        if (s.bytes == 0 || s.bytes > (size_t)effect_limits::kSlots * 8 || s.pos + sizeof(Vec3f) > s.bytes) {
            return false;
        }
    }
    return true;
}
static_assert(InitsFit(), "an effect's init data must fit in effect_limits::kSlots slots");
static_assert(EFFECT_SS_TYPE_MAX == effect_limits::kEffectTypes, "EffectImage.h's kEffectTypes");

const InitInfo* InitOf(int type) {
    for (const InitInfo& s : kInits) {
        if (s.type == type) {
            return &s;
        }
    }
    return nullptr;
}

bool sApplying = false;      // creating an effect another game sent: never sent back
bool sCollisionPass = false; // this frame's collision checks are running (Coop_CollisionPass)
EffectPacket sOut;           // this frame's effects for everyone near
EffectPacket sOutCinema;     // ...and the ones of our cutscene for who watches it
int sSentThisSecond = 0;
int64_t sSecondStartMs = 0;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// (The cheap checks first: this runs for every particle of the game.)
bool On() {
    return WorldSession_Active() && gPlayState != nullptr && !EndingMode_Active() && Authority_Known() &&
           CVarGetInteger("gCoop.Effects", 1) != 0;
}

bool TakeBudget() {
    int64_t now = NowMs();
    if (now - sSecondStartMs >= 1000) {
        sSecondStartMs = now;
        sSentThisSecond = 0;
    }
    return sSentThisSecond++ < kMaxPerSecond;
}

// Who made it decides who sees it: a cutscene actor of ours while others watch our cutscene (only they create it);
// our Link, an actor we simulate or one of our attacks landing (everyone near creates it). Nothing else is sent: an
// effect made by an effect, by a draw function or by a copy's actor is made by every game on its own.
bool Sendable(bool* cinema) {
    *cinema = false;
    Actor* a = ActorSync_AnyUpdating();
    if (a == nullptr) {
        return sCollisionPass; // no copy and no puppet attacks in this game: every hit of this pass is ours
    }
    if (a == gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first) {
        return !HostMode_Enabled(); // the Link of the server's own game is a ghost: nobody sees it
    }
    TrackedActor* t = ActorRegistry_Get(a);
    if (t == nullptr || !Leases_IsMine(*t)) {
        return false;
    }
    if (t->cinema) {
        *cinema = true;
        return Cinema_DirectingShared();
    }
    return true;
}

void Flush() {
    for (EffectPacket* p : { &sOut, &sOutCinema }) {
        if (p->effects.empty()) {
            continue;
        }
        p->scene = gPlayState != nullptr ? gPlayState->sceneId : -1;
        p->cinema = p == &sOutCinema;
        if (p->scene >= 0 && On()) {
            for (std::vector<uint8_t>& bytes : EncodeEffects(*p)) {
                NetClient::Get().SendStream(std::move(bytes));
            }
        }
        p->effects.clear();
    }
}

// Far ones are not created (buf: its init data, pointers already ours).
bool Near(const InitInfo& info, const uint8_t* buf, const Player* link) {
    Vec3f pos;
    std::memcpy(&pos, buf + info.pos, sizeof(Vec3f));
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
        return false;
    }
    return Math_Vec3f_DistXYZ(&pos, (Vec3f*)&link->actor.world.pos) < kNearDist;
}

void OnEffects(const uint8_t* data, size_t size) {
    PlayState* play = gPlayState;
    EffectPacket p;
    if (!On() || !DecodeEffects(data, size, p) || p.scene != play->sceneId || p.playerId == 0 ||
        p.playerId == Session_LocalId()) {
        return;
    }
    bool watching = Cinema_WatchingFrom(p.playerId);
    if (p.cinema && !watching) {
        return; // its cutscene: only who watches it
    }
    Player* link = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    for (const EffectRecord& r : p.effects) {
        const InitInfo* info = InitOf(r.type);
        if (info == nullptr || r.slots.size() * 8 < info->bytes) {
            continue;
        }
        alignas(16) uint8_t buf[effect_limits::kSlots * 8] = {};
        bool ok = true;
        for (size_t i = 0; i < r.slots.size() && ok; i++) {
            const Slot& s = r.slots[i];
            uint64_t v = 0;
            if (s.kind == SlotKind::Raw) {
                v = s.value;
            } else if (s.kind == SlotKind::Keep) {
                ok = false;
            } else if (s.kind != SlotKind::Zero) {
                uint8_t* q = ActorMemory_Resolve(s);
                ok = q != nullptr;
                v = (uint64_t)(uintptr_t)q;
            }
            std::memcpy(buf + i * 8, &v, 8);
        }
        // we look through its maker's camera (its cutscene), or it is near our Link
        if (!ok || (!watching && link != nullptr && !Near(*info, buf, link))) {
            continue;
        }
        sApplying = true;
        EffectSs_Spawn(play, r.type, r.priority, buf);
        sApplying = false;
    }
}

void Forget() {
    sOut.effects.clear();
    sOutCinema.effects.clear();
    sCollisionPass = false;
}

void RegisterEffectEcho() {
    COND_HOOK(OnGameStateMainFinish, true, Flush);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_CollisionPass(s32 begin) {
    sCollisionPass = begin != 0;
}

extern "C" s32 Coop_OnEffectSpawn(PlayState* play, s32 type, s32 priority, void* initData) {
    if (CopyCode_InCopyInit() && On()) {
        return 1; // the Init of a copy we create: its owner already sent its particles (Sync/CopyCode.cpp)
    }
    bool cinema = false;
    if (sApplying || initData == nullptr || play == nullptr || !On() || type < 0 || type >= EFFECT_SS_TYPE_MAX ||
        !Sendable(&cinema)) {
        return 0;
    }
    const InitInfo* info = InitOf(type);
    if (info == nullptr || !TakeBudget()) {
        return 0;
    }
    EffectRecord r;
    r.type = (uint8_t)type;
    r.priority = (uint8_t)std::clamp<s32>(priority, 0, 255);
    for (size_t off = 0; off < info->bytes; off += 8) {
        uint64_t raw = 0;
        std::memcpy(&raw, (const uint8_t*)initData + off, std::min<size_t>(8, info->bytes - off));
        Slot s = ActorMemory_Classify(raw);
        if (s.kind == SlotKind::Keep) {
            return 0; // points to something only this game has
        }
        r.slots.push_back(s);
    }
    (cinema ? sOutCinema : sOut).effects.push_back(std::move(r));
    return 0;
}

COOP_ON_STREAM(effectEcho, coop::kStreamEffects, OnEffects);
static RegisterShipInitFunc sEffectEchoInit(RegisterEffectEcho);
