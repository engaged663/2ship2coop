// Sincronización total §5.1: the loaded scene's flags, live. "sflag" goes to everyone in the same scene + layer, the
// sender too (everyone applies the changes in the server's order, so two changes at once end the same everywhere).
// The temporary words (switches 0x40-0x7F, collectibles 0x20-0x7F, the room cleared for now) are kept per scene +
// layer while somebody is there: a game that arrives ("sflags") gets them. The cycle's words belong to the shared world.
// server.json "sceneFlags": false turns it off.
#include "server/Registry.h"
#include "server/Server.h"
#include "server/Stage.h"

#include "common/SceneFlags.h"

#include <map>

namespace coop::server {

namespace {

std::map<int16_t, scene_flags::Words> sTemporary; // stage -> its temporary words

bool Takes(Server& server, const RemoteClient& c) {
    return server.Config().sceneFlags && c.welcomed && !c.closing && c.inWorld && c.hasState;
}

bool ValidScene(const json& ev) {
    auto it = ev.find("scene");
    return it != ev.end() && it->is_number_integer() && it->get<int64_t>() >= 0 && it->get<int64_t>() <= 0x7FFF;
}

void OnSceneFlag(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.sceneFlagBudget.Take(server.NowMs())) {
        return;
    }
    std::vector<scene_flags::Op> ops;
    if (!ValidScene(ev) || !scene_flags::OpsFromJson(ev, ops)) {
        server.NoteInvalid(client, Tr(Msg::InvSceneFlags));
        return;
    }
    if ((int16_t)GetInt(ev, "scene") != client.scene) {
        return; // changing scene
    }
    auto kept = sTemporary.find(StageOf(client));
    if (kept != sTemporary.end()) {
        for (const scene_flags::Op& op : ops) {
            if (scene_flags::IsTemporary(op.word)) {
                scene_flags::Apply(kept->second, op);
            }
        }
    }
    json out = MakeEvent(ev::kSceneFlag);
    out["scene"] = client.scene;
    out["ops"] = scene_flags::OpsToJson(ops);
    out["from"] = client.id;
    for (RemoteClient* other : server.Players().WelcomedAll()) {
        if (Takes(server, *other) && SameStage(*other, client)) {
            server.SendEvent(*other, out);
        }
    }
}

// A game arrived in a scene: the first one there gives its temporary flags to the scene; the others get the scene's.
void OnSceneFlags(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.sceneFlagBudget.Take(server.NowMs())) {
        return;
    }
    scene_flags::Words words{};
    if (!ValidScene(ev) || !scene_flags::WordsFromJson(ev, words)) {
        server.NoteInvalid(client, Tr(Msg::InvSceneFlags));
        return;
    }
    if ((int16_t)GetInt(ev, "scene") != client.scene) {
        return;
    }
    auto kept = sTemporary.find(StageOf(client));
    if (kept == sTemporary.end()) {
        sTemporary[StageOf(client)] = scene_flags::TemporaryOf(words);
        return;
    }
    json out = MakeEvent(ev::kSceneFlags);
    out["scene"] = client.scene;
    out["words"] = scene_flags::WordsToJson(kept->second);
    server.SendEvent(client, out);
}

// A stage nobody plays in any more starts again, as when leaving a scene in the original game.
void TickSceneFlags(Server& server) {
    for (auto it = sTemporary.begin(); it != sTemporary.end();) {
        bool occupied = false;
        for (RemoteClient* c : server.Players().WelcomedAll()) {
            occupied = occupied || (Takes(server, *c) && StageOf(*c) == it->first);
        }
        it = occupied ? std::next(it) : sTemporary.erase(it);
    }
}

} // namespace

COOP_SERVER_EVENT(sceneFlag, ev::kSceneFlag, true, OnSceneFlag);
COOP_SERVER_EVENT(sceneFlags, ev::kSceneFlags, true, OnSceneFlags);
COOP_SERVER_ON_TICK(sceneFlagsTick, TickSceneFlags);

} // namespace coop::server
