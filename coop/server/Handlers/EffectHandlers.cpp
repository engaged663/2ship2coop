// Effects echo (spec 2026-09-30-coop-grupos-limites §8): a game sends the particles its Link, the actors it simulates
// or the cutscene it shows create; the other games of the scene create them too. The server checks the packet,
// stamps the sender and passes it to the players in its scene (server.json "effects": false turns it off).
#include "server/Registry.h"
#include "server/Server.h"

#include "common/EffectImage.h"
#include "common/PlayerState.h"
#include "common/StreamIds.h"

namespace coop::server {

namespace {

void OnEffects(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    if (!server.Config().effects || !client.welcomed || client.closing || !client.inWorld || !client.hasState ||
        !client.effectBudget.Take(server.NowMs())) {
        return;
    }
    EffectPacket p;
    if (size > effect_limits::kPacketBytes || !DecodeEffects(data, size, p)) {
        server.NoteInvalid(client, Tr(Msg::InvEffects));
        return;
    }
    if (p.scene != client.scene) {
        return; // changing scene
    }
    StampPlayerId(data, size, client.id);
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &client && other->inWorld && !other->closing && other->scene == client.scene) {
            server.SendStream(*other, data, size);
        }
    }
}

} // namespace

COOP_SERVER_STREAM(effectStream, kStreamEffects, OnEffects);

} // namespace coop::server
