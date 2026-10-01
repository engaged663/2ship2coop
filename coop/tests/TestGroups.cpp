// Groups and invitations (spec 2026-09-30-coop-grupos-actividades §2): server.json, GroupBook's rules, then the
// commands and events through a real server.
#include "TestWorld.h"

#include "server/GroupBook.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

using namespace coop;
using namespace coop_test;

TEST_CASE(ConfigReadsGroupOptions) {
    const std::string path = "test_server_groups.json";
    {
        std::ofstream f(path);
        f << "{\"groups\":false,\"bossCutscenes\":false,\"inviteSeconds\":30}";
    }
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(!cfg.groups);
    CHECK(!cfg.bossCutscenes);
    CHECK_EQ(cfg.inviteMs, 30000);
    CHECK(warn.empty());
    {
        std::ofstream f(path);
        f << "{\"groups\":1,\"inviteSeconds\":5}";
    }
    server::ServerConfig bad;
    CHECK(server::LoadOrCreateConfig(path, bad, &warn));
    CHECK(bad.groups);
    CHECK_EQ(bad.inviteMs, 60000);
    CHECK(!warn.empty());
    std::remove(path.c_str());
}

using server::AcceptResult;
using server::GroupBook;
using server::GroupChanges;
using server::InviteResult;

namespace {

constexpr int64_t kTtl = 60000;

bool Contains(const std::vector<uint8_t>& v, uint8_t id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

bool EndedWith(const GroupChanges& ch, uint8_t from, uint8_t to, const char* reason) {
    for (const auto& e : ch.ended) {
        if (e.invite.from == from && e.invite.to == to && std::string(e.reason) == reason) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE(GroupInviteCreatesAGroupAndAcceptJoinsIt) {
    GroupBook book;
    GroupChanges ch;
    CHECK(book.Invite(1, 2, 0, kTtl, ch) == InviteResult::Sent);
    const server::Group* g = book.GroupOf(1);
    CHECK(g != nullptr);
    CHECK_EQ(g->Leader(), (uint8_t)1);
    CHECK_EQ(g->members.size(), (size_t)1);
    CHECK(book.GroupOf(2) == nullptr);
    CHECK_EQ(book.InvitesTo(2).size(), (size_t)1);
    GroupChanges joined;
    uint8_t inviter = 0;
    CHECK(book.Accept(2, 0, joined, &inviter) == AcceptResult::Joined);
    CHECK_EQ(inviter, (uint8_t)1);
    CHECK(book.SameGroup(1, 2));
    CHECK(book.InvitesTo(2).empty());
    CHECK(EndedWith(joined, 1, 2, "accepted"));
    CHECK_EQ(joined.groups.size(), (size_t)1);
    CHECK(book.Mates(1) == std::vector<uint8_t>{ 2 });
}

TEST_CASE(GroupInviteRefusesSelfAndMatesAndRenews) {
    GroupBook book;
    GroupChanges ch;
    CHECK(book.Invite(1, 1, 0, kTtl, ch) == InviteResult::Self);
    CHECK(book.Invite(1, 2, 0, kTtl, ch) == InviteResult::Sent);
    CHECK(book.Invite(1, 2, 10, kTtl, ch) == InviteResult::Renewed);
    CHECK_EQ(book.InvitesTo(2).size(), (size_t)1);
    CHECK_EQ(book.InvitesTo(2)[0].expiresMs, (int64_t)10 + kTtl);
    uint8_t inviter = 0;
    CHECK(book.Accept(2, 1, ch, &inviter) == AcceptResult::Joined);
    CHECK(book.Invite(2, 1, 0, kTtl, ch) == InviteResult::AlreadyMate);
}

TEST_CASE(GroupIsFullAtMaxPlayers) {
    GroupBook book;
    GroupChanges ch;
    uint8_t inviter = 0;
    for (uint8_t p = 2; p <= kMaxPlayers; p++) {
        CHECK(book.Invite(1, p, 0, kTtl, ch) == InviteResult::Sent);
        CHECK(book.Accept(p, 1, ch, &inviter) == AcceptResult::Joined);
    }
    CHECK_EQ(book.GroupOf(1)->members.size(), (size_t)kMaxPlayers);
    CHECK(book.Invite(1, kMaxPlayers + 1, 0, kTtl, ch) == InviteResult::Full);
}

TEST_CASE(GroupAcceptingAnotherGroupLeavesTheOld) {
    GroupBook book;
    GroupChanges ch;
    uint8_t inviter = 0;
    book.Invite(1, 2, 0, kTtl, ch);
    book.Accept(2, 1, ch, &inviter); // {1, 2}
    book.Invite(3, 2, 0, kTtl, ch);  // {3} invites 2
    GroupChanges moved;
    CHECK(book.Accept(2, 0, moved, &inviter) == AcceptResult::Joined); // the latest: 3's
    CHECK_EQ(inviter, (uint8_t)3);
    CHECK(book.SameGroup(2, 3));
    CHECK(book.GroupOf(1) == nullptr); // alone without invitations: gone
    CHECK(Contains(moved.removed, 1));
    CHECK(!Contains(moved.removed, 2)); // 2 is in a group again
    CHECK_EQ(moved.dissolved.size(), (size_t)1);
}

TEST_CASE(GroupDeclineAndExpiryDissolveALonelyGroup) {
    GroupBook book;
    GroupChanges ch;
    book.Invite(1, 2, 0, kTtl, ch);
    book.Invite(1, 3, 0, kTtl, ch);
    GroupChanges declined;
    CHECK_EQ(book.Decline(2, 0, declined), 1);
    CHECK(EndedWith(declined, 1, 2, "declined"));
    CHECK(book.GroupOf(1) != nullptr); // 3 may still come
    GroupChanges early;
    book.Expire(kTtl - 1, early);
    CHECK(early.ended.empty());
    GroupChanges expired;
    book.Expire(kTtl, expired);
    CHECK(EndedWith(expired, 1, 3, "expired"));
    CHECK(book.GroupOf(1) == nullptr);
    CHECK(Contains(expired.removed, 1));
    CHECK(Contains(expired.lonely, 1)); // it never had anyone: nothing to tell
}

TEST_CASE(GroupLeaderLeavingPassesTheLead) {
    GroupBook book;
    GroupChanges ch;
    uint8_t inviter = 0;
    book.Invite(1, 2, 0, kTtl, ch);
    book.Accept(2, 1, ch, &inviter);
    book.Invite(1, 3, 0, kTtl, ch);
    book.Accept(3, 1, ch, &inviter);
    GroupChanges left;
    book.Leave(1, left);
    CHECK(book.GroupOf(1) == nullptr);
    CHECK_EQ(book.GroupOf(2)->Leader(), (uint8_t)2);
    CHECK(book.SameGroup(2, 3));
    GroupChanges last;
    book.Leave(2, last);
    CHECK(book.GroupOf(3) == nullptr); // the last one alone: dissolved
    CHECK(Contains(last.removed, 3));
}

TEST_CASE(GroupLeavingCancelsInvitationsBothWays) {
    GroupBook book;
    GroupChanges ch;
    book.Invite(1, 2, 0, kTtl, ch);
    book.Invite(3, 1, 0, kTtl, ch);
    GroupChanges left;
    book.Leave(1, left);
    CHECK(book.InvitesTo(2).empty());
    CHECK(book.InvitesTo(1).empty());
    CHECK(EndedWith(left, 1, 2, "cancelled"));
    CHECK(EndedWith(left, 3, 1, "cancelled"));
    CHECK(book.GroupOf(3) == nullptr); // 3 had invited only 1: alone now
}

namespace {

constexpr int16_t kScene = 0x40;
constexpr int16_t kOther = 0x41;

// Alice (kScene, at 100,0,200), Bob and Carol (kOther) in the server's world.
struct World3 {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c;
    explicit World3(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        a->SendState(kScene, 0, 0, 100.f, 0.f, 200.f);
        b->SendState(kOther, 0);
        c->SendState(kOther, 0);
        s.PumpFor(80);
        Drain(s, { a.get(), b.get(), c.get() });
    }
};

// Alice invites Bob and Bob accepts.
void Pair(World3& w) {
    w.a->Cmd("/invitar Bob");
    if (!w.b->WaitFor("invite", w.s).has_value()) {
        Fail(__FILE__, __LINE__, "no invite");
    }
    w.b->Cmd("/aceptar");
    if (!w.b->WaitFor("group", w.s).has_value()) {
        Fail(__FILE__, __LINE__, "no group");
    }
    Drain(w.s, { w.a.get(), w.b.get(), w.c.get() });
}

} // namespace

TEST_CASE(InviteAndAcceptTakeTheGuestToTheInviter) {
    World3 w;
    w.a->Cmd("/invitar Bob");
    auto inv = w.b->WaitFor("invite", w.s);
    CHECK(inv.has_value());
    CHECK_EQ(GetString(*inv, "nick"), std::string("Alice"));
    CHECK_EQ(GetInt(*inv, "from"), (int64_t)w.a->id);
    CHECK(GetInt(*inv, "ms") > 0);
    auto lonely = w.a->WaitFor("group", w.s); // Alice alone, waiting for Bob
    CHECK(lonely.has_value() && GetInt(*lonely, "leader") == w.a->id);
    w.b->Cmd("/aceptar Alice");
    auto tp = w.b->WaitFor("tp", w.s);
    CHECK(tp.has_value());
    CHECK_EQ(GetInt(*tp, "scene"), (int64_t)kScene);
    CHECK_EQ((*tp)["pos"][0].get<double>(), 100.0);
    auto gb = w.b->WaitFor("group", w.s);
    auto ga = w.a->WaitFor("group", w.s);
    CHECK(gb.has_value() && ga.has_value());
    CHECK_EQ((*gb)["members"].size(), (size_t)2);
    CHECK_EQ(GetInt(*gb, "leader"), (int64_t)w.a->id);
    auto end = w.b->WaitFor("invite_end", w.s);
    CHECK(end.has_value() && GetString(*end, "reason") == "accepted");
    CHECK(!w.c->WaitFor("group", w.s, 150).has_value()); // Carol has nothing to do with it
}

TEST_CASE(InviteAllReachesEveryoneInTheWorld) {
    World3 w;
    auto d = Join(w.s, "Dave"); // connected, not in the world
    w.s.PumpFor(40);
    w.a->Cmd("/invitar todos");
    CHECK(w.b->WaitFor("invite", w.s).has_value());
    CHECK(w.c->WaitFor("invite", w.s).has_value());
    CHECK(!d->WaitFor("invite", w.s, 150).has_value());
}

TEST_CASE(DeclineTellsTheInviterAndTheLonelyGroupGoes) {
    World3 w;
    w.a->Cmd("/invitar Bob");
    CHECK(w.b->WaitFor("invite", w.s).has_value());
    CHECK(w.a->WaitFor("group", w.s).has_value());
    Drain(w.s, { w.a.get(), w.b.get() });
    w.b->Cmd("/rechazar");
    auto end = w.b->WaitFor("invite_end", w.s);
    CHECK(end.has_value() && GetString(*end, "reason") == "declined");
    auto gone = w.a->WaitFor("group", w.s);
    CHECK(gone.has_value() && GetInt(*gone, "id") == 0);
    auto sys = w.a->WaitFor("sys", w.s); // "Bob ha rechazado tu invitación." (and nothing about a broken group)
    CHECK(sys.has_value() && GetString(*sys, "text").find("Bob") != std::string::npos);
}

TEST_CASE(InvitationExpires) {
    server::ServerConfig cfg;
    cfg.inviteMs = 200;
    World3 w(cfg);
    w.a->Cmd("/invitar Bob");
    CHECK(w.b->WaitFor("invite", w.s).has_value());
    auto end = w.b->WaitFor("invite_end", w.s, 1500);
    CHECK(end.has_value() && GetString(*end, "reason") == "expired");
    w.b->Cmd("/aceptar");
    CHECK(!w.b->WaitFor("tp", w.s, 200).has_value());
}

TEST_CASE(LeavingTheWorldOrDisconnectingLeavesTheGroup) {
    World3 w;
    Pair(w);
    w.b->Send({ { "t", "world_leave" } });
    auto g = w.a->WaitFor("group", w.s);
    CHECK(g.has_value() && GetInt(*g, "id") == 0); // Alice alone: dissolved
    EnterWorld(w.s, *w.b);
    w.b->SendState(kOther, 0);
    w.s.PumpFor(40);
    Drain(w.s, { w.a.get(), w.b.get(), w.c.get() });
    Pair(w);
    w.b->Close();
    auto g2 = w.a->WaitFor("group", w.s, 3000);
    CHECK(g2.has_value() && GetInt(*g2, "id") == 0);
    auto e = Join(w.s, "Eve"); // may take Bob's id: never his group
    EnterWorld(w.s, *e);
    w.s.PumpFor(60);
    CHECK(!e->WaitFor("group", w.s, 150).has_value());
}

TEST_CASE(GroupCommandListsMembersAndInvitations) {
    World3 w;
    Pair(w);
    w.c->Cmd("/invitar Alice"); // Alice stays in her group until she accepts
    CHECK(w.a->WaitFor("invite", w.s).has_value());
    Drain(w.s, { w.a.get(), w.b.get(), w.c.get() });
    w.a->Cmd("/grupo");
    auto reply = w.a->WaitFor("sys", w.s);
    CHECK(reply.has_value());
    std::string text = GetString(*reply, "text");
    CHECK(text.find("Alice") != std::string::npos);
    CHECK(text.find("Bob") != std::string::npos);
    CHECK(text.find("Carol") != std::string::npos); // the pending invitation
}

TEST_CASE(LeaveGroupCommandTellsTheOthers) {
    World3 w;
    Pair(w);
    w.b->Cmd("/dejargrupo");
    CHECK(w.b->WaitForSys("info", w.s)); // "Has salido del grupo."
    auto gone = w.a->WaitFor("group", w.s);
    CHECK(gone.has_value() && GetInt(*gone, "id") == 0);
}

TEST_CASE(GroupsOffRefusesInvitations) {
    server::ServerConfig cfg;
    cfg.groups = false;
    World3 w(cfg);
    w.a->Cmd("/invitar Bob");
    auto err = w.a->WaitFor("sys", w.s);
    CHECK(err.has_value() && GetString(*err, "level") == "error");
    CHECK(err.has_value() && GetString(*err, "text") == Tr(Msg::GroupsOff)); // not "unknown command"
    CHECK(!w.b->WaitFor("invite", w.s, 150).has_value());
}

TEST_CASE(ConfigReadsEffectsOption) {
    const std::string path = "test_server_effects.json";
    {
        std::ofstream f(path);
        f << "{\"effects\":false}";
    }
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(!cfg.effects);
    CHECK(warn.empty());
    std::remove(path.c_str());
}
