#include "TestNet.h"

using namespace coop_test;

TEST_CASE(WelcomeAndJoinBroadcast) {
    TestServer s;
    auto a = Join(s, "Alice");
    CHECK_EQ(a->welcome["players"].size(), (size_t)0);
    auto b = Join(s, "Bob");
    auto j = a->WaitFor("join", s);
    CHECK(j.has_value());
    CHECK_EQ((*j)["nick"].get<std::string>(), std::string("Bob"));
    CHECK_EQ(b->welcome["players"].size(), (size_t)1);
    CHECK_EQ(b->welcome["players"][0]["nick"].get<std::string>(), std::string("Alice"));
    CHECK(a->id != b->id);
}

TEST_CASE(FifthPlayerRejected) {
    TestServer s;
    auto p1 = Join(s, "P1_");
    auto p2 = Join(s, "P2_");
    auto p3 = Join(s, "P3_");
    auto p4 = Join(s, "P4_");
    auto c = Connect(s);
    c->Hello("P5_");
    auto r = c->WaitFor("reject", s);
    CHECK(r.has_value());
    CHECK(c->WaitDisconnected(s));
}

TEST_CASE(DuplicateNickCaseInsensitive) {
    TestServer s;
    auto a = Join(s, "Link");
    auto c = Connect(s);
    c->Hello("LINK");
    CHECK(c->WaitFor("reject", s).has_value());
}

TEST_CASE(InvalidNickAndWrongProtocolRejected) {
    TestServer s;
    auto c = Connect(s);
    c->Hello("x");
    CHECK(c->WaitFor("reject", s).has_value());
    auto d = Connect(s);
    d->Hello("Good_Nick", "", 999);
    CHECK(d->WaitFor("reject", s).has_value());
}

TEST_CASE(PasswordRequired) {
    coop::server::ServerConfig cfg;
    cfg.password = "zelda";
    TestServer s(cfg);
    auto c = Connect(s);
    c->Hello("Alice", "mal");
    CHECK(c->WaitFor("reject", s).has_value());
    auto d = Connect(s);
    d->Hello("Alice", "zelda");
    CHECK(d->WaitFor("welcome", s).has_value());
}

TEST_CASE(SlotIsFreedAfterLeave) {
    TestServer s;
    auto p1 = Join(s, "P1_");
    auto p2 = Join(s, "P2_");
    auto p3 = Join(s, "P3_");
    {
        auto p4 = Join(s, "P4_");
        p4->Close();
    }
    CHECK(p1->WaitFor("leave", s, 3000).has_value());
    auto p5 = Join(s, "P5_");
    CHECK(p5->id >= 1 && p5->id <= 4);
}

TEST_CASE(LeaveBroadcastOnDisconnect) {
    TestServer s;
    auto a = Join(s, "Alice");
    {
        auto b = Join(s, "Bob");
        CHECK(a->WaitFor("join", s).has_value());
        b->Close();
    }
    auto l = a->WaitFor("leave", s, 3000);
    CHECK(l.has_value());
    CHECK_EQ((*l)["nick"].get<std::string>(), std::string("Bob"));
}

TEST_CASE(HandshakeTimeoutDisconnects) {
    coop::server::ServerConfig cfg;
    cfg.handshakeTimeoutMs = 200;
    TestServer s(cfg);
    auto c = Connect(s);
    CHECK(c->WaitDisconnected(s, 2000));
}

TEST_CASE(GarbageEventIgnored) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->SendRaw("{{{nope");
    a->SendRaw("{\"t\":5}");
    a->SendRaw("{\"t\":\"desconocido\"}");
    a->SendRaw("{\"t\":\"hello\",\"proto\":1,\"nick\":\"Otra\"}"); // second hello ignored
    auto b = Join(s, "Bob");
    auto j = a->WaitFor("join", s);
    CHECK(j.has_value());
    CHECK_EQ((*j)["nick"].get<std::string>(), std::string("Bob"));
    CHECK(!a->disconnected);
}
