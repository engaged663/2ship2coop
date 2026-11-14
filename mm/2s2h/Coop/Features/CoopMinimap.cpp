// [COOP] The other players on the minimap (see CoopMinimap.h), two ways:
// - in our scene, from their puppet (smooth: 20 poses a second). A puppet in a room that is not loaded here has no
//   draw function (PuppetActor.cpp) and goes the other way;
// - in any scene that shows the same minimap texture (Clock Town's five scenes share one), from "map_pos": each game
//   sends where its Link is on its own minimap (texture coordinates, at most 5 times a second while it moves, and every
//   2 s anyway for whoever arrives), and the others put that point on theirs, smoothed.
#include "CoopMinimap.h"

#include "PlayerColors.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/Puppet/PuppetActor.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "common/Protocol.h"

#include <chrono>
#include <cmath>
#include <map>

extern "C" {
#include "variables.h"
void MapDisp_Minimap_DrawActorIcon(PlayState* play, Actor* actor); // z_map_disp.c
}

using namespace coop;
using namespace coop::client;

namespace {

constexpr double kSendMinSeconds = 0.2;   // at most 5 a second while moving
constexpr double kHeartbeatSeconds = 2.0; // and this often anyway (whoever just arrived learns where we are)
constexpr double kStaleSeconds = 6.0;     // a position this old is not shown (its game stopped telling)
constexpr float kMoveTexels = 0.25f;      // smaller moves on the map's texture are not worth sending
constexpr int kTurnYaw = 0x800;           // nor turns smaller than this (about 11 degrees)
constexpr float kSmoothing = 0.3f;        // the arrow covers this part of the way to the latest point each frame

struct MapPos {
    int32_t map = -1; // minimap texture id, -1 = on no map
    float u = 0.0f;   // the point on that texture
    float v = 0.0f;
    int16_t yaw = 0;
};

struct RemoteMapPos {
    MapPos target;           // the latest one received
    float shownU = 0.0f;     // where its arrow is drawn now (moves toward target)
    float shownV = 0.0f;
    bool hasShown = false;
    double receivedAt = 0.0;
};

std::map<uint8_t, RemoteMapPos> sRemote;
MapPos sSent;
double sSentAt = -1.0;

Actor* sDrawing = nullptr;  // what MapDisp_Minimap_DrawActorIcon draws for us right now...
uint8_t sDrawingId = 0;     // ...for this player...
s16 sDrawingYaw = 0;        // ...pointing this way
Actor sMapMarker;           // stands for a player of another scene (only its position is read)

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool PuppetVisible(Actor* actor) {
    return actor != nullptr && actor->update != nullptr && actor->init == nullptr && actor->draw != nullptr;
}

void DrawArrow(PlayState* play, Actor* actor, uint8_t playerId, s16 yaw) {
    sDrawing = actor;
    sDrawingId = playerId;
    sDrawingYaw = yaw;
    MapDisp_Minimap_DrawActorIcon(play, actor);
    sDrawing = nullptr;
}

// A player of another scene (or of a room of ours that is not loaded) on our minimap, if it shows their map.
void DrawFromMapPos(PlayState* play, uint8_t playerId, RemoteMapPos& pos) {
    if (pos.target.map < 0 || Now() - pos.receivedAt > kStaleSeconds) {
        pos.hasShown = false;
        return;
    }
    if (!pos.hasShown) {
        pos.shownU = pos.target.u;
        pos.shownV = pos.target.v;
        pos.hasShown = true;
    } else {
        pos.shownU += (pos.target.u - pos.shownU) * kSmoothing;
        pos.shownV += (pos.target.v - pos.shownV) * kSmoothing;
    }
    f32 x, z;
    if (!MapDisp_Coop_MapToWorld(play, pos.target.map, pos.shownU, pos.shownV, &x, &z)) {
        return; // our room shows another map
    }
    sMapMarker.category = ACTORCAT_NPC;
    sMapMarker.world.pos = { x, GET_PLAYER(play)->actor.world.pos.y, z };
    DrawArrow(play, &sMapMarker, playerId, pos.target.yaw);
}

// Where our Link is on our minimap, to everyone else (FrameEnd).
void SendOurMapPos() {
    if (!Session_IsConnected()) {
        sSentAt = -1.0;
        return;
    }
    MapPos now;
    if (PoseCapture_InGameplay() && gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
        Player* player = GET_PLAYER(gPlayState);
        s32 mapId;
        f32 u, v;
        if (MapDisp_Coop_WorldToMap(gPlayState, player->actor.world.pos.x, player->actor.world.pos.z, &mapId, &u,
                                    &v)) {
            now = { mapId, u, v, player->actor.shape.rot.y };
        }
    } else if (gPlayState != nullptr && gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return; // changing scenes: keep the last one until the new map is there
    }
    double t = Now();
    bool changed = now.map != sSent.map ||
                   (now.map >= 0 && (std::fabs(now.u - sSent.u) >= kMoveTexels ||
                                     std::fabs(now.v - sSent.v) >= kMoveTexels ||
                                     std::abs((int16_t)(now.yaw - sSent.yaw)) >= kTurnYaw));
    bool due = sSentAt < 0.0 || (changed && t - sSentAt >= kSendMinSeconds) || t - sSentAt >= kHeartbeatSeconds;
    if (!due) {
        return;
    }
    json ev = MakeEvent(ev::kMapPos);
    ev["map"] = now.map;
    if (now.map >= 0) {
        ev["x"] = std::round(now.u * 100.0f) / 100.0f;
        ev["y"] = std::round(now.v * 100.0f) / 100.0f;
        ev["yaw"] = now.yaw;
    }
    NetClient::Get().SendEvent(ev);
    sSent = now;
    sSentAt = t;
}

void OnMapPos(const json& ev) {
    uint8_t id = (uint8_t)GetInt(ev, "id");
    RemoteMapPos& pos = sRemote[id];
    MapPos next;
    next.map = (int32_t)GetInt(ev, "map", -1);
    if (next.map >= 0) {
        next.u = (float)GetNumber(ev, "x");
        next.v = (float)GetNumber(ev, "y");
        next.yaw = (int16_t)GetInt(ev, "yaw");
    }
    if (next.map != pos.target.map) {
        pos.hasShown = false; // another map: jump there, no sliding across
    }
    pos.target = next;
    pos.receivedAt = Now();
}

void OnLeave(const json& ev) {
    sRemote.erase((uint8_t)GetInt(ev, "id"));
}

void OnLost(const std::string&) {
    sRemote.clear();
    sSentAt = -1.0;
}

void RegisterCoopMinimap() {
    COND_HOOK(OnGameStateMainFinish, true, SendOurMapPos);
}

} // namespace

extern "C" void Coop_MinimapDrawPlayers(PlayState* play) {
    if (!Session_IsConnected() || !PlayerColors_MinimapOn()) {
        return;
    }
    for (const auto& [id, player] : Session_Players()) {
        Actor* actor = PuppetManager_Actor(id);
        if (PuppetVisible(actor)) {
            DrawArrow(play, actor, id, actor->shape.rot.y);
            auto it = sRemote.find(id);
            if (it != sRemote.end()) {
                it->second.hasShown = false; // if it leaves our scene, its arrow starts from where it is then
            }
            continue;
        }
        auto it = sRemote.find(id);
        if (it != sRemote.end()) {
            DrawFromMapPos(play, id, it->second);
        }
    }
}

extern "C" s32 Coop_MinimapArrow(Actor* actor, Color_RGB8* color, s16* yaw) {
    if (actor == nullptr) {
        return 0;
    }
    if (actor == sDrawing) {
        Rgb c = PlayerColors_Shown(sDrawingId);
        *color = { c.r, c.g, c.b };
        *yaw = sDrawingYaw;
        return 1;
    }
    if (actor->category == ACTORCAT_PLAYER && Session_IsConnected() && PlayerColors_MinimapOwnColor()) {
        Rgb c = PlayerColors_Shown(Session_LocalId());
        *color = { c.r, c.g, c.b };
    }
    return 0;
}

COOP_ON_EVENT(minimapPos, ev::kMapPos, OnMapPos);
COOP_ON_EVENT(minimapLeave, ev::kLeave, OnLeave);
COOP_ON_LOST(minimapLost, OnLost);

static RegisterShipInitFunc sCoopMinimapInit(RegisterCoopMinimap);
