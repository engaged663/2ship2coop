// The end of the game, together. The games do the work (mm/2s2h/Coop/Features/Ending*.cpp); the server passes on:
// - "ending": a player got to the Clock Tower's rooftop (1), into Majora's lair (2), beat Majora (3) or started the
//   ending (4): everyone else is taken there (server.json "endingForAll": false turns this off).
// - "rooftop": the rooftop's countdown (5 minutes in the original) is the server's: the first game that starts it
//   sets it, every game shows what is left, and when it runs out the moon falls for everyone. Oath to Order (stage 2)
//   stops it.
// - "ending_sync": where each game is in the ending's cutscenes, so the ones ahead wait for the others.
// - "cinema" (Majora's death seen by everyone in the lair) now goes through Handlers/ActivityHandlers.cpp.
// - "ending_done": the ending is over: the world goes on from the Dawn of the First Day (SharedWorld::FinishGame).
#include "server/Registry.h"
#include "server/Server.h"

#include <string>

namespace coop::server {

namespace {

int64_t sRooftopDeadlineMs = 0; // 0: no countdown
int sRooftopCycle = 0;          // the world cycle it belongs to (a reset forgets it)

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

bool InWorld(const RemoteClient& c) {
    return c.welcomed && !c.closing && c.inWorld;
}

// Every player in the server's world but `except` (the server's own games never take part).
void SendToPlayers(Server& server, const json& ev, const RemoteClient* except) {
    for (RemoteClient* p : server.Players().Welcomed()) {
        if (p != except && InWorld(*p)) {
            server.SendEvent(*p, ev);
        }
    }
}

json RooftopEvent(int64_t ms) {
    json ev = MakeEvent(ev::kRooftop);
    ev["ms"] = ms;
    return ev;
}

void StopRooftop(Server& server) {
    if (sRooftopDeadlineMs == 0) {
        return;
    }
    sRooftopDeadlineMs = 0;
    server.Log().Info(Tr(Msg::RooftopStopped));
    SendToPlayers(server, RooftopEvent(-1), nullptr);
}

std::string StageText(int stage, const std::string& who) {
    switch (stage) {
        case 1:
            return Tr(Msg::EndingStage1, { who });
        case 2:
            return Tr(Msg::EndingStage2, { who });
        case 3:
            return Tr(Msg::EndingStage3, { who });
        default:
            return Tr(Msg::EndingStage4, { who });
    }
}

void OnEnding(Server& server, RemoteClient& client, const json& ev) {
    if (!InWorld(client) || !client.endingBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "stage", 1, 4)) {
        server.NoteInvalid(client, Tr(Msg::InvEnding));
        return;
    }
    int stage = (int)GetInt(ev, "stage");
    if (stage >= 2) {
        StopRooftop(server); // the giants hold the moon
    }
    if (!server.Config().endingForAll) {
        return;
    }
    std::string who = client.host ? Tr(Msg::EndingServerGame) : client.nick;
    server.Log().Info(StageText(stage, who));
    json out = MakeEvent(ev::kEnding);
    out["stage"] = stage;
    out["from"] = client.id;
    out["nick"] = client.host ? std::string() : client.nick;
    SendToPlayers(server, out, &client);
}

void OnEndingSync(Server& server, RemoteClient& client, const json& ev) {
    if (client.host || !InWorld(client) || !client.endingBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "seg", 0, 1000) || !IntIn(ev, "scene", -1, 0x7FFF) || !IntIn(ev, "cs", 0, 0xFFFF) ||
        !IntIn(ev, "frame", 0, 0xFFFF)) {
        server.NoteInvalid(client, Tr(Msg::InvEndingSync));
        return;
    }
    json out = ev;
    out["from"] = client.id;
    SendToPlayers(server, out, &client);
}

void OnEndingDone(Server& server, RemoteClient& client, const json&) {
    if (client.host || !InWorld(client)) {
        return;
    }
    server.World().FinishGame(client);
}

void OnRooftop(Server& server, RemoteClient& client, const json& ev) {
    if (client.host || !InWorld(client) || !client.endingBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "ms", 1, kRooftopMaxMs)) {
        server.NoteInvalid(client, Tr(Msg::InvCountdown));
        return;
    }
    int64_t now = server.NowMs();
    if (sRooftopDeadlineMs != 0 && sRooftopCycle == server.World().Cycle() && now < sRooftopDeadlineMs) {
        server.SendEvent(client, RooftopEvent(sRooftopDeadlineMs - now)); // already running: what is left
        return;
    }
    int64_t ms = GetInt(ev, "ms");
    sRooftopDeadlineMs = now + ms;
    sRooftopCycle = server.World().Cycle();
    server.Log().Info(Tr(Msg::RooftopStarted, { client.nick, std::to_string(ms / 1000) }));
    SendToPlayers(server, RooftopEvent(ms), nullptr);
}

// The rooftop's countdown ran out: the moon falls (a reset in between already told everyone).
void TickEnding(Server& server) {
    if (sRooftopDeadlineMs == 0) {
        return;
    }
    if (!server.World().Exists() || server.World().Cycle() != sRooftopCycle) {
        sRooftopDeadlineMs = 0;
        return;
    }
    if (server.NowMs() < sRooftopDeadlineMs) {
        return;
    }
    sRooftopDeadlineMs = 0;
    SendToPlayers(server, RooftopEvent(-1), nullptr);
    server.Log().Info(Tr(Msg::RooftopEnded));
    server.World().CrashMoon();
}

} // namespace

COOP_SERVER_EVENT(endingStage, ev::kEnding, true, OnEnding);
COOP_SERVER_EVENT(endingSync, ev::kEndingSync, true, OnEndingSync);
COOP_SERVER_EVENT(endingDone, ev::kEndingDone, true, OnEndingDone);
COOP_SERVER_EVENT(endingRooftop, ev::kRooftop, true, OnRooftop);
COOP_SERVER_ON_TICK(endingTick, TickEnding);

} // namespace coop::server
