#include "TestNet.h"

using namespace coop_test;

TEST_CASE(StreamRelayedOnlyToSameScene) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Cid");
    b->SendState(0x6F);
    c->SendState(0x10);
    s.PumpFor(100);
    a->SendState(0x6F);
    auto st = b->WaitForStream(s);
    CHECK(st.has_value());
    CHECK_EQ(st->playerId, a->id);
    CHECK_EQ(st->sceneId, (int16_t)0x6F);
    CHECK(!c->WaitForStream(s, 300).has_value());
}

TEST_CASE(StreamNotEchoedToSender) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->SendState(0x6F);
    s.PumpFor(50);
    a->SendState(0x6F);
    CHECK(!a->WaitForStream(s, 300).has_value());
}

TEST_CASE(MalformedStreamDropped) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    b->SendState(0x6F);
    s.PumpFor(50);
    uint8_t junk[5] = { coop::kStreamPlayerState, 0, 1, 2, 3 };
    a->transport.Send(a->peer, coop::kChannelStream, junk, sizeof(junk));
    CHECK(!b->WaitForStream(s, 300).has_value());
    a->SendState(0x6F);
    CHECK(b->WaitForStream(s).has_value()); // server still relays valid ones
}

TEST_CASE(LocBroadcastOnSceneChange) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "entrance", 0xD800 }, { "sceneName", "South Clock Town" } });
    auto l = b->WaitFor("loc", s);
    CHECK(l.has_value());
    CHECK_EQ((*l)["id"].get<int>(), (int)a->id);
    CHECK_EQ((*l)["scene"].get<int>(), 0x6F);
    CHECK_EQ((*l)["sceneName"].get<std::string>(), std::string("South Clock Town"));
    // A late joiner sees the scene in its welcome.
    auto c = Join(s, "Cid");
    bool found = false;
    for (auto& p : c->welcome["players"]) {
        found = found || (p["nick"] == "Alice" && p["sceneName"] == "South Clock Town");
    }
    CHECK(found);
}

TEST_CASE(TpSendsTargetLocation) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    b->SendState(0x6F, 2, 0xD800, 10.f, 20.f, 30.f);
    s.PumpFor(100);
    a->Cmd("/tp bob");
    auto t = a->WaitFor("tp", s);
    CHECK(t.has_value());
    CHECK_EQ((*t)["scene"].get<int>(), 0x6F);
    CHECK_EQ((*t)["room"].get<int>(), 2);
    CHECK_EQ((*t)["entrance"].get<int>(), 0xD800);
    CHECK_EQ((*t)["pos"][2].get<float>(), 30.f);
    CHECK(b->WaitForSys("info", s)); // "Alice se está teletransportando a ti"
    a->Cmd("/tp Alice");
    CHECK(a->WaitForSys("error", s)); // self
}

TEST_CASE(TpTargetWithoutLocation) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/tp Bob");
    CHECK(a->WaitForSys("error", s));
    CHECK(!a->WaitFor("tp", s, 200).has_value());
}

TEST_CASE(TpFromConsoleRefused) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("tp Alice"); // must not crash; console has no body to move
    s.PumpFor(50);
    CHECK(!a->WaitFor("tp", s, 200).has_value());
}
