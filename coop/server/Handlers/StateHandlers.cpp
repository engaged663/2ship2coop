// Pose stream relay (only to players in the same scene) and "loc" (scene changes, shown in /list).
#include "server/Registry.h"
#include "server/Server.h"

#include "common/PlayerState.h"
#include "common/Text.h"

namespace coop::server {

namespace {

void OnPlayerState(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    PlayerState state;
    if (!DecodePlayerState(data, size, state)) {
        return; // malformed or from another protocol version
    }
    client.scene = state.sceneId;
    client.room = state.roomNum;
    client.entrance = state.entrance;
    client.pos[0] = state.pos[0];
    client.pos[1] = state.pos[1];
    client.pos[2] = state.pos[2];
    client.rotY = state.rot.y;
    client.hasState = true;
    client.streamsIn++;

    StampPlayerId(data, size, client.id);
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &client && !other->closing && other->scene == state.sceneId) {
            server.SendStream(*other, data, size);
            client.streamsRelayed++;
        }
    }
}

void OnLoc(Server& server, RemoteClient& client, const json& ev) {
    int16_t scene = (int16_t)GetInt(ev, "scene", -1);
    std::string sceneName = SanitizeChat(GetString(ev, "sceneName"), 64);
    bool changed = scene != client.scene || sceneName != client.sceneName;
    client.scene = scene;
    client.room = (int8_t)GetInt(ev, "room", client.room);
    client.entrance = (uint16_t)GetInt(ev, "entrance", client.entrance);
    client.sceneName = sceneName;
    if (changed) {
        json out = MakeEvent(ev::kLoc);
        out["id"] = client.id;
        out["scene"] = client.scene;
        out["sceneName"] = client.sceneName;
        server.Broadcast(out, &client);
    }
}

} // namespace

COOP_SERVER_STREAM(statePose, kStreamPlayerState, OnPlayerState);
COOP_SERVER_EVENT(stateLoc, ev::kLoc, true, OnLoc);

} // namespace coop::server
