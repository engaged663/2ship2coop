// [COOP] Keeps the "auth" table of the server. It only counts while the loaded scene is the one it names (the server
// sends a new one when we change scene, and scene -1 when we leave the world); the connection going forgets it.
// Reloading the same scene keeps it: the server has nothing new to say then.
#include "Authority.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/World/WorldSession.h"

#include <spdlog/spdlog.h>

#include <map>

extern "C" {
#include "z64.h"
#include "variables.h"
}

namespace coop::client {

namespace {

int16_t sScene = -1;
std::map<int8_t, uint8_t> sOwners;

void Forget() {
    sScene = -1;
    sOwners.clear();
}

void OnAuth(const json& ev) {
    std::map<int8_t, uint8_t> owners;
    auto rooms = ev.find("rooms");
    if (rooms != ev.end() && rooms->is_array()) {
        for (const json& pair : *rooms) {
            if (pair.is_array() && pair.size() == 2 && pair[0].is_number_integer() && pair[1].is_number_integer()) {
                owners[(int8_t)pair[0].get<int>()] = (uint8_t)pair[1].get<int>();
            }
        }
    }
    int16_t scene = (int16_t)GetInt(ev, "scene", -1);
    for (const auto& [room, id] : owners) {
        auto old = sOwners.find(room);
        if (scene != sScene || old == sOwners.end() || old->second != id) {
            SPDLOG_INFO("[Coop] Authority of room {} of scene {}: {}", (int)room, (int)scene,
                        id == Session_LocalId() ? std::string("me") : "player " + std::to_string(id));
        }
    }
    sScene = scene;
    sOwners = owners;
}

void OnLost(const std::string&) {
    Forget();
}

} // namespace

bool Authority_Known() {
    return gPlayState != nullptr && sScene >= 0 && sScene == gPlayState->sceneId && WorldSession_Active();
}

uint8_t Authority_Owner(int8_t room) {
    if (!Authority_Known()) {
        return 0;
    }
    auto it = sOwners.find(room);
    return it == sOwners.end() ? 0 : it->second;
}

bool Authority_IsMine(int8_t room) {
    uint8_t owner = Authority_Owner(room);
    return owner != 0 && owner == Session_LocalId();
}

bool Authority_IsRemote(int8_t room) {
    uint8_t owner = Authority_Owner(room);
    return owner != 0 && owner != Session_LocalId();
}

} // namespace coop::client

COOP_ON_EVENT(authTable, coop::ev::kAuth, coop::client::OnAuth);
COOP_ON_LOST(authLost, coop::client::OnLost);
