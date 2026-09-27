// Sub-project D3: every game in the server's world runs the same executable.
#include "TestNet.h"

#include "common/BuildId.h"

using namespace coop;
using namespace coop_test;

namespace {

std::unique_ptr<TestClient> JoinBuild(TestServer& s, const std::string& nick, const std::string& build) {
    auto c = Connect(s);
    c->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", nick }, { "pass", "" }, { "build", build } });
    auto welcome = c->WaitFor("welcome", s, 500);
    if (welcome.has_value()) {
        c->welcome = *welcome;
        c->id = (uint8_t)GetInt(*welcome, "id");
    }
    return c;
}

} // namespace

TEST_CASE(Fnv1aKnownValues) {
    CHECK_EQ(Fnv1a64(nullptr, 0), 0xCBF29CE484222325ull);
    const uint8_t a = 'a';
    CHECK_EQ(Fnv1a64(&a, 1), 0xAF63DC4C8601EC8Cull);
    CHECK_EQ(BuildId().size(), (size_t)16); // the test exe itself
    CHECK_EQ(FileBuildId("no-such-file.exe"), std::string());
}

TEST_CASE(SameBuildOnly) {
    TestServer s;
    auto a = JoinBuild(s, "Alice", "aaaa000000000001");
    CHECK(a->id != 0);
    auto b = JoinBuild(s, "Bob", "bbbb000000000002");
    auto reject = b->WaitFor("reject", s);
    CHECK(reject.has_value());
    CHECK(GetString(*reject, "reason").find("mismo") != std::string::npos);
    auto c = JoinBuild(s, "Carol", "aaaa000000000001");
    CHECK(c->id != 0);
}

TEST_CASE(BuildForgottenWhenEveryoneLeaves) {
    TestServer s;
    {
        auto a = JoinBuild(s, "Alice", "aaaa000000000001");
        CHECK(a->id != 0);
    }
    s.PumpFor(300);
    auto b = JoinBuild(s, "Bob", "bbbb000000000002"); // a new version of the mod after everyone left
    CHECK(b->id != 0);
}

TEST_CASE(NoBuildIsNotChecked) {
    TestServer s; // bots and tools send no build
    auto a = JoinBuild(s, "Alice", "aaaa000000000001");
    auto bot = Join(s, "Bot");
    CHECK(bot->id != 0);
}

TEST_CASE(SameBuildCanBeTurnedOff) {
    server::ServerConfig cfg;
    cfg.requireSameBuild = false;
    TestServer s(cfg);
    auto a = JoinBuild(s, "Alice", "aaaa000000000001");
    auto b = JoinBuild(s, "Bob", "bbbb000000000002");
    CHECK(b->id != 0);
}
