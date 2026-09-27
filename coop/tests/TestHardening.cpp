// Hardening from the final review of sub-project A: hostile packets, gifts that must never lose
// rupees, ops bound to their IP and config files edited by hand.
#include "TestNet.h"

#include "server/GiftManager.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

using namespace coop_test;

namespace {

coop::PlayerState ValidState() {
    coop::PlayerState s;
    s.sceneId = 5;
    s.form = 4;
    s.mask = 0x18;
    s.shield = 2;
    s.modelGroup = 14;
    s.face = 15;
    s.sword = 3;
    s.itemAction = -1;
    s.heldItemAction = 0x52;
    s.entrance = (uint16_t)(0x6D << 9);
    return s;
}

template <class Mutate> bool Rejects(Mutate mutate) {
    coop::PlayerState s = ValidState();
    mutate(s);
    return !coop::SanitizePlayerState(s);
}

void WriteFile(const std::string& path, const std::string& text) {
    std::ofstream(path) << text;
}

} // namespace

// --- Poses a puppet can draw (Critical #1) ---

TEST_CASE(SanitizeAcceptsEngineRanges) {
    coop::PlayerState s = ValidState();
    CHECK(coop::SanitizePlayerState(s));
    CHECK_EQ(s.sword, (uint8_t)3);
}

TEST_CASE(SanitizeRejectsOutOfRangeEnums) {
    CHECK(Rejects([](coop::PlayerState& s) { s.form = 5; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.mask = 0x19; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.shield = 3; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.modelGroup = 15; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.face = 16; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.itemAction = -2; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.heldItemAction = 0x53; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.entrance = (uint16_t)(0x6E << 9); }));
}

TEST_CASE(SanitizeRejectsNonFiniteAndFarAway) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(Rejects([&](coop::PlayerState& s) { s.pos[1] = nan; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.pos[0] = 1e9f; }));
    CHECK(Rejects([](coop::PlayerState& s) { s.speed = std::numeric_limits<float>::infinity(); }));
    CHECK(Rejects([&](coop::PlayerState& s) { s.unk_ABC = nan; }));
    CHECK(Rejects([&](coop::PlayerState& s) { s.unk_AB8 = nan; }));
    CHECK(Rejects([&](coop::PlayerState& s) { s.unk_B10 = nan; }));
}

TEST_CASE(SanitizeMasksStateFlagsAndSword) {
    coop::PlayerState s = ValidState();
    s.stateFlags1 = 0xFFFFFFFF;
    s.stateFlags2 = 0xFFFFFFFF;
    s.stateFlags3 = 0xFFFFFFFF;
    s.sword = 4; // Fierce Deity's: the human sword tables end at the Gilded Sword
    CHECK(coop::SanitizePlayerState(s));
    CHECK_EQ(s.stateFlags1, coop::pose_limits::kStateFlags1);
    CHECK_EQ(s.stateFlags2, coop::pose_limits::kStateFlags2);
    CHECK_EQ(s.stateFlags3, coop::pose_limits::kStateFlags3);
    CHECK_EQ(s.sword, (uint8_t)0);
}

TEST_CASE(ImplausibleStreamNotRelayed) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    b->SendState(5);
    s.PumpFor(50);
    coop::PlayerState bad;
    bad.sceneId = 5;
    bad.mask = 0x7F;
    auto bytes = coop::EncodePlayerState(bad);
    a->transport.Send(a->peer, coop::kChannelStream, bytes.data(), bytes.size());
    CHECK(!b->WaitForStream(s, 300).has_value());
    a->SendState(5);
    CHECK(b->WaitForStream(s).has_value());
}

// --- Gifts never lose rupees (Important #3) ---

TEST_CASE(GiftPaidAfterReceiverLeftIsRefunded) {
    TestServer s;
    auto a = Join(s, "Alice");
    int64_t gid = 0;
    {
        auto b = Join(s, "Bob");
        CHECK(a->WaitFor("join", s).has_value());
        a->Cmd("/gift Bob 30");
        auto d = a->WaitFor("gift_debit", s);
        CHECK(d.has_value());
        gid = (*d)["gid"].get<int64_t>();
        b->Close();
        CHECK(a->WaitFor("leave", s, 3000).has_value());
    }
    a->Send({ { "t", "gift_paid" }, { "gid", gid }, { "paid", 30 } });
    auto r = a->WaitFor("gift_refund", s);
    CHECK(r.has_value());
    CHECK_EQ((*r)["amount"].get<int>(), 30);
}

TEST_CASE(GiftPaidAfterExpiryIsRefunded) {
    coop::server::ServerConfig cfg;
    cfg.giftTimeoutMs = 150;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    a->Cmd("/gift Bob 25");
    auto d = a->WaitFor("gift_debit", s);
    CHECK(d.has_value());
    CHECK(a->WaitForSys("warn", s)); // "El regalo ha caducado."
    a->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 25 } });
    auto r = a->WaitFor("gift_refund", s);
    CHECK(r.has_value());
    CHECK_EQ((*r)["amount"].get<int>(), 25);
    CHECK(!b->WaitFor("gift_credit", s, 200).has_value());
}

TEST_CASE(GiftRefundNeverReachesNewcomerWithReusedId) {
    TestServer s;
    auto b = Join(s, "Bob");
    int64_t gid = 0;
    {
        auto a = Join(s, "Alice");
        CHECK(b->WaitFor("join", s).has_value());
        a->Cmd("/gift Bob 50");
        auto d = a->WaitFor("gift_debit", s);
        CHECK(d.has_value());
        a->Send({ { "t", "gift_paid" }, { "gid", (*d)["gid"] }, { "paid", 50 } });
        auto c = b->WaitFor("gift_credit", s);
        CHECK(c.has_value());
        gid = (*c)["gid"].get<int64_t>();
        a->Close();
        CHECK(b->WaitFor("leave", s, 3000).has_value());
    }
    auto carol = Join(s, "Carol"); // takes Alice's old player id
    CHECK(b->WaitFor("join", s).has_value());
    b->Send({ { "t", "gift_recv" }, { "gid", gid }, { "accepted", 20 } });
    auto ok = b->WaitFor("sys", s);
    CHECK(ok.has_value());
    CHECK_EQ((*ok)["text"].get<std::string>(), std::string("Alice te ha regalado 20 rupias."));
    CHECK(!carol->WaitFor("gift_refund", s, 300).has_value());
    CHECK(!carol->WaitFor("sys", s, 100).has_value());
}

TEST_CASE(GiftManagerLifecycleByConnection) {
    coop::server::GiftManager g;
    auto& paid = g.Create(10, 20, "Alice", "Bob", 10, 0);
    paid.stage = coop::server::PendingGift::WaitCredit;
    paid.paid = 10;
    uint32_t paidGid = paid.gid;
    uint32_t unpaidGid = g.Create(10, 20, "Alice", "Bob", 5, 0).gid;
    uint32_t leaverUnpaidGid = g.Create(20, 30, "Bob", "Carol", 7, 0).gid;

    auto refunds = g.OnPlayerLeft(20); // Bob (connection 20) leaves
    CHECK_EQ(refunds.size(), (size_t)1);
    CHECK_EQ(refunds[0].gid, paidGid);
    CHECK(g.Find(leaverUnpaidGid) == nullptr);
    auto* unpaid = g.Find(unpaidGid);
    CHECK(unpaid != nullptr);
    CHECK(unpaid->stage == coop::server::PendingGift::RefundOnDebit);

    CHECK(g.Expire(1000, 10000, 60000).empty());
    CHECK(g.Expire(70000, 10000, 60000).empty()); // orphan forgotten silently
    CHECK(g.Find(unpaidGid) == nullptr);
}

// --- Hostile or broken clients (Important #4) ---

TEST_CASE(InvalidFloodKicksPlayer) {
    TestServer s;
    auto a = Join(s, "Alice");
    for (int i = 0; i < coop::kInvalidKickCount + 5; i++) {
        a->SendRaw("{{{basura");
    }
    auto k = a->WaitFor("kicked", s, 2000);
    CHECK(k.has_value());
    CHECK(a->WaitDisconnected(s));
}

TEST_CASE(InvalidFloodBeforeHelloDisconnects) {
    TestServer s;
    auto c = Connect(s);
    for (int i = 0; i < coop::kInvalidKickCount + 5; i++) {
        c->SendRaw("{\"t\":\"desconocido\"}");
    }
    CHECK(c->WaitDisconnected(s, 2000));
}

TEST_CASE(OversizedPacketNeverReassembled) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->SendRaw(std::string(6000, 'x'));  // over the event limit, under the transport limit
    a->SendRaw(std::string(40000, 'x')); // over the transport limit: ENet drops it
    s.PumpFor(500);
    auto* rc = s.server->Players().ByNick("Alice");
    CHECK(rc != nullptr);
    CHECK_EQ(rc->invalidMessages, 1u);
}

TEST_CASE(StreamFloodIsRateLimited) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    b->SendState(5);
    s.PumpFor(50);
    a->SendState(5);
    CHECK(b->WaitForStream(s).has_value());
    b->streams.clear();
    for (int i = 0; i < 200; i++) {
        a->SendState(5);
    }
    s.PumpFor(300);
    CHECK(b->streams.size() >= (size_t)10);
    CHECK(b->streams.size() <= (size_t)(coop::kStreamBurst + 15));
}

TEST_CASE(LocFloodIsRateLimitedButConverges) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    for (int i = 0; i < 40; i++) {
        a->Send({ { "t", "loc" }, { "scene", 100 + i }, { "room", 0 }, { "entrance", 0 },
                  { "sceneName", "S" + std::to_string(i) } });
    }
    s.PumpFor(400);
    int count = 0;
    int lastScene = -1;
    while (auto l = b->TakeEvent("loc")) {
        count++;
        lastScene = (*l)["scene"].get<int>();
    }
    CHECK(count <= coop::kLocBurst + 4);
    CHECK(b->WaitUntil(s, 3000, [&] {
        while (auto l = b->TakeEvent("loc")) {
            lastScene = (*l)["scene"].get<int>();
        }
        return lastScene == 139;
    }));
}

TEST_CASE(HostileTextIsCleanedBeforeLogging) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->SendRaw("{\"t\":\"x\\n[INFO] falso\\u001b[31m" + std::string(300, 'y') + "\"}");
    a->Cmd("/pm Nadie \u001b[2J" + std::string(150, 'z')); // a known command: its line is logged
    s.PumpFor(200);
    auto lines = s.log.Lines();
    CHECK(lines.size() >= (size_t)2);
    for (const std::string& line : lines) {
        CHECK(line.find('\n') == std::string::npos);
        CHECK(line.find('\x1b') == std::string::npos);
        CHECK(line.size() < 250);
    }
}

// --- Ops (Important #5) ---

TEST_CASE(OpIsBoundToTheIpItWasGrantedFrom) {
    TestServer s;
    s.access.AddOp("Alice", "10.1.2.3"); // granted to another connection
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    a->Cmd("/kick Bob");
    CHECK(a->WaitForSys("error", s));
    CHECK(!b->WaitFor("kicked", s, 200).has_value());
}

TEST_CASE(OpNeedsTheLivePlayer) {
    TestServer s;
    s.server->ExecuteConsoleLine("op Fantasma");
    CHECK(!s.access.IsOp("Fantasma", "127.0.0.1"));
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("op Alice");
    CHECK(a->WaitForSys("ok", s));
    CHECK(s.access.IsOp("alice", "127.0.0.1"));
}

TEST_CASE(OpsFileWithoutIpGrantsNothing) {
    const std::string path = "test_ops_legacy.json";
    WriteFile(path, "[\"Alice\", {\"nick\":\"Bob\",\"ip\":\"127.0.0.1\"}, 7]");
    coop::server::AccessLists lists("", path);
    lists.Load();
    CHECK(!lists.IsOp("Alice", "127.0.0.1"));
    CHECK(lists.IsOp("bob", "127.0.0.1"));
    CHECK(!lists.IsOp("Bob", "10.0.0.1"));
    std::remove(path.c_str());
}

// --- Files edited by hand (re-graded minor) ---

TEST_CASE(ConfigWithWrongTypesFallsBackToDefaults) {
    const std::string path = "test_server_types.json";
    WriteFile(path, "{\"port\":\"abc\",\"maxPlayers\":\"x\",\"password\":5,\"motd\":null}");
    coop::server::ServerConfig cfg;
    std::string warn;
    CHECK(coop::server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.port, coop::kDefaultPort);
    CHECK_EQ(cfg.maxPlayers, coop::kMaxPlayers);
    CHECK(cfg.password.empty());
    CHECK(!warn.empty());
    std::remove(path.c_str());
}

TEST_CASE(AccessFilesWithWrongTypesAreSkipped) {
    const std::string bans = "test_bans_types.json";
    WriteFile(bans, "[{\"nick\":5,\"ip\":[1]}, \"x\", {\"nick\":\"Eve\"}]");
    coop::server::AccessLists lists(bans, "");
    lists.Load();
    CHECK_EQ(lists.Bans().size(), (size_t)1);
    CHECK(lists.IsBanned("eve", "1.1.1.1", nullptr));
    std::remove(bans.c_str());
}

TEST_CASE(AccessFilesCreatedOnFirstRun) {
    const std::string bans = "test_bans_new.json";
    const std::string ops = "test_ops_new.json";
    std::remove(bans.c_str());
    std::remove(ops.c_str());
    coop::server::AccessLists lists(bans, ops);
    lists.Load();
    CHECK(std::filesystem::exists(bans));
    CHECK(std::filesystem::exists(ops));
    std::remove(bans.c_str());
    std::remove(ops.c_str());
}
