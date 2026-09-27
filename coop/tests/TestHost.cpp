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
}

TEST_CASE(HostWinsAuthorityInPureRules) {
    std::vector<server::AuthMember> m = { { 1, kScene, 0, false, 100 }, { 2, kScene, 0, false, 900 } };
    m[1].host = true;
    auto owners = server::ComputeAuthority(m);
    CHECK_EQ(owners.at({ kScene, 0 }), (uint8_t)2);
}
