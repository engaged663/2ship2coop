// Shared props (pots, grass, crates, barrels, rocks, rupees lying around) and the items they drop: what one player
// breaks, cuts or picks up is gone for everyone in the scene. The games say what went ("prop"); the server passes
// it on to the scene and remembers it while a player stays in the scene, so whoever arrives later gets the list
// ("props"). When the last player leaves the scene everything grows back, as when leaving it in the original game.
// What a prop drops is a shared item (Handlers/ItemHandlers.cpp).
// server.json: "sharedProps": false turns it off.
#include "server/Registry.h"
#include "server/Server.h"
#include "server/Stage.h"

#include "common/PlayerState.h"

#include <cmath>
#include <map>
#include <string>
#include <set>
#include <utility>

namespace coop::server {

namespace {

constexpr int kChildRetired = -3;    // v18's shared item: items are Handlers/ItemHandlers.cpp's now (invalid)
constexpr int kChildRecut = -4;      // grass that grows back was cut: never remembered
constexpr int kChildMin = -4;
constexpr int kChildMax = 15; // a grass group has at most 16 grass

std::map<int16_t, std::set<std::pair<int64_t, int>>> sGone; // stage (scene + layer, Stage.h) -> (key, child)

bool TakesProps(Server& server, const RemoteClient& c) {
    return server.Config().sharedProps && c.welcomed && !c.closing && !c.host && c.inWorld && c.hasState;
}

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

// Everyone else in the same scene and layer who plays in the world (the server's own games have no props to break).
void SendToStage(Server& server, const RemoteClient& from, const json& ev) {
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &from && TakesProps(server, *other) && SameStage(*other, from)) {
            server.SendEvent(*other, ev);
        }
    }
}

void OnProp(Server& server, RemoteClient& client, const json& ev) {
    if (!TakesProps(server, client) || !client.propBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "scene", 0, 0x7FFF) || !IntIn(ev, "key", 0, 0xFFFFFFFFll) || !IntIn(ev, "child", kChildMin, kChildMax) ||
        GetInt(ev, "child") == kChildRetired) {
        server.NoteInvalid(client, Tr(Msg::InvPropObject));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene) {
        return; // changing scene: its props are not those of where it is now
    }
    int64_t key = GetInt(ev, "key");
    int child = (int)GetInt(ev, "child");
    if (child != kChildRecut) {
        auto& gone = sGone[StageOf(client)];
        if (gone.size() >= kMaxPropsPerScene || !gone.insert({ key, child }).second) {
            return; // already gone (two players broke it at once): the others know
        }
    }
    json out = ev;
    out["from"] = client.id;
    SendToStage(server, client, out);
    server.Log().Info("Objeto roto por " + client.nick + ": escena " + std::to_string(scene) + ", clave " +
                      std::to_string(key) + ", parte " + std::to_string(child));
}

// A game that just arrived in a scene asks what is gone there.
void OnProps(Server& server, RemoteClient& client, const json& ev) {
    if (!TakesProps(server, client) || !client.propBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "scene", 0, 0x7FFF) || (ev.contains("layer") && !IntIn(ev, "layer", 0, pose_limits::kMaxLayer))) {
        server.NoteInvalid(client, Tr(Msg::InvPropRequest));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    json list = json::array();
    auto it = sGone.find(StageOf(scene, (uint8_t)GetInt(ev, "layer", 0))); // the layer it arrived in (Stage.h)
    if (it != sGone.end()) {
        for (const auto& [key, child] : it->second) {
            list.push_back(json::array({ key, child }));
        }
    }
    json out = MakeEvent(ev::kProps);
    out["scene"] = scene;
    out["list"] = std::move(list);
    server.SendEvent(client, out);
}

// A scene nobody plays in any more grows back.
void TickProps(Server& server) {
    for (auto it = sGone.begin(); it != sGone.end();) {
        bool occupied = false;
        for (RemoteClient* c : server.Players().Welcomed()) {
            occupied = occupied || (TakesProps(server, *c) && StageOf(*c) == it->first);
        }
        it = occupied ? std::next(it) : sGone.erase(it);
    }
}

} // namespace

COOP_SERVER_EVENT(propGone, ev::kProp, true, OnProp);
COOP_SERVER_EVENT(propList, ev::kProps, true, OnProps);
COOP_SERVER_ON_TICK(propTick, TickProps);

} // namespace coop::server
