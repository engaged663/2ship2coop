// "/tp <jugador>": the server answers with the target's last location ("tp"). Same scene and room:
// Link is moved directly. Otherwise the target's scene is loaded and Link appears at its exact position
// (the respawn mechanism also used by DeveloperTools/WarpPoint.cpp).
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"

extern "C" {
#include "functions.h"
#include "variables.h"
}

using coop::client::Chat_Add;
using coop::client::ChatKind;

namespace {

// Empty string = teleport allowed; otherwise the reason shown to the player.
std::string BlockedReason() {
    if (!PoseCapture_InGameplay()) {
        return "no estás dentro de una partida";
    }
    PlayState* play = gPlayState;
    Player* player = GET_PLAYER(play);
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return "estás cambiando de escenario";
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

bool ReadPos(const coop::json& ev, Vec3f& out) {
    auto it = ev.find("pos");
    if (it == ev.end() || !it->is_array() || it->size() != 3) {
        return false;
    }
    for (const auto& v : *it) {
        if (!v.is_number()) {
            return false;
        }
    }
    out = { (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>() };
    return true;
}

void MoveInsideRoom(Player* player, const Vec3f& pos, s16 rot) {
    player->actor.world.pos = pos;
    player->actor.prevPos = pos;
    player->actor.shape.rot.y = rot;
    player->actor.world.rot.y = rot;
    player->yaw = rot;
    player->actor.speed = 0.0f;
    player->actor.velocity = { 0.0f, 0.0f, 0.0f };
}

void WarpToScene(u16 entrance, s8 room, const Vec3f& pos, s16 rot) {
    PlayState* play = gPlayState;
    u16 target = Entrance_Create(entrance >> 9, 0, entrance & 0xF);
    play->nextEntrance = target;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = target;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = room;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = pos;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = rot;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D);
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gSaveContext.respawnFlag = -8;
}

void OnTp(const coop::json& ev) {
    std::string target = coop::GetString(ev, "target", "el jugador");
    std::string blocked = BlockedReason();
    if (!blocked.empty()) {
        Chat_Add(ChatKind::Error, "No puedes teletransportarte ahora: " + blocked + ".");
        return;
    }
    Vec3f pos;
    if (!ReadPos(ev, pos)) {
        return;
    }
    s16 scene = (s16)coop::GetInt(ev, "scene", -1);
    s8 room = (s8)coop::GetInt(ev, "room", 0);
    u16 entrance = (u16)coop::GetInt(ev, "entrance", 0);
    s16 rot = (s16)coop::GetInt(ev, "rot", 0);

    if (scene == gPlayState->sceneId && room == gPlayState->roomCtx.curRoom.num) {
        MoveInsideRoom(GET_PLAYER(gPlayState), pos, rot);
        Chat_Add(ChatKind::Ok, "Te has teletransportado junto a " + target + ".");
    } else {
        WarpToScene(entrance, room, pos, rot);
        Chat_Add(ChatKind::Ok, "Viajando hasta " + target + "...");
    }
}

} // namespace

COOP_ON_EVENT(teleportTp, coop::ev::kTp, OnTp);
