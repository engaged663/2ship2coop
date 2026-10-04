// Sincronización total §3: the music orders and quakes of the actors a game simulates go to the other players of its
// scene + layer, who play them if they are in that room (the games check the room). server.json "ambient": false
// turns it off. The server's own games are never sent anything: they have no sound nor camera to shake.
#include "server/Registry.h"
#include "server/Server.h"
#include "server/Stage.h"

#include "common/ActorImage.h"
#include "common/Ambient.h"

namespace coop::server {

namespace {

bool Takes(Server& server, const RemoteClient& c) {
    return server.Config().ambient && c.welcomed && !c.closing && c.inWorld && c.hasState;
}

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    return it != ev.end() && it->is_number_integer() && it->get<int64_t>() >= min && it->get<int64_t>() <= max;
}

void OnAmbient(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.ambientBudget.Take(server.NowMs())) {
        return;
    }
    std::vector<ambient::Music> music;
    std::vector<ambient::Quake> quakes;
    if (!IntIn(ev, "scene", 0, 0x7FFF) || !IntIn(ev, "room", 0, image_limits::kRoomMax) ||
        !ambient::MusicFromJson(ev, music) || !ambient::QuakesFromJson(ev, quakes) ||
        (music.empty() && quakes.empty())) {
        server.NoteInvalid(client, Tr(Msg::InvAmbient));
        return;
    }
    if ((int16_t)GetInt(ev, "scene") != client.scene) {
        return;
    }
    json out = MakeEvent(ev::kAmbient);
    out["scene"] = client.scene;
    out["room"] = GetInt(ev, "room");
    out["music"] = ambient::MusicToJson(music);
    out["quake"] = ambient::QuakesToJson(quakes);
    out["from"] = client.id;
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &client && Takes(server, *other) && SameStage(*other, client)) {
            server.SendEvent(*other, out);
        }
    }
}

} // namespace

COOP_SERVER_EVENT(ambientEcho, ev::kAmbient, true, OnAmbient);

} // namespace coop::server
