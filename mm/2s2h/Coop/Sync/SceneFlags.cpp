// [COOP] Sincronización total S4 (spec §5.1): the loaded scene's flags are the same in every game of the scene + layer,
// at once. Each frame what changed here goes out ("sflag"); the server sends every change to everyone in the scene,
// us too, so everyone applies them in its order. On arriving we send ours ("sflags"): the first one there gives its
// temporary flags to the scene, the others get the scene's. A flag another player turned on removes what only read it
// when created (LiveFlags.cpp) and recreates what never looks at it again (FlagReload.cpp). gCoop.Sync.SceneFlags.
#include "Sync.h"

#include "2s2h/Coop/Actors/LiveFlags.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/SceneFlags.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

namespace coop::client {

namespace {

using scene_flags::Words;

bool sArrived = false; // our flags of this scene went to the server
int sReadyFrames = 0;  // frames this scene has been ready: "sflags" goes on the second one
Words sShadow{};       // what the server knows of ours

Words WordsOf(const ActorContextSceneFlags& f) {
    return { f.chest,           f.switches[0],    f.switches[1],    f.switches[2],    f.switches[3],   f.clearedRoom,
             f.clearedRoomTemp, f.collectible[0], f.collectible[1], f.collectible[2], f.collectible[3] };
}

Words Read(const PlayState* play) {
    return WordsOf(play->actorCtx.sceneFlags);
}

void Write(PlayState* play, const Words& w) {
    ActorContextSceneFlags& f = play->actorCtx.sceneFlags;
    f.chest = w[scene_flags::kChest];
    for (int i = 0; i < 4; i++) {
        f.switches[i] = w[scene_flags::kSwitch0 + i];
        f.collectible[i] = w[scene_flags::kCollect0 + i];
    }
    f.clearedRoom = w[scene_flags::kClear];
    f.clearedRoomTemp = w[scene_flags::kClearTemp];
}

bool Ready(const PlayState* play) {
    return play != nullptr && Sync_On(SyncPart::SceneFlags) && !EndingMode_Active() && PoseCapture_InGameplay() &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && Session_IsConnected();
}

// What another player changed reaches what only reads its flag once (spec §5.1). live: also tell LiveFlags.cpp (the
// world's writes told it already: FieldTable.cpp).
void Triggers(const Words& before, const Words& after, bool live = true) {
    for (uint8_t w = 0; w < scene_flags::kWordCount; w++) {
        uint32_t changed = before[w] ^ after[w];
        for (int bit = 0; changed != 0 && bit < 32; bit++) {
            if (!(changed & (1u << bit))) {
                continue;
            }
            bool on = (after[w] & (1u << bit)) != 0;
            int kind = -1;
            int flag = bit;
            if (w == scene_flags::kChest) {
                kind = 1;
                if (on && live) {
                    LiveFlags_OnRemoteFlag(LiveFlagType::Chest, flag);
                }
            } else if (w >= scene_flags::kSwitch0 && w <= scene_flags::kSwitch3) {
                kind = 0;
                flag = (w - scene_flags::kSwitch0) * 32 + bit;
                if (on && live) {
                    LiveFlags_OnRemoteFlag(LiveFlagType::Switch, flag);
                }
            } else if (w == scene_flags::kClear) {
                kind = 2;
            } else if (w == scene_flags::kClearTemp) {
                kind = 3;
            } else {
                kind = 4;
                flag = (w - scene_flags::kCollect0) * 32 + bit;
                if (on && live && flag != 0) {
                    LiveFlags_OnRemoteFlag(LiveFlagType::Collectible, flag);
                }
            }
            FlagReload_OnRemoteChange(kind, flag);
        }
    }
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (!Ready(play)) {
        return;
    }
    Words now = Read(play);
    if (!sArrived) {
        // On the second ready frame: Location.cpp's "loc" of this scene left before on the same reliable channel, so
        // the server knows where we are when our flags arrive (it drops the flags of another scene)
        if (++sReadyFrames < 2) {
            return;
        }
        sArrived = true;
        sShadow = now;
        json ev = MakeEvent(ev::kSceneFlags);
        ev["scene"] = play->sceneId;
        ev["words"] = scene_flags::WordsToJson(now);
        NetClient::Get().SendEvent(ev);
        return;
    }
    std::vector<scene_flags::Op> ops = scene_flags::Diff(sShadow, now);
    if (ops.empty()) {
        return;
    }
    sShadow = now;
    json ev = MakeEvent(ev::kSceneFlag);
    ev["scene"] = play->sceneId;
    ev["ops"] = scene_flags::OpsToJson(ops);
    NetClient::Get().SendEvent(ev);
}

void OnSceneFlag(const json& ev) {
    PlayState* play = gPlayState;
    std::vector<scene_flags::Op> ops;
    if (!sArrived || !Ready(play) || GetInt(ev, "scene", -1) != play->sceneId || !scene_flags::OpsFromJson(ev, ops)) {
        return;
    }
    Words before = Read(play);
    Words after = before;
    for (const scene_flags::Op& op : ops) {
        scene_flags::Apply(after, op);
        scene_flags::Apply(sShadow, op); // the server's order: not ours to send again
    }
    Write(play, after);
    if (GetInt(ev, "from") != Session_LocalId()) {
        Triggers(before, after);
    }
}

void OnSceneFlags(const json& ev) {
    PlayState* play = gPlayState;
    Words kept{};
    if (!sArrived || !Ready(play) || GetInt(ev, "scene", -1) != play->sceneId || !scene_flags::WordsFromJson(ev, kept)) {
        return;
    }
    Words before = Read(play);
    Words after = before;
    for (uint8_t w = 0; w < scene_flags::kWordCount; w++) {
        if (scene_flags::IsTemporary(w)) {
            after[w] = kept[w];
            sShadow[w] = kept[w];
        }
    }
    Write(play, after);
    Triggers(before, after);
}

void Forget() {
    sArrived = false;
    sReadyFrames = 0;
}

void RegisterSceneFlags() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

void SceneFlags_NoteWorldWrite(const ActorContextSceneFlags& before) {
    if (gPlayState == nullptr) {
        return;
    }
    Words was = WordsOf(before);
    Words now = Read(gPlayState);
    for (uint8_t w = 0; w < scene_flags::kWordCount; w++) {
        if (scene_flags::IsTemporary(w)) {
            was[w] = now[w]; // the world only writes the cycle's words
        } else if (sArrived) {
            sShadow[w] = now[w]; // the world's: whoever changed them sent them already
        }
    }
    // Another player changed them: what read them only when created is created again, whether this or their "sflag"
    // arrives first (the later one finds nothing changed)
    Triggers(was, now, false);
}

static RegisterShipInitFunc sSceneFlagsInit(RegisterSceneFlags);

} // namespace coop::client

COOP_ON_EVENT(sceneFlagEvent, coop::ev::kSceneFlag, coop::client::OnSceneFlag);
COOP_ON_EVENT(sceneFlagsEvent, coop::ev::kSceneFlags, coop::client::OnSceneFlags);
COOP_ON_LOST(sceneFlagsLost, [](const std::string&) { coop::client::Forget(); });
