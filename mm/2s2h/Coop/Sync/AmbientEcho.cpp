// [COOP] Sincronización total S2 (spec §3): the music orders and camera quakes an actor makes while this game
// simulates it go to the other players of its room ("ambient"), who play them: a miniboss's music, a boss stomping, a
// minigame's fanfare. Never a cutscene actor's (Cinema.cpp shares the cutscene's music). gCoop.Sync.Ambient.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Ambient.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <map>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64quake.h"
}

namespace coop::client {

namespace {

struct Out {
    std::vector<ambient::Music> music;
    std::vector<s16> quakes; // request indexes: their values are read at the end of the frame
};

std::map<int8_t, Out> sOut; // room -> this frame's
bool sApplying = false;     // playing what another game sent: never sent back
int sDepth = 0;             // inside a music function (its own raw orders are not counted)

// The room of the actor we simulate whose update runs now (a player's object: where its player is).
bool Context(int8_t* room) {
    TrackedActor* t = ActorSync_Updating();
    if (sApplying || t == nullptr || t->cinema || gPlayState == nullptr || !Sync_On(SyncPart::Ambient)) {
        return false;
    }
    *room = t->owner != 0 ? (int8_t)gPlayState->roomCtx.curRoom.num : t->room;
    return *room >= 0;
}

void Push(ambient::MusicKind kind, uint32_t value) {
    int8_t room = -1;
    if (Context(&room) && sOut[room].music.size() < (size_t)ambient::kMaxMusic) {
        sOut[room].music.push_back({ kind, value });
    }
}

void FrameEnd() {
    if (sOut.empty()) {
        return;
    }
    std::map<int8_t, Out> out;
    out.swap(sOut);
    if (gPlayState == nullptr || !WorldSession_Active() || !Session_IsConnected()) {
        return;
    }
    for (auto& [room, o] : out) {
        std::vector<ambient::Quake> quakes;
        for (s16 index : o.quakes) {
            s16 v[7];
            // Only what the server accepts (it counts the rest towards a kick): a longer quake is cut
            if (Coop_QuakeRead(index, v) && v[0] >= 1 && v[0] < ambient::kQuakeTypes && v[6] >= 1) {
                s16 duration = (s16)std::min<int>(v[6], ambient::kMaxQuakeDuration);
                quakes.push_back({ v[0], v[1], v[2], v[3], v[4], v[5], duration });
            }
        }
        if (o.music.empty() && quakes.empty()) {
            continue;
        }
        json ev = MakeEvent(ev::kAmbient);
        ev["scene"] = gPlayState->sceneId;
        ev["room"] = room;
        ev["music"] = ambient::MusicToJson(o.music);
        ev["quake"] = ambient::QuakesToJson(quakes);
        NetClient::Get().SendEvent(ev);
    }
}

void OnAmbient(const json& ev) {
    PlayState* play = gPlayState;
    if (play == nullptr || !Sync_On(SyncPart::Ambient) || EndingMode_Active() || Cinema_Watching() ||
        GetInt(ev, "scene", -1) != play->sceneId) {
        return;
    }
    int64_t room = GetInt(ev, "room", -1);
    if (room != play->roomCtx.curRoom.num && room != play->roomCtx.prevRoom.num) {
        return; // only who is in that room hears and feels it
    }
    std::vector<ambient::Music> music;
    std::vector<ambient::Quake> quakes;
    if (!ambient::MusicFromJson(ev, music) || !ambient::QuakesFromJson(ev, quakes)) {
        return;
    }
    sApplying = true;
    for (const ambient::Music& m : music) {
        switch (m.kind) {
            case ambient::MusicKind::Cmd:
                AudioSeq_QueueSeqCmd(m.value);
                break;
            case ambient::MusicKind::StorePrevBgm:
                Audio_PlayBgm_StorePrevBgm((u16)m.value);
                break;
            case ambient::MusicKind::RestorePrevBgm:
                Audio_RestorePrevBgm();
                break;
            case ambient::MusicKind::PlaySubBgm:
                Audio_PlaySubBgm((u16)m.value);
                break;
            case ambient::MusicKind::StopSubBgm:
                Audio_StopSubBgm();
                break;
            case ambient::MusicKind::Fanfare:
                Audio_PlayFanfare((u16)m.value);
                break;
        }
    }
    for (const ambient::Quake& q : quakes) {
        s16 index = Quake_Request(GET_ACTIVE_CAM(play), (u32)q.type);
        Quake_SetSpeed(index, q.speed);
        Quake_SetPerturbations(index, q.y, q.x, q.fov, q.roll);
        Quake_SetDuration(index, q.duration);
    }
    sApplying = false;
}

void Forget() {
    sOut.clear();
    sDepth = 0;
}

void RegisterAmbientEcho() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnSeqCmd(u32 cmd) {
    if (sDepth == 0 && ambient::SeqCmdAllowed(cmd)) {
        Push(ambient::MusicKind::Cmd, cmd);
    }
}

extern "C" void Coop_MusicBegin(s32 kind, u32 value) {
    if (sDepth == 0 && kind > 0 && kind < ambient::kMusicKinds) {
        Push((ambient::MusicKind)kind, value);
    }
    sDepth++;
}

extern "C" void Coop_MusicEnd(void) {
    if (sDepth > 0) {
        sDepth--;
    }
}

extern "C" void Coop_OnQuake(s16 index) {
    int8_t room = -1;
    if (Context(&room) && sOut[room].quakes.size() < (size_t)ambient::kMaxQuakes) {
        sOut[room].quakes.push_back(index);
    }
}

COOP_ON_EVENT(ambientEcho, coop::ev::kAmbient, coop::client::OnAmbient);
static RegisterShipInitFunc sAmbientEchoInit(coop::client::RegisterAmbientEcho);
