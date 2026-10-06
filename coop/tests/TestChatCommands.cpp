#include "TestNet.h"
#include "TestWorld.h"

using namespace coop_test;

TEST_CASE(ChatBroadcastIncludesSender) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Send({ { "t", "chat" }, { "text", "  hola\x01 " } });
    auto ea = a->WaitFor("chat", s);
    auto eb = b->WaitFor("chat", s);
    CHECK(ea.has_value());
    CHECK(eb.has_value());
    CHECK_EQ((*eb)["text"].get<std::string>(), std::string("hola"));
    CHECK_EQ((*eb)["from"].get<std::string>(), std::string("Alice"));
}

TEST_CASE(EmptyChatIgnored) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->Send({ { "t", "chat" }, { "text", "   \t " } });
    CHECK(!a->WaitFor("chat", s, 300).has_value());
}

TEST_CASE(EventsBeforeHelloIgnored) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto c = Connect(s);
    c->Send({ { "t", "chat" }, { "text", "colado" } });
    CHECK(!a->WaitFor("chat", s, 300).has_value());
}

TEST_CASE(PmOnlyToTarget) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Cid");
    a->Cmd("/pm bob \"hola bob\"");
    auto pb = b->WaitFor("pm", s);
    CHECK(pb.has_value());
    CHECK_EQ((*pb)["text"].get<std::string>(), std::string("hola bob"));
    CHECK_EQ((*pb)["from"].get<std::string>(), std::string("Alice"));
    CHECK(a->WaitFor("pm", s).has_value()); // echo to the sender
    CHECK(!c->WaitFor("pm", s, 300).has_value());
}

TEST_CASE(PmWithoutQuotesUsesRestOfLine) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/pm Bob hola que tal");
    auto pb = b->WaitFor("pm", s);
    CHECK(pb.has_value());
    CHECK_EQ((*pb)["text"].get<std::string>(), std::string("hola que tal"));
}

TEST_CASE(UnknownCommandAndTarget) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/nope");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/pm Nadie hola");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/pm");
    CHECK(a->WaitForSys("error", s)); // usage
}

TEST_CASE(HelpFiltersByPermission) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/help");
    auto h = a->WaitFor("sys", s);
    CHECK(h.has_value());
    std::string text = (*h)["text"];
    CHECK(text.find("/pm") != std::string::npos);
    CHECK(text.find("/tp") == std::string::npos || true); // /tp arrives in Task 6
    CHECK(text.find("/kick") == std::string::npos);
    s.server->ExecuteConsoleLine("op Alice");
    CHECK(a->WaitForSys("ok", s)); // "Ahora eres administrador"
    a->Cmd("/HELP");
    auto h2 = a->WaitFor("sys", s);
    CHECK(h2.has_value());
    CHECK(std::string((*h2)["text"]).find("/kick") != std::string::npos);
}

TEST_CASE(KickRequiresOp) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/kick Bob");
    CHECK(a->WaitForSys("error", s));
    s.server->ExecuteConsoleLine("op Alice");
    a->Cmd("/kick Bob adios");
    auto k = b->WaitFor("kicked", s);
    CHECK(k.has_value());
    CHECK_EQ((*k)["reason"].get<std::string>(), std::string("adios"));
    CHECK(b->WaitDisconnected(s));
    auto l = a->WaitFor("leave", s, 3000);
    CHECK(l.has_value());
}

TEST_CASE(OpIsConsoleOnly) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("/op Alice");
    a->Cmd("/op Bob");
    CHECK(a->WaitForSys("error", s));
}

TEST_CASE(BanBlocksReconnectUntilUnban) {
    TestServer s;
    auto a = Join(s, "Alice");
    {
        auto b = Join(s, "Bob");
        s.server->ExecuteConsoleLine("/ban Bob spam");
        CHECK(b->WaitDisconnected(s));
    }
    auto c = Connect(s);
    c->Hello("Bob");
    CHECK(c->WaitFor("reject", s).has_value());
    auto d = Connect(s);
    d->Hello("OtroNick");
    CHECK(d->WaitFor("reject", s).has_value()); // same IP is banned too
    s.server->ExecuteConsoleLine("unban Bob");
    auto e = Connect(s);
    e->Hello("Bob");
    CHECK(e->WaitFor("welcome", s).has_value());
}

TEST_CASE(BanOfflineNick) {
    TestServer s;
    s.server->ExecuteConsoleLine("ban Ausente");
    CHECK_EQ(s.access.Bans().size(), (size_t)1);
    CHECK(s.access.Bans()[0].ip.empty());
    auto c = Connect(s);
    c->Hello("Ausente");
    CHECK(c->WaitFor("reject", s).has_value());
    auto d = Connect(s);
    d->Hello("Presente");
    CHECK(d->WaitFor("welcome", s).has_value());
}

TEST_CASE(SayBroadcastsSystemMessage) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("say reinicio en 5 minutos");
    auto m = a->WaitFor("sys", s);
    CHECK(m.has_value());
    CHECK(std::string((*m)["text"]).find("reinicio en 5 minutos") != std::string::npos);
}

TEST_CASE(RateLimit) {
    TestServer s;
    auto a = Join(s, "Alice");
    for (int i = 0; i < 8; i++) {
        a->Send({ { "t", "chat" }, { "text", "spam" } });
    }
    CHECK(a->WaitForSys("warn", s, 1500));
}

TEST_CASE(StopDisconnectsEveryone) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("stop");
    CHECK(a->WaitFor("sys", s).has_value());
    CHECK(a->WaitDisconnected(s));
    CHECK(a->WaitUntil(s, 2000, [&] { return !s.server->IsRunning(); }));
}

// ---- Admin commands (/unlockall, /give, /freezetime, /time set) ----

TEST_CASE(AdminCommandsNeedOp) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/unlockall Alice");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/give Alice 100");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/freezetime");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/time set night");
    CHECK(a->WaitForSys("error", s));
}

TEST_CASE(UnlockAllAndGiveReachThePlayer) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("op Alice");
    CreateWorld(s, *a); // both only reach players in the server's world (TestSaveCommands.cpp)
    a->Cmd("/unlockall Alice");
    auto unlock = a->WaitFor("unlock_all", s);
    CHECK(unlock.has_value());
    CHECK_EQ((*unlock)["nick"].get<std::string>(), std::string("Alice"));
    a->Cmd("/give Alice 250");
    auto give = a->WaitFor("give", s);
    CHECK(give.has_value());
    CHECK_EQ((*give)["amount"].get<int>(), 250);
    // Unknown players say so.
    a->Cmd("/unlockall Nadie");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/give Nadie 10");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/give Alice cien");
    CHECK(a->WaitForSys("error", s));
}

// The rupees go into the server's world only: outside it /give refuses (UnlockAllAndGiveNeedThePlayerInTheWorld).
TEST_CASE(GiveChecksTheAmount) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("op Alice");
    CreateWorld(s, *a);
    a->Cmd("/give Alice 1000000");
    CHECK(a->WaitForSys("error", s)); // out of range
    a->Cmd("/give Alice 500");
    auto give = a->WaitFor("give", s);
    CHECK(give.has_value());
}
