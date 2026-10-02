// "/tp <jugador>": the server answers with the target's last location ("tp"). Same scene and room:
// Link is moved directly. Otherwise the target's scene is loaded and Link appears at its exact position (Warp.h).
// A server's mod may send one to a point (players.teleport): no target then ("").
#include "Warp.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"

#include "common/Protocol.h"

extern "C" {
#include "variables.h"
}

namespace coop::client {

namespace {

void OnTp(const json& ev) {
    std::string target = GetString(ev, "target", "el jugador");
    std::string blocked = Warp_BlockedReason();
    if (!blocked.empty()) {
        Chat_Add(ChatKind::Error, "No puedes teletransportarte ahora: " + blocked + ".");
        return;
    }
    WarpTarget spot;
    if (!Warp_FromJson(ev, spot)) {
        Chat_Add(ChatKind::Error, target.empty() ? "El destino no es válido."
                                                 : "El destino de " + target + " no es válido.");
        return;
    }
    s16 scene = (s16)GetInt(ev, "scene", -1);
    if (scene == gPlayState->sceneId && spot.room == gPlayState->roomCtx.curRoom.num) {
        Warp_MoveInsideRoom(spot);
        Chat_Add(ChatKind::Ok, target.empty() ? "Te han teletransportado." : "Te has teletransportado junto a " + target + ".");
    } else {
        Warp_Go(spot);
        Chat_Add(ChatKind::Ok, target.empty() ? "Viajando..." : "Viajando hasta " + target + "...");
    }
}

} // namespace

COOP_ON_EVENT(teleportTp, ev::kTp, OnTp);

} // namespace coop::client
