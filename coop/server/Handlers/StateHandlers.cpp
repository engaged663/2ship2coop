// Pose stream relay (only to players in the same scene) and "loc" (scene changes, shown in /list).
// Both are rate limited per player (Protocol.h) and poses the game could not draw are dropped.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/PlayerState.h"
#include "common/Text.h"

namespace coop::server {

namespace {

// The oldest player in a room owns its enemies (ActorHandlers.cpp): remember when each one arrived.
void NoteRoom(Server& server, RemoteClient& client, int16_t scene, int8_t room) {
    if (scene != client.scene || room != client.room) {
        client.roomSinceMs = server.NowMs();
    }
    client.scene = scene;
    client.room = room;
}

void OnPlayerState(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    if (!client.streamBudget.Take(server.NowMs())) {
        return; // faster than any real client: drop
    }
    PlayerState state;
    if (!DecodePlayerState(data, size, state) || !SanitizePlayerState(state)) {
        server.NoteInvalid(client, "pose inválida");
        return;
    }
    NoteRoom(server, client, state.sceneId, state.roomNum);
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

void FlushLoc(Server& server, RemoteClient& client) {
    if (!client.locDirty || client.closing || !client.locBudget.Take(server.NowMs())) {
        return;
    }
    client.locDirty = false;
    json out = MakeEvent(ev::kLoc);
    out["id"] = client.id;
    out["scene"] = client.scene;
    out["sceneName"] = client.sceneName;
    server.Broadcast(out, &client);
}

void OnLoc(Server& server, RemoteClient& client, const json& ev) {
    int16_t scene = (int16_t)GetInt(ev, "scene", -1);
    std::string sceneName = SanitizeChat(GetString(ev, "sceneName"), 64);
    bool changed = scene != client.scene || sceneName != client.sceneName;
    NoteRoom(server, client, scene, (int8_t)GetInt(ev, "room", client.room));
    client.busy = GetBool(ev, "busy");
    uint16_t entrance = (uint16_t)GetInt(ev, "entrance", client.entrance);
    if ((entrance >> 9) < pose_limits::kEntranceScenes) {
        client.entrance = entrance; // used by /tp: never store an entrance the game lacks
    }
    client.sceneName = sceneName;
    bool timeStopped = GetBool(ev, "timeStopped");
    if (timeStopped != client.timeStopped) {
        client.timeStopped = timeStopped;
        server.World().UpdateClock();
    }
    client.locDirty = client.locDirty || changed;
    FlushLoc(server, client);
}

// Location changes that exceeded the budget go out as soon as it refills (only the latest one).
void FlushPendingLocs(Server& server) {
    for (RemoteClient* client : server.Players().Welcomed()) {
        FlushLoc(server, *client);
    }
}

} // namespace

COOP_SERVER_STREAM(statePose, kStreamPlayerState, OnPlayerState);
COOP_SERVER_EVENT(stateLoc, ev::kLoc, true, OnLoc);
COOP_SERVER_ON_TICK(stateLocFlush, FlushPendingLocs);

} // namespace coop::server
