// [COOP] Sincronización total S6 (spec §7): the notes another player's ocarina plays (in their pose) sound here on the
// ocarina channel, at their puppet, while our own ocarina is quiet: the nearest one playing within kHearDist.
// A song played right ("song", z_message.c): the others near us hear its music (with the instrument of the form that
// played it) and, right next to us, see its effect (its wipe, the Song of Storms' rain), as we do.
// gCoop.Sync.Ocarina (gCoop.Sync.SongEffects: the effects only); server.json "ocarina".
#include "Sync.h"

#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "common/PlayerState.h"
#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "functions.h"
#include "variables.h"
void Coop_SongEcho(PlayState* play, u8 song, u8 form, Vec3f* pos, s32 effect); // z_message.c
}

static_assert(coop::kMaxSong == OCARINA_SONG_DOUBLE_TIME, "Protocol.h kMaxSong: the last song with music of its own");

namespace coop::client {

namespace {

constexpr float kHearDist = 1500.f;
constexpr float kSeeDist = 800.f; // a song's wipe is drawn at our camera: only right next to who played it

// After our Link updates (the puppets took this frame's poses).
void Tick(Actor* linkActor) {
    PlayState* play = gPlayState;
    if (play == nullptr || linkActor == nullptr || !Sync_On(SyncPart::Ocarina) || Coop_OcarinaLocalBusy()) {
        Coop_OcarinaRemoteStop();
        return;
    }
    Actor* best = nullptr;
    PlayerState bestState;
    float bestDist = kHearDist;
    for (const auto& [id, remote] : Session_Players()) {
        Actor* puppet = PuppetManager_Actor(id);
        PlayerState st;
        if (puppet == nullptr || puppet->update == nullptr || !PuppetManager_GetCurrent(id, st) ||
            st.ocarinaInstrument == 0) {
            continue;
        }
        float d = Actor_WorldDistXYZToActor(linkActor, puppet);
        if (d < bestDist) {
            best = puppet;
            bestState = st;
            bestDist = d;
        }
    }
    if (best == nullptr) {
        Coop_OcarinaRemoteStop();
        return;
    }
    Coop_OcarinaRemote(bestState.ocarinaInstrument, bestState.ocarinaPitch, bestState.ocarinaBend / 4096.f,
                       bestState.ocarinaVibrato, &best->projectedPos);
}

// Another player's song: never over our own ocarina, a text, a cutscene or the pause menu.
void OnSong(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from", 0);
    int64_t song = GetInt(ev, "song", -1);
    int64_t form = GetInt(ev, "form", -1);
    if (play == nullptr || !Sync_On(SyncPart::Ocarina) || from == 0 || from == Session_LocalId() ||
        GetInt(ev, "scene", -1) != play->sceneId || song < 0 || song > kMaxSong || form < 0 ||
        form >= pose_limits::kFormCount || Coop_OcarinaLocalBusy() || play->msgCtx.msgMode != MSGMODE_NONE ||
        Play_InCsMode(play) || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    Actor* puppet = PuppetManager_Actor(from);
    Player* link = GET_PLAYER(play);
    if (puppet == nullptr || puppet->update == nullptr || link == nullptr) {
        return;
    }
    float d = Actor_WorldDistXYZToActor(&link->actor, puppet);
    if (d > kHearDist) {
        return;
    }
    bool effect = d < kSeeDist && CVarGetInteger("gCoop.Sync.SongEffects", 1) != 0;
    Coop_OcarinaRemoteStop(); // its last note: the song's music takes over
    Coop_SongEcho(play, (u8)song, (u8)form, &puppet->world.pos, effect ? 1 : 0);
}

void RegisterOcarinaEcho() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, Tick);
    COND_HOOK(OnPlayDestroy, true, []() { Coop_OcarinaRemoteStop(); });
}

} // namespace

static RegisterShipInitFunc sOcarinaEchoInit(RegisterOcarinaEcho);

} // namespace coop::client

COOP_ON_EVENT(songEcho, coop::ev::kSong, coop::client::OnSong);

// Our Link played that song right (z_message.c, as its music starts): the others of the scene hear it too.
extern "C" void Coop_OnSongPlayed(PlayState* play, u8 song, u8 form) {
    using namespace coop;
    if (play == nullptr || song > kMaxSong || form >= pose_limits::kFormCount || !client::Session_IsConnected() ||
        !client::Sync_On(client::SyncPart::Ocarina)) {
        return;
    }
    json ev = MakeEvent(ev::kSong);
    ev["scene"] = play->sceneId;
    ev["song"] = song;
    ev["form"] = form;
    client::NetClient::Get().SendEvent(ev);
}
