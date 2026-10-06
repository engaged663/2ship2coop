#include "Warp.h"

#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/Room/Room.h"

#include "common/PlayerState.h"

#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
extern SceneEntranceTableEntry sSceneEntranceTable[]; // z_scene_table.c
}

namespace coop::client {

bool Warp_IsValidEntrance(uint16_t entrance) {
    uint32_t sceneIndex = entrance >> 9;
    return sceneIndex < ENTR_SCENE_MAX && sSceneEntranceTable[sceneIndex].table != nullptr;
}

bool Warp_IsValid(const WarpTarget& t) {
    if (!Warp_IsValidEntrance(t.entrance) || t.room < 0) {
        return false;
    }
    for (float v : t.pos) {
        if (!std::isfinite(v) || std::fabs(v) > pose_limits::kWorldLimit) {
            return false;
        }
    }
    return true;
}

bool Warp_FromJson(const json& j, WarpTarget& out) {
    if (!j.is_object()) {
        return false;
    }
    auto pos = j.find("pos");
    if (pos == j.end() || !pos->is_array() || pos->size() != 3) {
        return false;
    }
    WarpTarget t;
    for (size_t i = 0; i < 3; i++) {
        const json& v = (*pos)[i];
        if (!v.is_number() || !(std::fabs(v.get<double>()) <= pose_limits::kWorldLimit)) {
            return false;
        }
        t.pos[i] = (float)v.get<double>();
    }
    int64_t entrance = GetInt(j, "entrance", -1);
    int64_t room = GetInt(j, "room", -1);
    if (entrance < 0 || entrance > 0xFFFF || room < 0 || room > 127) {
        return false;
    }
    t.entrance = (uint16_t)entrance;
    t.room = (int8_t)room;
    t.rot = (int16_t)GetInt(j, "rot", 0);
    if (!Warp_IsValid(t)) {
        return false;
    }
    out = t;
    return true;
}

json Warp_ToJson(const WarpTarget& t) {
    return json{ { "entrance", t.entrance },
                 { "room", t.room },
                 { "pos", json::array({ t.pos[0], t.pos[1], t.pos[2] }) },
                 { "rot", t.rot } };
}

bool Warp_Current(WarpTarget& out) {
    if (!PoseCapture_InGameplay()) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    WarpTarget t;
    t.entrance = (uint16_t)gSaveContext.save.entrance;
    t.room = gPlayState->roomCtx.curRoom.num;
    t.pos[0] = player->actor.world.pos.x;
    t.pos[1] = player->actor.world.pos.y;
    t.pos[2] = player->actor.world.pos.z;
    t.rot = player->actor.shape.rot.y;
    if (!Warp_IsValid(t)) {
        return false;
    }
    out = t;
    return true;
}

std::string Warp_BlockedReason() {
    if (!PoseCapture_InGameplay()) {
        return "no estás dentro de una partida";
    }
    PlayState* play = gPlayState;
    Player* player = GET_PLAYER(play);
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return "estás cambiando de escenario";
    }
    if (coop::client::RoomHold_Active()) {
        return "tu partida espera a que empiece tu sala";
    }
    if (player->stateFlags1 & PLAYER_STATE1_DEAD) {
        return "Link no tiene energía";
    }
    if (Play_InCsMode(play) || CutsceneManager_GetCurrentCsId() != CS_ID_NONE) {
        return "hay una cinemática en curso";
    }
    if (IS_PAUSED(&play->pauseCtx)) {
        return "el menú de pausa está abierto";
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE || (player->stateFlags1 & PLAYER_STATE1_TALKING)) {
        return "estás en un diálogo";
    }
    if (player->rideActor != nullptr) {
        return "vas a caballo";
    }
    return "";
}

void Warp_SetRespawn(const WarpTarget& t) {
    // Spawn 0 / layer 0 of the scene always exist; the respawn data then places Link exactly.
    u16 entrance = Entrance_Create(t.entrance >> 9, 0, 0);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = entrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = t.room;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = { t.pos[0], t.pos[1], t.pos[2] };
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = t.rot;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D);
    // The respawn also restores these scene flags (a void-out's): none for a warp
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwitchFlags = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].unk_18 = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = 0;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gSaveContext.respawnFlag = -8;
}

void Warp_Go(const WarpTarget& t) {
    PlayState* play = gPlayState;
    play->nextEntrance = Entrance_Create(t.entrance >> 9, 0, 0);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_INSTANT;
    Warp_SetRespawn(t);
}

void Warp_MoveInsideRoom(const WarpTarget& t) {
    Player* player = GET_PLAYER(gPlayState);
    Vec3f pos = { t.pos[0], t.pos[1], t.pos[2] };
    player->actor.world.pos = pos;
    player->actor.prevPos = pos;
    player->actor.shape.rot.y = t.rot;
    player->actor.world.rot.y = t.rot;
    player->yaw = t.rot;
    player->actor.speed = 0.0f;
    player->actor.velocity = { 0.0f, 0.0f, 0.0f };
}

} // namespace coop::client
