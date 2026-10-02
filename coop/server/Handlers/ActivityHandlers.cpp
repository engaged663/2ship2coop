// Activities, shared dialogues and cutscenes (spec 2026-09-30-coop-grupos-actividades §3-§9). The games do the work;
// the server checks the fields and passes each event to the ones who take part:
//   act          what a player does (here / start / end / none): kept for /list and invitations; start/end to its group
//   act_hud      the HUD of the game that runs a minigame, to its group in its scene (10 times a second)
//   act_reward   a prize to copy, to its group in its scene
//   follow       a game goes to another entrance and takes along who watches it or plays with it
//   talk, cinema, title   a dialogue, a cutscene's camera, a boss's title card: to the group in the scene
//                ("scope": "group", the default) or to everyone in the scene ("scope": "scene": bosses; off with
//                server.json "bossCutscenes": false)
#include "server/Groups.h"
#include "server/Mods/ModHost.h"
#include "server/Registry.h"
#include "server/Server.h"

#include "common/PlayerState.h"
#include "common/Text.h"

#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace coop::server {

namespace {

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

bool OptionalIntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    return !ev.contains(key) || IntIn(ev, key, min, max);
}

bool OptionalBool(const json& ev, const char* key) {
    return !ev.contains(key) || ev[key].is_boolean();
}

bool FiniteVec(const json& ev, const char* key, size_t size, double limit) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_array() || it->size() != size) {
        return false;
    }
    for (const json& v : *it) {
        if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > limit) {
            return false;
        }
    }
    return true;
}

// 1..kMaxActivityKey characters of [a-z0-9_]: they name a line of the games' table (ActivityTable.cpp).
bool ValidKey(const std::string& key) {
    if (key.empty() || key.size() > (size_t)kMaxActivityKey) {
        return false;
    }
    for (char ch : key) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) {
            return false;
        }
    }
    return true;
}

// "group" (the default) or "scene"; anything else is invalid.
bool ReadScope(const json& ev, std::string& out) {
    out = GetString(ev, "scope", "group");
    return out == "group" || out == "scene";
}

// Who may send to that scope: its group needs a player; the scene also takes the server's own game (it may run the
// boss).
bool MaySend(const RemoteClient& c, const std::string& scope) {
    if (scope == "scene") {
        return c.welcomed && !c.closing && c.inWorld && c.scene >= 0;
    }
    return Groups_InWorld(c);
}

std::vector<RemoteClient*> Audience(Server& server, const RemoteClient& from, const std::string& scope) {
    std::vector<RemoteClient*> out;
    if (scope == "scene") {
        if (!server.Config().bossCutscenes) {
            return out;
        }
        for (RemoteClient* p : server.Players().Welcomed()) {
            if (p != &from && Groups_InWorld(*p) && p->scene == from.scene) {
                out.push_back(p);
            }
        }
        return out;
    }
    return Groups_MatesInScene(server, from);
}

void Relay(Server& server, const RemoteClient& from, json ev, const std::vector<RemoteClient*>& to) {
    ev["from"] = from.id;
    for (RemoteClient* p : to) {
        server.SendEvent(*p, ev);
    }
}

// The talker's values for a mirrored text (game: Features/MessageVars.cpp): an even run of hex digits.
bool ValidVars(const json& ev) {
    auto it = ev.find("vars");
    if (it == ev.end()) {
        return true;
    }
    if (!it->is_string()) {
        return false;
    }
    const std::string& s = it->get_ref<const std::string&>();
    if (s.size() > kMaxTalkVarsHex || s.size() % 2 != 0) {
        return false;
    }
    for (char c : s) {
        if (!std::isxdigit((unsigned char)c)) {
            return false;
        }
    }
    return true;
}

// The mods' "activity" event: a minigame started, ended or told its result.
void TellMods(Server& server, const RemoteClient& client, const char* state, const std::string& key,
              const std::string& name, int64_t score, int64_t cs, bool won) {
    if (server.Mods().Wants(ModEvent::Activity)) {
        json e = { { "player", client.id }, { "nick", client.nick }, { "state", state }, { "key", key },
                   { "name", name },        { "score", score },      { "cs", cs },       { "won", won } };
        server.Mods().Fire(ModEvent::Activity, e);
    }
}

void OnAct(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.activityBudget.Take(server.NowMs())) {
        return;
    }
    std::string state = GetString(ev, "state");
    std::string key = GetString(ev, "key");
    bool none = state == "none";
    bool result = state == "result";
    if ((state != "here" && state != "start" && state != "end" && !none && !result) || (!none && !ValidKey(key)) ||
        !OptionalIntIn(ev, "score", 0, 0xFFFF) || !OptionalIntIn(ev, "cs", 0, kMaxActivityCs) ||
        (result && (!ev.contains("won") || !ev["won"].is_boolean()))) {
        server.NoteInvalid(client, Tr(Msg::InvActivity));
        return;
    }
    std::string name = SanitizeChat(GetString(ev, "name"), kMaxActivityName);
    if (name.empty()) {
        name = key;
    }
    if (result) {
        // How this game's own run ended (Cada uno): the group sees it, nothing else changes
        bool won = GetBool(ev, "won");
        server.Log().Info(Tr(won ? Msg::LogActivityWon : Msg::LogActivityLost, { client.nick, name }));
        json out = MakeEvent(ev::kAct);
        out["state"] = "result";
        out["key"] = key;
        out["name"] = name;
        out["nick"] = client.nick;
        out["won"] = won;
        out["cs"] = GetInt(ev, "cs");
        Relay(server, client, out, Groups_Mates(server, client));
        TellMods(server, client, "result", key, name, 0, GetInt(ev, "cs"), won);
        return;
    }
    std::string oldKey = client.activityKey;
    std::string oldName = client.activityName;
    bool wasRunning = client.activityRunning;
    if (none) {
        client.activityKey.clear();
        client.activityName.clear();
        client.activityRunning = false;
    } else {
        if (key != client.activityKey) {
            client.activityRunning = false;
        }
        client.activityKey = key;
        client.activityName = name;
        if (state == "start") {
            client.activityRunning = true;
        } else if (state == "end") {
            client.activityRunning = false;
        }
    }
    if (client.activityRunning != wasRunning) {
        bool started = client.activityRunning;
        std::string whatKey = started || oldKey.empty() ? client.activityKey : oldKey;
        std::string whatName = started || oldName.empty() ? client.activityName : oldName;
        server.Log().Info(Tr(started ? Msg::LogActivityStart : Msg::LogActivityEnd, { client.nick, whatName }));
        json out = MakeEvent(ev::kAct);
        out["state"] = started ? "start" : "end";
        out["key"] = whatKey;
        out["name"] = whatName;
        out["nick"] = client.nick;
        if (!started) { // the round's result (Por turnos)
            out["score"] = GetInt(ev, "score");
            out["cs"] = GetInt(ev, "cs");
        }
        Relay(server, client, out, Groups_Mates(server, client));
        TellMods(server, client, started ? "start" : "end", whatKey, whatName, started ? 0 : GetInt(ev, "score"),
                 started ? 0 : GetInt(ev, "cs"), false);
    }
    // The leader's activity names its group (menus): the members hear it.
    const Group* g = server.Groups().GroupOf(client.id);
    if (g != nullptr && g->Leader() == client.id && (client.activityKey != oldKey || client.activityName != oldName)) {
        json gev = Groups_Event(server, g);
        for (uint8_t id : g->members) {
            if (RemoteClient* m = server.Players().ById(id)) {
                server.SendEvent(*m, gev);
            }
        }
    }
}

bool ValidTimer(const json& ev) {
    auto it = ev.find("timer");
    if (it == ev.end()) {
        return true;
    }
    return it->is_object() && IntIn(*it, "id", 0, 6) && IntIn(*it, "limit", 0, 36000000) &&
           IntIn(*it, "elapsed", 0, 36000000) && OptionalBool(*it, "down") && OptionalBool(*it, "run");
}

void OnActHud(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.activityBudget.Take(server.NowMs())) {
        return;
    }
    if (!ValidKey(GetString(ev, "key")) || !OptionalIntIn(ev, "score", 0, 0xFFFF) ||
        !OptionalIntIn(ev, "hidden", 0, 0xFFFF) || !OptionalIntIn(ev, "status", 0, 3) ||
        !OptionalIntIn(ev, "mstate", 0, 255) || !OptionalIntIn(ev, "perfect", 0, 16) ||
        !OptionalIntIn(ev, "ammo", 0, 0xFFFF) || !OptionalIntIn(ev, "b", -128, 127) ||
        !OptionalIntIn(ev, "bomb", -128, 127) || !OptionalIntIn(ev, "chu", -128, 127) ||
        !OptionalIntIn(ev, "sub", -1, 0xFFFF) || !OptionalIntIn(ev, "rot", -32768, 32767) ||
        !OptionalBool(ev, "arrows") || !OptionalBool(ev, "frozen") ||
        (ev.contains("pos") && !FiniteVec(ev, "pos", 3, pose_limits::kWorldLimit)) || !ValidTimer(ev)) {
        server.NoteInvalid(client, Tr(Msg::InvActHud));
        return;
    }
    Relay(server, client, ev, Groups_MatesInScene(server, client));
}

void OnActReward(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.rewardBudget.Take(server.NowMs())) {
        return;
    }
    bool gi = ev.contains("gi");
    bool rupees = ev.contains("rupees");
    if (gi == rupees || (gi && !IntIn(ev, "gi", 1, 0xFF)) || (rupees && !IntIn(ev, "rupees", 1, kMaxRewardRupees))) {
        server.NoteInvalid(client, Tr(Msg::InvReward));
        return;
    }
    std::vector<RemoteClient*> mates = Groups_MatesInScene(server, client);
    if (mates.empty()) {
        return;
    }
    json out = MakeEvent(ev::kActReward);
    std::string what;
    if (gi) {
        out["gi"] = GetInt(ev, "gi");
        what = "gi=" + std::to_string(GetInt(ev, "gi"));
    } else {
        out["rupees"] = GetInt(ev, "rupees");
        what = "rupees=" + std::to_string(GetInt(ev, "rupees"));
    }
    out["nick"] = client.nick;
    server.Log().Info(Tr(Msg::LogReward, { client.nick, what }));
    Relay(server, client, out, mates);
}

void OnFollow(Server& server, RemoteClient& client, const json& ev) {
    std::string scope;
    bool scopeOk = ReadScope(ev, scope);
    if (!MaySend(client, scopeOk ? scope : "group") || !client.activityBudget.Take(server.NowMs())) {
        return;
    }
    std::string key = GetString(ev, "key");
    if (!scopeOk || !IntIn(ev, "entrance", 0, 0xFFFF) || !IntIn(ev, "cs", 0, 0xFFFF) || !IntIn(ev, "trans", 0, 255) ||
        (!key.empty() && !ValidKey(key))) {
        server.NoteInvalid(client, Tr(Msg::InvFollow));
        return;
    }
    Relay(server, client, ev, Audience(server, client, scope));
}

void OnTalk(Server& server, RemoteClient& client, const json& ev) {
    std::string scope;
    bool scopeOk = ReadScope(ev, scope);
    if (!MaySend(client, scopeOk ? scope : "group") || !client.activityBudget.Take(server.NowMs())) {
        return;
    }
    std::string op = GetString(ev, "op");
    bool known = op == "open" || op == "id" || op == "page" || op == "choice" || op == "close";
    if (!known || !scopeOk || ((op == "open" || op == "id") && !IntIn(ev, "id", 0, 0xFFFF)) ||
        (op == "page" && !IntIn(ev, "page", 0, 1000)) || (op == "choice" && !IntIn(ev, "choice", 0, 2)) ||
        !ValidVars(ev)) {
        server.NoteInvalid(client, Tr(Msg::InvTalk));
        return;
    }
    Relay(server, client, ev, Audience(server, client, scope));
}

void OnCinema(Server& server, RemoteClient& client, const json& ev) {
    std::string scope;
    bool scopeOk = ReadScope(ev, scope);
    if (!MaySend(client, scopeOk ? scope : "group") || !client.endingBudget.Take(server.NowMs())) {
        return;
    }
    auto fov = ev.find("fov");
    if (!scopeOk || !FiniteVec(ev, "eye", 3, pose_limits::kWorldLimit) ||
        !FiniteVec(ev, "at", 3, pose_limits::kWorldLimit) || fov == ev.end() || !fov->is_number() ||
        !(fov->get<double>() >= 1.0 && fov->get<double>() <= 179.0) ||
        (ev.contains("fill") && !FiniteVec(ev, "fill", 4, 255.0)) || !OptionalIntIn(ev, "roll", -32768, 32767) ||
        !OptionalIntIn(ev, "bgm", -1, 0xFFFF) || !OptionalIntIn(ev, "blur", -1, 255)) {
        server.NoteInvalid(client, Tr(Msg::InvCamera));
        return;
    }
    client.cinemaMs = server.NowMs(); // its cutscene actors travel with it (ActorHandlers.cpp: OnActors)
    Relay(server, client, ev, Audience(server, client, scope));
}

void OnTitle(Server& server, RemoteClient& client, const json& ev) {
    std::string scope;
    bool scopeOk = ReadScope(ev, scope);
    if (!MaySend(client, scopeOk ? scope : "group") || !client.activityBudget.Take(server.NowMs())) {
        return;
    }
    std::string tex = GetString(ev, "tex");
    if (!scopeOk || tex.size() < 8 || tex.size() > 128 || tex.rfind("__OTR__", 0) != 0 ||
        !IntIn(ev, "x", -2000, 2000) || !IntIn(ev, "y", -2000, 2000) || !IntIn(ev, "w", 1, 255) ||
        !IntIn(ev, "h", 1, 255)) {
        server.NoteInvalid(client, Tr(Msg::InvTitle));
        return;
    }
    Relay(server, client, ev, Audience(server, client, scope));
}

} // namespace

COOP_SERVER_EVENT(activityAct, ev::kAct, true, OnAct);
COOP_SERVER_EVENT(activityHud, ev::kActHud, true, OnActHud);
COOP_SERVER_EVENT(activityReward, ev::kActReward, true, OnActReward);
COOP_SERVER_EVENT(activityFollow, ev::kFollow, true, OnFollow);
COOP_SERVER_EVENT(activityTalk, ev::kTalk, true, OnTalk);
COOP_SERVER_EVENT(activityCinema, ev::kCinema, true, OnCinema);
COOP_SERVER_EVENT(activityTitle, ev::kTitle, true, OnTitle);

} // namespace coop::server
