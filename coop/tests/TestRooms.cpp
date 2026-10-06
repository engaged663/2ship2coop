// Activity rooms through a real server (spec 2026-10-04-coop-salas-actividades §3-§4): server.json, the room_op
// actions, the events and texts, and the activity relays that go by room membership.
#include "TestWorld.h"

#include <cstdio>
#include <fstream>

using namespace coop;
using namespace coop_test;

TEST_CASE(ConfigReadsRoomsOption) {
    const std::string path = "test_server_rooms.json";
    {
        std::ofstream f(path);
        f << "{\"rooms\":false}";
    }
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(!cfg.rooms);
    CHECK(warn.empty());
    std::remove(path.c_str());
    server::ServerConfig def;
    CHECK(def.rooms);
    CHECK_EQ(def.roomCountdownMs, (int64_t)kRoomCountdownMs);
    CHECK_EQ(def.roomLobbyMs, (int64_t)kRoomLobbyMs);
    CHECK_EQ(def.roomLingerMs, (int64_t)kRoomLingerMs);
}

namespace {

constexpr int16_t kScene = 0x40;
constexpr int16_t kOther = 0x41;

server::ServerConfig RoomConfig() {
    server::ServerConfig cfg;
    cfg.roomCountdownMs = 50;
    return cfg;
}

// Alice and Bob in a group in kScene (Alice at 10,0,20); Carol, in no group, in kOther. All in the world.
struct Lobby {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c;
    explicit Lobby(server::ServerConfig cfg = RoomConfig()) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        a->SendState(kScene, 0, 0x1234, 10.f, 0.f, 20.f);
        b->SendState(kScene, 0);
        c->SendState(kOther, 0);
        s.PumpFor(80);
        if (cfg.groups) {
            a->Cmd("/invitar Bob");
            if (!b->WaitFor("invite", s).has_value()) {
                Fail(__FILE__, __LINE__, "no invite");
            }
            b->Cmd("/aceptar");
            if (!b->WaitFor("group", s).has_value()) {
                Fail(__FILE__, __LINE__, "no group");
            }
        }
        Drain(s, { a.get(), b.get(), c.get() });
    }
};

json Op(const char* op) {
    return { { "t", "room_op" }, { "op", op } };
}

json OpenOp(const char* key = "galeria_ciudad", const char* place = "here") {
    json o = Op("open");
    o["key"] = key;
    o["name"] = "Galería de tiro";
    o["mode"] = "together";
    o["place"] = place;
    return o;
}

json ReadyOp(bool ready = true) {
    json o = Op("ready");
    o["ready"] = ready;
    return o;
}

// The next "room" event of c that matches pred (older ones are dropped).
template <class Pred>
std::optional<json> WaitRoom(TestServer& s, TestClient& c, Pred pred, int timeoutMs = 1500) {
    std::optional<json> found;
    c.WaitUntil(s, timeoutMs, [&] {
        while (auto ev = c.TakeEvent("room")) {
            if (pred(*ev)) {
                found = ev;
                return true;
            }
        }
        return false;
    });
    return found;
}

bool InRoom(const json& r) {
    return GetInt(r, "id") != 0;
}

// Alice opens a room (Bob comes along with the group); everything that arrives is dropped.
void OpenAndDrain(Lobby& l, const char* place = "here", const char* key = "galeria_ciudad") {
    l.a->Send(OpenOp(key, place));
    if (!WaitRoom(l.s, *l.b, InRoom).has_value()) {
        Fail(__FILE__, __LINE__, "no room for Bob");
    }
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
}

// Alice invites Carol, who accepts.
void BringCarol(Lobby& l) {
    json inv = Op("invite");
    inv["to"] = l.c->id;
    l.a->Send(inv);
    auto ri = l.c->WaitFor("room_invite", l.s);
    if (!ri.has_value()) {
        Fail(__FILE__, __LINE__, "no room_invite");
    }
    json ans = Op("answer");
    ans["room"] = GetInt(*ri, "room");
    ans["accept"] = true;
    l.c->Send(ans);
    if (!WaitRoom(l.s, *l.c, [](const json& r) { return r["members"].size() == 3; }).has_value()) {
        Fail(__FILE__, __LINE__, "Carol not in the room");
    }
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
}

} // namespace

TEST_CASE(RoomOpenTakesTheGroupAlong) {
    Lobby l;
    l.a->Send(OpenOp());
    auto ra = WaitRoom(l.s, *l.a, InRoom);
    auto rb = WaitRoom(l.s, *l.b, InRoom);
    CHECK(ra.has_value() && rb.has_value());
    CHECK_EQ(GetString(*rb, "state"), std::string("lobby"));
    CHECK_EQ((*rb)["members"].size(), (size_t)2);
    CHECK_EQ(GetString((*rb)["members"][1], "nick"), std::string("Bob"));
    CHECK(!GetBool((*rb)["members"][1], "ready"));
    CHECK_EQ(GetInt(*rb, "host"), (int64_t)l.a->id);
    CHECK_EQ(GetInt(*rb, "director"), (int64_t)l.a->id);
    CHECK_EQ(GetString(*rb, "key"), std::string("galeria_ciudad"));
    CHECK_EQ(GetString(*rb, "name"), std::string("Galería de tiro"));
    CHECK_EQ(GetString(*rb, "place"), std::string("here"));
    CHECK_EQ(GetString(*rb, "mode"), std::string("together"));
    CHECK(GetInt(*rb, "ms") > 0); // until the lobby gives up
    const json& set = (*rb)["settings"];
    CHECK(GetBool(set, "travel") && GetBool(set, "rewards") && !GetBool(set, "open"));
    CHECK(l.b->WaitForSys("info", l.s)); // "Alice empieza Galería de tiro: estás en su sala..."
    CHECK(!l.c->WaitFor("room", l.s, 150).has_value());
}

TEST_CASE(RoomInviteAcceptReadyTravelAndStart) {
    Lobby l;
    OpenAndDrain(l);
    json inv = Op("invite");
    inv["to"] = l.c->id;
    l.a->Send(inv);
    auto ri = l.c->WaitFor("room_invite", l.s);
    CHECK(ri.has_value());
    CHECK_EQ(GetString(*ri, "nick"), std::string("Alice"));
    CHECK_EQ(GetInt(*ri, "from"), (int64_t)l.a->id);
    CHECK_EQ(GetString(*ri, "name"), std::string("Galería de tiro"));
    CHECK(GetInt(*ri, "ms") > 0);
    CHECK(WaitRoom(l.s, *l.a, [](const json& r) { return r["invites"].size() == 1; }).has_value());
    json ans = Op("answer");
    ans["room"] = GetInt(*ri, "room");
    ans["accept"] = true;
    l.c->Send(ans);
    CHECK(WaitRoom(l.s, *l.c, [](const json& r) { return r["members"].size() == 3; }).has_value());
    auto end = l.c->WaitFor("room_invite_end", l.s);
    CHECK(end.has_value() && GetString(*end, "reason") == "accepted");
    // Carol is in another scene: being ready takes her to Alice, and the room waits until she is there
    l.a->Send(ReadyOp());
    l.b->Send(ReadyOp());
    l.c->Send(ReadyOp());
    auto tp = l.c->WaitFor("tp", l.s);
    CHECK(tp.has_value());
    CHECK_EQ(GetInt(*tp, "scene"), (int64_t)kScene);
    CHECK_EQ((*tp)["pos"][0].get<double>(), 10.0);
    CHECK(GetBool(*tp, "roomTrip"));
    CHECK(!l.b->WaitFor("tp", l.s, 100).has_value()); // already there
    auto started = [](const json& r) {
        std::string st = GetString(r, "state");
        return st == "starting" || st == "running";
    };
    CHECK(!WaitRoom(l.s, *l.a, started, 250).has_value());
    l.c->SendState(kScene, 0);
    CHECK(WaitRoom(l.s, *l.a, [](const json& r) { return GetString(r, "state") == "running"; }).has_value());
    CHECK(WaitRoom(l.s, *l.c, [](const json& r) { return GetString(r, "state") == "running"; }).has_value());
}

TEST_CASE(RoomChatReachesOnlyTheRoom) {
    Lobby l;
    OpenAndDrain(l);
    json chat = Op("chat");
    chat["text"] = "voy!";
    l.b->Send(chat);
    auto ma = l.a->WaitFor("room_chat", l.s);
    CHECK(ma.has_value());
    CHECK_EQ(GetString(*ma, "text"), std::string("voy!"));
    CHECK_EQ(GetString(*ma, "nick"), std::string("Bob"));
    CHECK_EQ(GetInt(*ma, "from"), (int64_t)l.b->id);
    CHECK(l.b->WaitFor("room_chat", l.s).has_value()); // the sender sees its line once the server took it
    CHECK(!l.c->WaitFor("room_chat", l.s, 150).has_value());
    json empty = Op("chat");
    empty["text"] = "   ";
    l.b->Send(empty);
    CHECK(!l.a->WaitFor("room_chat", l.s, 150).has_value());
    json lone = Op("chat");
    lone["text"] = "hola";
    l.c->Send(lone);
    CHECK(l.c->WaitForSys("error", l.s)); // "No estás en ninguna sala."
}

TEST_CASE(RoomOpenRoomsAreListedAndJoinable) {
    Lobby l;
    OpenAndDrain(l);
    json set = Op("settings");
    set["open"] = true;
    l.a->Send(set);
    std::optional<json> list;
    l.c->WaitUntil(l.s, 1500, [&] {
        while (auto ev = l.c->TakeEvent("rooms")) {
            if ((*ev)["list"].size() == 1) {
                list = ev;
                return true;
            }
        }
        return false;
    });
    CHECK(list.has_value());
    const json& entry = (*list)["list"][0];
    CHECK_EQ(GetString(entry, "nick"), std::string("Alice"));
    CHECK_EQ(GetString(entry, "key"), std::string("galeria_ciudad"));
    CHECK_EQ(GetInt(entry, "count"), (int64_t)2);
    CHECK_EQ(GetString(entry, "state"), std::string("lobby"));
    json join = Op("join");
    join["room"] = GetInt(entry, "id");
    l.c->Send(join);
    CHECK(WaitRoom(l.s, *l.c, [](const json& r) { return r["members"].size() == 3; }).has_value());
    json bad = Op("settings");
    bad["open"] = false;
    l.b->Send(bad); // only the host changes them
    CHECK(l.b->WaitForSys("error", l.s));
}

TEST_CASE(RoomsOffAnswersOpenWithNoRoom) {
    server::ServerConfig cfg = RoomConfig();
    cfg.rooms = false;
    Lobby l(cfg);
    l.a->Send(OpenOp());
    auto r = l.a->WaitFor("room", l.s);
    CHECK(r.has_value() && GetInt(*r, "id") == 0 && GetString(*r, "reason") == "off");
    CHECK(!l.b->WaitFor("room", l.s, 150).has_value());
    l.a->Send(ReadyOp());
    CHECK(l.a->WaitForSys("error", l.s)); // "Las salas están desactivadas en este servidor."
}

TEST_CASE(RoomOpsAreChecked) {
    Lobby l;
    l.a->Send(Op("dance"));
    l.a->Send(OpenOp("Bad Key!"));
    l.a->Send(OpenOp("galeria_ciudad", "moon"));
    json mode = OpenOp();
    mode["mode"] = "solo";
    l.a->Send(mode);
    json notBool = Op("ready");
    notBool["ready"] = 1;
    l.a->Send(notBool);
    json noTarget = Op("invite");
    l.a->Send(noTarget);
    CHECK(!l.a->WaitFor("room", l.s, 200).has_value());
    CHECK(!l.b->WaitFor("room", l.s, 50).has_value());
    // A member who is not the host cannot invite or remove anyone
    OpenAndDrain(l);
    json inv = Op("invite");
    inv["to"] = l.c->id;
    l.b->Send(inv);
    CHECK(l.b->WaitForSys("error", l.s)); // "Solo el anfitrión de la sala puede hacer eso."
    CHECK(!l.c->WaitFor("room_invite", l.s, 150).has_value());
    json kick = Op("kick");
    kick["who"] = l.a->id;
    l.b->Send(kick);
    CHECK(l.b->WaitForSys("error", l.s));
}

TEST_CASE(RoomInvitationsAreRateLimitedLikeTheGroups) {
    Lobby l;
    OpenAndDrain(l);
    json inv = Op("invite");
    inv["to"] = l.c->id;
    for (int i = 0; i < kInviteBurst + 5; i++) {
        l.a->Send(inv); // renewing the same invitation again and again
    }
    l.s.PumpFor(150);
    int got = 0;
    while (l.c->TakeEvent("room_invite").has_value()) {
        got++;
    }
    CHECK(got >= 1 && got <= kInviteBurst);
    CHECK(l.a->WaitForSys("warn", l.s)); // "Estás enviando mensajes demasiado rápido..."
}

TEST_CASE(RoomDirectorDisconnectingEndsTheRound) {
    Lobby l;
    OpenAndDrain(l);
    l.a->Close();
    auto r = WaitRoom(l.s, *l.b, [&](const json& ev) { return GetInt(ev, "host") == l.b->id; }, 3000);
    CHECK(r.has_value());
    CHECK_EQ(GetString(*r, "state"), std::string("ended")); // Alice's game ran the round: nobody waits for her
    CHECK_EQ((*r)["members"].size(), (size_t)1);
}

TEST_CASE(RoomKickAndCloseTellTheMembers) {
    Lobby l;
    OpenAndDrain(l);
    BringCarol(l);
    json kick = Op("kick");
    kick["who"] = l.c->id;
    l.a->Send(kick);
    auto rc = WaitRoom(l.s, *l.c, [](const json& r) { return !InRoom(r); });
    CHECK(rc.has_value() && GetString(*rc, "reason") == "kicked");
    CHECK(l.c->WaitForSys("warn", l.s)); // "Alice te ha quitado de la sala."
    CHECK(WaitRoom(l.s, *l.b, [](const json& r) { return r["members"].size() == 2; }).has_value());
    l.a->Send(Op("close"));
    auto rb = WaitRoom(l.s, *l.b, [](const json& r) { return !InRoom(r); });
    CHECK(rb.has_value() && GetString(*rb, "reason") == "closed");
}

namespace {

bool Running(const json& r) {
    return GetString(r, "state") == "running";
}

json Act(const char* state, const char* key) {
    return { { "t", "act" }, { "state", state }, { "key", key }, { "name", "Actividad" } };
}

} // namespace

TEST_CASE(RoomShareActivityAcrossScenes) {
    Lobby l;
    OpenAndDrain(l, "anywhere", "bombers");
    l.a->Send(ReadyOp());
    l.b->Send(ReadyOp());
    CHECK(WaitRoom(l.s, *l.b, Running).has_value());
    l.b->SendState(kOther, 0); // Bob looks for Bombers somewhere else
    l.s.PumpFor(60);
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    json hud = { { "t", "act_hud" }, { "key", "bombers" }, { "score", 3 } };
    l.a->Send(hud);
    auto h = l.b->WaitFor("act_hud", l.s);
    CHECK(h.has_value() && GetInt(*h, "score") == 3 && GetInt(*h, "from") == l.a->id);
    CHECK(!l.c->WaitFor("act_hud", l.s, 150).has_value());
    l.a->Send({ { "t", "act_reward" }, { "rupees", 20 }, { "room", true } });
    auto r = l.b->WaitFor("act_reward", l.s);
    CHECK(r.has_value() && GetInt(*r, "rupees") == 20 && GetBool(*r, "room"));
    l.a->Send({ { "t", "act_reward" }, { "rupees", 20 } }); // not the room's: the group of the scene, as before
    CHECK(!l.b->WaitFor("act_reward", l.s, 150).has_value());
    json follow = { { "t", "follow" }, { "entrance", 0xD660 }, { "cs", 0 }, { "trans", 2 },
                    { "key", "bombers" }, { "all", true } };
    l.a->Send(follow);
    auto f = l.b->WaitFor("follow", l.s);
    CHECK(f.has_value() && GetBool(*f, "all") && GetInt(*f, "entrance") == 0xD660);
    json other = follow;
    other["key"] = "galeria_ciudad"; // not the room's activity, and Bob is in another scene
    l.a->Send(other);
    CHECK(!l.b->WaitFor("follow", l.s, 150).has_value());
    json bad = follow;
    bad["all"] = 1;
    l.a->Send(bad);
    CHECK(!l.b->WaitFor("follow", l.s, 150).has_value());
    json badPrize = { { "t", "act_reward" }, { "rupees", 20 }, { "room", "yes" } };
    l.a->Send(badPrize);
    CHECK(!l.b->WaitFor("act_reward", l.s, 150).has_value());
    // The host stops sharing the prizes
    json set = Op("settings");
    set["rewards"] = false;
    l.a->Send(set);
    CHECK(WaitRoom(l.s, *l.b, [](const json& rr) { return !GetBool(rr["settings"], "rewards", true); }).has_value());
    l.a->Send({ { "t", "act_reward" }, { "rupees", 20 }, { "room", true } });
    CHECK(!l.b->WaitFor("act_reward", l.s, 150).has_value());
}

TEST_CASE(RoomMatesInTheSceneSeeDialoguesAndCutscenes) {
    Lobby l;
    OpenAndDrain(l);
    BringCarol(l); // Carol is in no group: only the room makes her Alice's mate
    l.c->SendState(kScene, 0);
    l.s.PumpFor(60);
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 0x3E8 } });
    auto t = l.c->WaitFor("talk", l.s);
    CHECK(t.has_value() && GetInt(*t, "from") == l.a->id);
    json cam = { { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 } };
    l.a->Send(cam);
    CHECK(l.c->WaitFor("cinema", l.s).has_value());
    // Dialogues and cutscenes are for who is there
    l.c->SendState(kOther, 0);
    l.s.PumpFor(60);
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 0x3E9 } });
    CHECK(l.b->WaitFor("talk", l.s).has_value());
    CHECK(!l.c->TakeEvent("talk").has_value());
}

TEST_CASE(RoomEndOfRoundAndAnotherRound) {
    Lobby l;
    OpenAndDrain(l, "here", "cartero");
    l.a->Send(ReadyOp());
    l.b->Send(ReadyOp());
    CHECK(WaitRoom(l.s, *l.b, Running).has_value());
    l.a->Send(Act("start", "cartero"));
    json end = Act("end", "cartero");
    end["score"] = 25;
    end["cs"] = 1000;
    l.a->Send(end);
    auto r = WaitRoom(l.s, *l.b, [](const json& rr) { return GetString(rr, "state") == "ended"; });
    CHECK(r.has_value());
    CHECK_EQ(GetInt((*r)["members"][0], "score"), (int64_t)25);
    CHECK_EQ(GetInt((*r)["members"][0], "cs"), (int64_t)1000);
    CHECK(GetInt(*r, "ms") > 0); // until it closes
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    // Bob plays his round (Por turnos): the same room, directed by him, everyone confirms again
    json open = OpenOp("cartero");
    open["mode"] = "turns";
    l.b->Send(open);
    auto re = WaitRoom(l.s, *l.a, [](const json& rr) { return GetString(rr, "state") == "lobby"; });
    CHECK(re.has_value());
    CHECK_EQ(GetInt(*re, "director"), (int64_t)l.b->id);
    CHECK_EQ(GetInt(*re, "host"), (int64_t)l.a->id);
    CHECK_EQ((*re)["members"].size(), (size_t)2);
    CHECK(!GetBool((*re)["members"][0], "ready"));
}

TEST_CASE(RoomLeaversGetNoActivityRelays) {
    Lobby l;
    OpenAndDrain(l);
    l.b->Send(Op("leave")); // Bob does not take part (he stays in Alice's group)
    CHECK(WaitRoom(l.s, *l.b, [](const json& r) { return !InRoom(r); }).has_value());
    l.a->Send(ReadyOp()); // alone: it runs at once
    CHECK(WaitRoom(l.s, *l.a, Running).has_value());
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.a->Send({ { "t", "act_hud" }, { "key", "galeria_ciudad" }, { "score", 3 } });
    CHECK(!l.b->WaitFor("act_hud", l.s, 150).has_value());
    l.a->Send({ { "t", "act_reward" }, { "gi", 5 }, { "room", true } });
    CHECK(!l.b->WaitFor("act_reward", l.s, 150).has_value());
    // ...but he still hears her dialogues next to her, as her group mate
    l.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 5 } });
    CHECK(l.b->WaitFor("talk", l.s).has_value());
}

namespace {

// The ready flag of player id in a "room" event (false if it is not there).
bool ReadyOf(const json& r, uint8_t id) {
    for (const json& m : r["members"]) {
        if (GetInt(m, "id") == id) {
            return GetBool(m, "ready");
        }
    }
    return false;
}

} // namespace

TEST_CASE(RoomCommandsDoTheSame) {
    Lobby l;
    OpenAndDrain(l);
    l.a->Cmd("/sala invitar Carol");
    CHECK(l.c->WaitFor("room_invite", l.s).has_value());
    l.c->Cmd("/sala aceptar Alice");
    CHECK(WaitRoom(l.s, *l.c, [](const json& r) { return r["members"].size() == 3; }).has_value());
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.c->Cmd("/listo");
    CHECK(WaitRoom(l.s, *l.a, [&](const json& r) { return ReadyOf(r, l.c->id); }).has_value());
    CHECK(l.c->WaitForSys("ok", l.s)); // "Estás listo."
    l.c->Cmd("/ready"); // the English name, and again: no longer ready
    CHECK(WaitRoom(l.s, *l.a, [&](const json& r) { return !ReadyOf(r, l.c->id); }).has_value());
    l.b->Cmd("/s hola sala");
    auto m = l.c->WaitFor("room_chat", l.s);
    CHECK(m.has_value() && GetString(*m, "text") == "hola sala" && GetString(*m, "nick") == "Bob");
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.a->Cmd("/sala");
    auto t = l.a->WaitFor("sys", l.s);
    CHECK(t.has_value());
    std::string text = t.has_value() ? GetString(*t, "text") : std::string();
    CHECK(text.find("Alice") != std::string::npos && text.find("Bob") != std::string::npos &&
          text.find("Carol") != std::string::npos);
    l.a->Cmd("/sala abierta si");
    CHECK(WaitRoom(l.s, *l.b, [](const json& r) { return GetBool(r["settings"], "open"); }).has_value());
    l.a->Cmd("/sala viaje quizas");
    CHECK(l.a->WaitForSys("error", l.s)); // "Usa sí o no."
    l.b->Cmd("/sala premios no");
    CHECK(l.b->WaitForSys("error", l.s)); // only the host
    l.a->Cmd("/room kick Carol");
    CHECK(WaitRoom(l.s, *l.c, [](const json& r) { return !InRoom(r); }).has_value());
    l.b->Cmd("/sala salir");
    CHECK(WaitRoom(l.s, *l.b, [](const json& r) { return !InRoom(r); }).has_value());
    Drain(l.s, { l.a.get(), l.b.get(), l.c.get() });
    l.c->Cmd("/sala");
    auto none = l.c->WaitFor("sys", l.s);
    CHECK(none.has_value() && GetString(*none, "text") == Tr(Msg::RoomNone));
}
