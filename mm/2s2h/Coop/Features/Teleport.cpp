// "/tp <jugador>": the server answers with the target's last location ("tp"). Same scene and room:
// Link is moved directly. Otherwise the target's scene is loaded and Link appears at its exact position (Warp.h).
// A server's mod may send one to a point (players.teleport): no target then ("").
// An activity room's trip ("roomTrip": a member who got ready goes to its director) waits while this game cannot
// travel (a text, a cutscene, on horseback...), 30 s at most.
#include "Warp.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kRoomTripMs = 30000;

struct PendingTrip {
    bool active = false;
    WarpTarget spot;
    s16 scene = -1;
    std::string target;
    int64_t untilMs = 0;
};
PendingTrip sPendingTrip;

void Travel(const WarpTarget& spot, s16 scene, const std::string& target) {
    if (scene == gPlayState->sceneId && spot.room == gPlayState->roomCtx.curRoom.num) {
        Warp_MoveInsideRoom(spot);
        Chat_Add(ChatKind::Ok, target.empty() ? "Te han teletransportado." : "Te has teletransportado junto a " + target + ".");
    } else {
        Warp_Go(spot);
        Chat_Add(ChatKind::Ok, target.empty() ? "Viajando..." : "Viajando hasta " + target + "...");
    }
}

void OnTp(const json& ev) {
    std::string target = GetString(ev, "target", "el jugador");
    WarpTarget spot;
    if (!Warp_FromJson(ev, spot)) {
        Chat_Add(ChatKind::Error, target.empty() ? "El destino no es válido."
                                                 : "El destino de " + target + " no es válido.");
        return;
    }
    s16 scene = (s16)GetInt(ev, "scene", -1);
    std::string blocked = Warp_BlockedReason();
    if (!blocked.empty()) {
        if (GetBool(ev, "roomTrip")) {
            sPendingTrip = PendingTrip{ true, spot, scene, target, Group_NowMs() + kRoomTripMs };
            Chat_Add(ChatKind::Info, "Irás junto a " + target + " en cuanto puedas (" + blocked + ").");
            return;
        }
        Chat_Add(ChatKind::Error, "No puedes teletransportarte ahora: " + blocked + ".");
        return;
    }
    sPendingTrip.active = false;
    Travel(spot, scene, target);
}

// A room's trip that had to wait goes as soon as this game can travel.
void FrameEnd() {
    if (!sPendingTrip.active) {
        return;
    }
    if (!WorldSession_Active() || Group_NowMs() > sPendingTrip.untilMs) {
        sPendingTrip.active = false;
        return;
    }
    if (Warp_BlockedReason().empty()) {
        sPendingTrip.active = false;
        Travel(sPendingTrip.spot, sPendingTrip.scene, sPendingTrip.target);
    }
}

void RegisterTeleport() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

} // namespace coop::client

COOP_ON_EVENT(teleportTp, coop::ev::kTp, coop::client::OnTp);
COOP_ON_LOST(teleportLost, [](const std::string&) { coop::client::sPendingTrip.active = false; });
static RegisterShipInitFunc sTeleportInit(coop::client::RegisterTeleport);
