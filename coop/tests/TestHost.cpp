// Sub-project D: the headless host game that simulates the world for the server.
#include "TestWorld.h"

#include "server/World/RoomAuthority.h"

using namespace coop;
using namespace coop_test;

namespace {

constexpr int16_t kScene = 0x2D;

server::ServerConfig HostConfig() {
    server::ServerConfig cfg;
    cfg.hostToken = "secreto-de-prueba";
    cfg.maxPlayers = 1;
    return cfg;
}

std::unique_ptr<TestClient> JoinHost(TestServer& s, const std::string& token = "secreto-de-prueba") {
    auto host = Connect(s);
    host->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", "#host" }, { "host", true }, { "token", token } });
    auto welcome = host->WaitFor("welcome", s);
    if (welcome.has_value()) {
        host->welcome = *welcome;
        host->id = (uint8_t)GetInt(*welcome, "id");
    }
    return host;
}

} // namespace

TEST_CASE(HostNeedsTheToken) {
    TestServer s(HostConfig());
    auto bad = JoinHost(s, "otro");
    CHECK(bad->WaitFor("reject", s).has_value());
    auto host = JoinHost(s);
    CHECK(host->id != 0);
    CHECK(GetBool(host->welcome, "host"));
}

TEST_CASE(HostWithoutConfiguredTokenIsRefused) {
    TestServer s;
    auto host = JoinHost(s, "");
    CHECK(host->WaitFor("reject", s).has_value());
}

TEST_CASE(HostTakesNoPlayerSlotAndIsNotListed) {
    TestServer s(HostConfig()); // maxPlayers = 1
    auto host = JoinHost(s);
    CHECK(host->id != 0);
    auto a = Join(s, "Alice"); // still room for one player
    CHECK(a->id != 0);
    bool listed = false;
    for (auto& p : a->welcome["players"]) {
        listed = listed || p["nick"] == "#host";
    }
    CHECK(!listed);
    CHECK(!a->WaitFor("join", s, 200).has_value());
    a->Cmd("/list");
    auto sys = a->WaitFor("sys", s);
    CHECK(sys.has_value());
    CHECK(GetString(*sys, "text").find("#host") == std::string::npos);
}

TEST_CASE(HostOwnsItsRoomsAndFollowsAPlayer) {
    TestServer s(HostConfig());
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->SendState(kScene, 0);
    s.PumpFor(50);
    auto host = JoinHost(s);
    EnterWorld(s, *host);
    host->SendState(kScene, 0);
    s.PumpFor(300);
    std::optional<json> auth;
    while (auto ev = a->TakeEvent("auth")) {
        auth = ev;
    }
    CHECK(auth.has_value());
    bool hostOwns = false;
    for (const json& pair : (*auth)["rooms"]) {
        hostOwns = hostOwns || (pair[0] == 0 && pair[1].get<int>() == host->id);
    }
    CHECK(hostOwns); // arrived later, still the owner
    auto follow = host->WaitFor("host_follow", s);
    CHECK(follow.has_value());
    CHECK_EQ(GetInt(*follow, "id"), (int64_t)a->id);
    host->streams.clear();
    a->SendState(kScene, 0);
    auto got = host->WaitForStream(s); // another scene, but it is the one it follows
    CHECK(got.has_value());
    CHECK_EQ(got->sceneId, kScene);
}

TEST_CASE(HostWinsAuthorityInPureRules) {
    std::vector<server::AuthMember> m = { { 1, kScene, 0, false, 100 }, { 2, kScene, 0, false, 900 } };
    m[1].host = true;
    auto owners = server::ComputeAuthority(m);
    CHECK_EQ(owners.at({ kScene, 0 }), (uint8_t)2);
}

TEST_CASE(HostGetsPosesButNobodySeesTheHost) {
    TestServer s(HostConfig());
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    auto host = JoinHost(s);
    EnterWorld(s, *host);
    host->SendState(kScene, 0);
    a->SendState(kScene, 0);
    s.PumpFor(100);
    a->streams.clear();
    host->streams.clear();
    a->SendState(kScene, 0);
    auto got = host->WaitForStream(s);
    CHECK(got.has_value());
    CHECK_EQ(got->playerId, a->id);
    host->SendState(kScene, 0);
    CHECK(!a->WaitForStream(s, 300).has_value());
}

TEST_CASE(HostChangesNothingInTheWorld) {
    TestServer s(HostConfig());
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    auto host = JoinHost(s);
    EnterWorld(s, *host);
    Drain(s, { a.get(), host.get() });
    host->Send({ { "t", "wops" }, { "bits", Arr(Arr(Field("weekEventReg"), 3, 1)) }, { "bytes", json::array() },
                 { "adds", json::array() }, { "cycle", 1 } });
    CHECK(!a->WaitFor("wops", s, 300).has_value());
    host->Send({ { "t", "sot_propose" } });
    CHECK(!a->WaitFor("sys", s, 300).has_value()); // no vote started
    CHECK(s.server->World().Exists());
}

TEST_CASE(HostFollowsAPlayerInAnotherScene) {
    TestServer s(HostConfig());
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->SendState(kScene, 0);
    auto host = JoinHost(s);
    EnterWorld(s, *host);
    host->SendState(0x6F, 0); // it booted in Clock Town
    auto follow = host->WaitFor("host_follow", s);
    CHECK(follow.has_value());
    CHECK_EQ(GetInt(*follow, "id"), (int64_t)a->id);
    host->streams.clear();
    a->SendState(kScene, 0);
    auto got = host->WaitForStream(s); // another scene, but it is the one it follows
    CHECK(got.has_value());
    CHECK_EQ(got->sceneId, kScene);
}

TEST_CASE(HostKnowsThePlayers) {
    server::ServerConfig cfg = HostConfig();
    cfg.maxPlayers = 4;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    auto host = JoinHost(s);
    bool hasAlice = false;
    for (auto& p : host->welcome["players"]) {
        hasAlice = hasAlice || p["nick"] == "Alice";
    }
    CHECK(hasAlice); // there before it
    auto b = Join(s, "Bob");
    auto join = host->WaitFor("join", s);
    CHECK(join.has_value()); // arrived after it
    CHECK_EQ(GetString(*join, "nick"), std::string("Bob"));
    b.reset();
    CHECK(host->WaitFor("leave", s).has_value());
}

TEST_CASE(HostHurtsPlayersWithHighIdsButIsNeverHurt) {
    server::ServerConfig cfg = HostConfig();
    cfg.maxPlayers = 4;
    TestServer s(cfg);
    auto first = Join(s, "Primero");
    CreateWorld(s, *first);
    auto host = JoinHost(s); // takes id 2
    EnterWorld(s, *host);
    host->SendState(kScene, 0);
    std::vector<std::unique_ptr<TestClient>> players;
    for (const char* nick : { "Segundo", "Tercero", "Cuarto" }) {
        players.push_back(Join(s, nick));
        EnterWorld(s, *players.back());
        players.back()->SendState(kScene, 0);
    }
    TestClient& last = *players.back();
    CHECK(last.id > (uint8_t)kMaxPlayers); // 5: past the old 1..4 range
    s.PumpFor(300);
    json hurt = { { "t", "hurt" }, { "to", last.id }, { "kind", "col" },  { "dmgFlags", 0x1 },
                  { "effect", 0 }, { "damage", 8 },   { "hitEffect", 0 }, { "pos", Arr(0.0, 0.0, 0.0) } };
    host->Send(hurt);
    auto got = last.WaitFor("hurt", s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)host->id);
    // A player's game may think the host's ghost is a Link to hurt: the server drops it (the sender owns room 1,
    // where no host is, so only the host rule stops it).
    last.SendState(kScene, 1);
    s.PumpFor(300);
    hurt["to"] = host->id;
    last.Send(hurt);
    CHECK(!host->WaitFor("hurt", s, 300).has_value());
}

TEST_CASE(HostWaitsForTheWorldAndNeverCreatesIt) {
    server::ServerConfig cfg = HostConfig();
    cfg.maxPlayers = 4;
    TestServer s(cfg);
    auto host = JoinHost(s);
    host->Send({ { "t", "world_enter" } });
    CHECK(!host->WaitFor("world_full", s, 300).has_value()); // no world yet: it waits
    auto a = Join(s, "Alice");
    CreateWorld(s, *a); // Alice creates it although the host asked first
    auto full = host->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK(!GetBool(*full, "create"));
}

TEST_CASE(CreatorLeavesWithAHostWaiting) {
    server::ServerConfig cfg = HostConfig();
    cfg.maxPlayers = 4;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    json first = EnterWorld(s, *a); // asked to create it, never answers
    CHECK(GetBool(first, "create"));
    auto host = JoinHost(s);
    host->Send({ { "t", "world_enter" } });
    auto b = Join(s, "Bob");
    b->Send({ { "t", "world_enter" } });
    s.PumpFor(100);
    a.reset(); // the creator leaves: Bob, behind the host in the queue, is asked next
    auto ask = b->WaitFor("world_full", s);
    CHECK(ask.has_value());
    CHECK(GetBool(*ask, "create"));
    b->Send({ { "t", "world_init" }, { "fields", WorldFields(0) } });
    auto full = host->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK(!GetBool(*full, "create"));
}
