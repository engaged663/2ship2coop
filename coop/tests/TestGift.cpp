#include "TestNet.h"

#include "server/GiftManager.h"

using namespace coop_test;

TEST_CASE(GiftFullFlowWithPartialAccept) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/gift Bob 50");
    auto d = a->WaitFor("gift_debit", s);
    CHECK(d.has_value());
    CHECK_EQ((*d)["amount"].get<int>(), 50);
    int gid = (*d)["gid"];
    a->Send({ { "t", "gift_paid" }, { "gid", gid }, { "paid", 50 } });
    auto c = b->WaitFor("gift_credit", s);
    CHECK(c.has_value());
    CHECK_EQ((*c)["amount"].get<int>(), 50);
    CHECK_EQ((*c)["from"].get<std::string>(), std::string("Alice"));
    b->Send({ { "t", "gift_recv" }, { "gid", gid }, { "accepted", 30 } });
    auto r = a->WaitFor("gift_refund", s);
    CHECK(r.has_value());
    CHECK_EQ((*r)["amount"].get<int>(), 20);
    CHECK(a->WaitForSys("ok", s)); // "Has regalado 30 rupias a Bob"
    CHECK(b->WaitForSys("ok", s)); // "Alice te ha regalado 30 rupias"
}

TEST_CASE(GiftFullyAcceptedNoRefund) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/gift Bob 10");
    auto d = a->WaitFor("gift_debit", s);
    CHECK(d.has_value());
    a->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 10 } });
    auto c = b->WaitFor("gift_credit", s);
    CHECK(c.has_value());
    b->Send({ { "t", "gift_recv" }, { "gid", (*c)["gid"] }, { "accepted", 10 } });
    CHECK(a->WaitForSys("ok", s));
    CHECK(!a->WaitFor("gift_refund", s, 300).has_value());
}

TEST_CASE(GiftInsufficientFunds) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/gift Bob 50");
    auto d = a->WaitFor("gift_debit", s);
    CHECK(d.has_value());
    a->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 0 } });
    CHECK(a->WaitForSys("error", s));
    CHECK(!b->WaitFor("gift_credit", s, 300).has_value());
}

TEST_CASE(GiftValidation) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/gift Bob 0");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/gift Bob mil");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/gift Alice 5");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/gift Bob 1000");
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/gift Nadie 5");
    CHECK(a->WaitForSys("error", s));
    CHECK(!a->WaitFor("gift_debit", s, 200).has_value());
}

TEST_CASE(GiftForgedReplyIgnored) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/gift Bob 50");
    auto d = a->WaitFor("gift_debit", s);
    CHECK(d.has_value());
    // Bob cannot "pay" on Alice's behalf, and nobody can confirm an unknown gift.
    b->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 50 } });
    b->Send({ { "t", "gift_recv" }, { "gid", 999 }, { "accepted", 1 } });
    CHECK(!b->WaitFor("gift_credit", s, 300).has_value());
}

TEST_CASE(GiftReceiverLeavesRefunds) {
    TestServer s;
    auto a = Join(s, "Alice");
    {
        auto b = Join(s, "Bob");
        CHECK(a->WaitFor("join", s).has_value());
        a->Cmd("/gift Bob 40");
        auto d = a->WaitFor("gift_debit", s);
        CHECK(d.has_value());
        a->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 40 } });
        CHECK(b->WaitFor("gift_credit", s).has_value());
        b->Close();
    }
    auto r = a->WaitFor("gift_refund", s, 3000);
    CHECK(r.has_value());
    CHECK_EQ((*r)["amount"].get<int>(), 40);
}

TEST_CASE(GiftManagerExpiry) {
    coop::server::GiftManager g;
    auto& p = g.Create(1, 2, 10, 0);
    p.stage = coop::server::PendingGift::WaitCredit;
    p.paid = 10;
    CHECK(g.TakeExpired(5000, 10000).empty());
    auto expired = g.TakeExpired(10001, 10000);
    CHECK_EQ(expired.size(), (size_t)1);
    CHECK_EQ(expired[0].paid, 10);
    CHECK(g.Find(expired[0].gid) == nullptr);
}

TEST_CASE(GiftManagerCancelledBy) {
    coop::server::GiftManager g;
    auto& toLeaver = g.Create(1, 2, 10, 0); // player 2 leaves -> cancelled
    uint32_t gidA = toLeaver.gid;
    auto& fromLeaverUnpaid = g.Create(2, 3, 5, 0); // sender left before paying -> cancelled
    uint32_t gidB = fromLeaverUnpaid.gid;
    auto& fromLeaverPaid = g.Create(2, 4, 7, 0); // already paid -> receiver can still take it
    fromLeaverPaid.stage = coop::server::PendingGift::WaitCredit;
    uint32_t gidC = fromLeaverPaid.gid;
    auto cancelled = g.TakeCancelledBy(2);
    CHECK_EQ(cancelled.size(), (size_t)2);
    CHECK(g.Find(gidA) == nullptr);
    CHECK(g.Find(gidB) == nullptr);
    CHECK(g.Find(gidC) != nullptr);
}
