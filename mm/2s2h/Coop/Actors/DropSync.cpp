// [COOP] Per-player drops (spec §7). When a shared enemy of ours drops something, every other game of the scene
// drops its own copy with its own luck: each player picks up their own hearts, rupies and ammo. The replicas drop
// nothing (ActorSync.cpp clears their dropFlag), so nobody gets them twice. Only with gCoop.Sync.Drops off: on, the
// item itself is one for everyone (Sync/SharedDrops.cpp).
#include "ActorSync.h"
#include "Authority.h"
#include "CoopEngine.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Sync/Sync.h"
#include "2s2h/Coop/World/WorldSession.h"

#include <spdlog/spdlog.h>

#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace {

bool sDropping = false; // dropping an item another game told us about: do not report it back

void OnDrop(const coop::json& ev) {
    using namespace coop;
    if (!client::WorldSession_Active() || client::EndingMode_Active() || gPlayState == nullptr ||
        client::Sync_On(client::SyncPart::Drops) ||
        GetInt(ev, "scene", -1) != gPlayState->sceneId) {
        return;
    }
    auto pos = ev.find("pos");
    if (pos == ev.end() || !pos->is_array() || pos->size() != 3) {
        return;
    }
    Vec3f at = { (f32)(*pos)[0].get<double>(), (f32)(*pos)[1].get<double>(), (f32)(*pos)[2].get<double>() };
    int64_t params = GetInt(ev, "params");
    sDropping = true;
    if (GetInt(ev, "fn") == 1) {
        // Fixed items never carry a collectible flag across games (0x7F00: the flag of a unique pickup).
        Item_DropCollectible(gPlayState, &at, (u32)(params & ~0x7F00));
    } else {
        Item_DropCollectibleRandom(gPlayState, nullptr, &at, (s16)params);
    }
    sDropping = false;
}

} // namespace

extern "C" void Coop_OnDrop(PlayState* play, Vec3f* pos, s32 params, s32 fn) {
    using namespace coop;
    client::TrackedActor* t = client::ActorSync_Updating();
    if (sDropping || t == nullptr || client::Sync_On(client::SyncPart::Drops) || t->cinema || !client::WorldSession_Active() || !std::isfinite(pos->x) ||
        !std::isfinite(pos->y) || !std::isfinite(pos->z)) {
        return;
    }
    json ev = MakeEvent(ev::kDrop);
    ev["scene"] = play->sceneId;
    ev["room"] = t->room;
    ev["pos"] = { pos->x, pos->y, pos->z };
    ev["params"] = params;
    ev["fn"] = fn;
    client::NetClient::Get().SendEvent(ev);
}

COOP_ON_EVENT(dropEvent, coop::ev::kDrop, OnDrop);
