// [COOP] Drops compartidos (docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md): what falls in one game
// is one item for its whole stage (scene + layer). "item": a game announces what fell (kept, passed on); "item_take":
// the first one who asks gets it ("item_take" ok to them, "item_gone" to the rest); "item_rest": where it came to lie;
// "items": whoever arrives gets what lies there and the keys taken that a game could make again. A stage nobody plays in
// is forgotten. The server's own games (hosts) drop what their enemies drop but take nothing. server.json "drops": false
// turns it off (each game keeps its drops to itself).
#include "server/ItemBook.h"
#include "server/Registry.h"
#include "server/Server.h"
#include "server/Stage.h"

#include "common/PlayerState.h"

#include <cmath>
#include <string>
#include <utility>

namespace coop::server {

namespace {

ItemBook sBook;

bool Plays(Server& server, const RemoteClient& c) {
    return server.Config().drops && c.welcomed && !c.closing && c.inWorld && c.hasState;
}

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

bool ReadPos(const json& ev, float out[3]) {
    auto it = ev.find("pos");
    if (it == ev.end() || !it->is_array() || it->size() != 3) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        const json& v = (*it)[i];
        if (!v.is_number() || !std::isfinite(v.get<double>()) ||
            std::fabs(v.get<double>()) > pose_limits::kWorldLimit) {
            return false;
        }
        out[i] = (float)v.get<double>();
    }
    return true;
}

// The players of its stage but itself (the hosts need no items).
template <class Fn> void ForStageMates(Server& server, const RemoteClient& from, Fn fn) {
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &from && Plays(server, *other) && SameStage(*other, from)) {
            fn(*other);
        }
    }
}

void OnItem(Server& server, RemoteClient& client, const json& ev) {
    if (!Plays(server, client) || !client.itemBudget.Take(server.NowMs())) {
        return;
    }
    ItemState item;
    if (!IntIn(ev, "scene", 0, 0x7FFF) || !ReadItemState(ev, item) ||
        (ItemKeyKindOf(item.key) == ItemKeyKind::Unique && UniqueItemKeyPlayer(item.key) != client.id)) {
        server.NoteInvalid(client, Tr(Msg::InvDrop));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene || !sBook.Add(StageOf(client), item, client.id, server.NowMs())) {
        return; // changing scene; or a twin another game announced first, or taken already
    }
    json out = MakeEvent(ev::kItem);
    out["scene"] = scene;
    WriteItemState(item, out);
    out["from"] = client.id;
    ForStageMates(server, client, [&](RemoteClient& other) { server.SendEvent(other, out); });
    server.Log().Info(Tr(Msg::LogItemDropped, { client.nick, std::to_string(scene), std::to_string(item.key) }));
}

void OnTake(Server& server, RemoteClient& client, const json& ev) {
    if (!Plays(server, client) || client.host || !client.itemBudget.Take(server.NowMs())) {
        return; // the server's own games never take anything
    }
    auto after = ev.find("after");
    if (!IntIn(ev, "scene", 0, 0x7FFF) || !IntIn(ev, "key", 0, 0xFFFFFFFFll) ||
        (after != ev.end() && !after->is_boolean())) {
        server.NoteInvalid(client, Tr(Msg::InvDrop));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    uint32_t key = (uint32_t)GetInt(ev, "key");
    if (scene != client.scene) {
        return; // its game is somewhere else now: that item stays for the ones in the scene
    }
    bool yours = sBook.Take(StageOf(client), key) == TakeResult::Yours;
    if (after == ev.end() || !after->get<bool>()) {
        json reply = MakeEvent(ev::kItemTake);
        reply["scene"] = scene;
        reply["key"] = key;
        reply["ok"] = yours;
        server.SendEvent(client, reply);
    }
    if (!yours) {
        return;
    }
    json gone = MakeEvent(ev::kItemGone);
    gone["scene"] = scene;
    gone["key"] = key;
    gone["from"] = client.id;
    for (RemoteClient* other : server.Players().WelcomedAll()) { // the hosts remove theirs too
        if (other != &client && !other->closing && other->inWorld && SameStage(*other, client)) {
            server.SendEvent(*other, gone);
        }
    }
    server.Log().Info(Tr(Msg::LogItemTaken, { client.nick, std::to_string(scene), std::to_string(key) }));
}

void OnRest(Server& server, RemoteClient& client, const json& ev) {
    if (!Plays(server, client) || !client.itemBudget.Take(server.NowMs())) {
        return;
    }
    float pos[3];
    if (!IntIn(ev, "scene", 0, 0x7FFF) || !IntIn(ev, "key", 0, 0xFFFFFFFFll) || !ReadPos(ev, pos)) {
        server.NoteInvalid(client, Tr(Msg::InvDrop));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    uint32_t key = (uint32_t)GetInt(ev, "key");
    if (scene != client.scene || !sBook.Rest(StageOf(client), key, client.id, pos)) {
        return; // not the one who dropped it, or gone
    }
    json out = MakeEvent(ev::kItemRest);
    out["scene"] = scene;
    out["key"] = key;
    out["pos"] = { pos[0], pos[1], pos[2] };
    out["from"] = client.id;
    ForStageMates(server, client, [&](RemoteClient& other) { server.SendEvent(other, out); });
}

// A game that just arrived in a scene asks what lies there (the layer it arrived in).
void OnItems(Server& server, RemoteClient& client, const json& ev) {
    if (!Plays(server, client) || client.host || !client.itemBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "scene", 0, 0x7FFF) || (ev.contains("layer") && !IntIn(ev, "layer", 0, pose_limits::kMaxLayer))) {
        server.NoteInvalid(client, Tr(Msg::InvDrop));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    int16_t stage = StageOf(scene, (uint8_t)GetInt(ev, "layer", 0));
    int64_t now = server.NowMs();
    json list = json::array();
    for (const LiveItem* it : sBook.Live(stage)) {
        json e = json::object();
        WriteItemState(it->state, e);
        e["age"] = now - it->bornMs;
        e["rest"] = it->rested;
        list.push_back(std::move(e));
    }
    json out = MakeEvent(ev::kItems);
    out["scene"] = scene;
    out["list"] = std::move(list);
    out["taken"] = sBook.Taken(stage);
    server.SendEvent(client, out);
}

void TickItems(Server& server) {
    sBook.Expire(server.NowMs());
    for (int16_t stage : sBook.Stages()) {
        bool occupied = false;
        for (RemoteClient* c : server.Players().Welcomed()) {
            occupied = occupied || (Plays(server, *c) && StageOf(*c) == stage);
        }
        if (!occupied) {
            sBook.Forget(stage); // as leaving a scene in the original: what lay there is gone
        }
    }
}

} // namespace

COOP_SERVER_EVENT(itemNew, ev::kItem, true, OnItem);
COOP_SERVER_EVENT(itemTake, ev::kItemTake, true, OnTake);
COOP_SERVER_EVENT(itemRest, ev::kItemRest, true, OnRest);
COOP_SERVER_EVENT(itemList, ev::kItems, true, OnItems);
COOP_SERVER_ON_TICK(itemTick, TickItems);

} // namespace coop::server
