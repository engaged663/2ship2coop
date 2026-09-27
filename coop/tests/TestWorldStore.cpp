// Server-side stores of the shared world: world bytes and ops, the clock, saved players, the vote.
#include "TestWorld.h"

#include "common/Clock.h"
#include "common/Text.h"
#include "server/World/JsonFile.h"
#include "server/World/PlayerStore.h"
#include "server/World/SotVote.h"
#include "server/World/WorldClock.h"
#include "server/World/WorldStore.h"

#include <fstream>

using namespace coop;
using namespace coop::server;
using namespace coop_test;

TEST_CASE(WorldStoreAppliesAndReportsChanges) {
    WorldStore store;
    store.StartCycle(WorldStore::EmptyFields(), 1);
    world::Ops ops;
    ops.bits.push_back({ Field("weekEventReg"), 7, 0x04, 0 });
    ops.bits.push_back({ Field("weekEventReg"), 8, 0, 0x01 }); // clears a bit that is already clear
    ops.bytes.push_back({ Field("masks"), 3, 0x3A });
    ops.adds.push_back({ Field("fairies"), 2, 300 });
    ops.adds.push_back({ Field("keys"), 20, 1 }); // outside the field
    world::Ops relay;
    world::Ops corrections;
    int invalid = 0;
    CHECK(store.Apply(ops, relay, corrections, &invalid));
    CHECK_EQ(invalid, 1);
    CHECK_EQ(relay.bits.size(), (size_t)1);
    CHECK_EQ(relay.bytes.size(), (size_t)1);
    CHECK_EQ(relay.adds.size(), (size_t)1);
    CHECK_EQ(relay.adds[0].delta, 255); // what was really added
    CHECK_EQ(corrections.adds.size(), (size_t)1);
    CHECK_EQ(corrections.adds[0].delta, -45); // the sender added 300 locally: take back 45
    CHECK_EQ(store.Fields()[Field("weekEventReg")][7], (uint8_t)0x04);
    CHECK_EQ(store.Fields()[Field("masks")][3], (uint8_t)0x3A);
    CHECK_EQ(store.Fields()[Field("fairies")][2], (uint8_t)255);
    // The same bits and bytes again change nothing and are not relayed.
    world::Ops again;
    again.bits = ops.bits;
    again.bytes = ops.bytes;
    world::Ops relay2;
    world::Ops corrections2;
    CHECK(!store.Apply(again, relay2, corrections2, &invalid));
    CHECK(relay2.Empty());
    CHECK(corrections2.Empty());
    CHECK_EQ(invalid, 0);
}

TEST_CASE(WorldStoreFieldsJsonRoundTrip) {
    FieldSet fields;
    std::string err;
    CHECK(WorldStore::ParseFields(WorldFields(0x5A), fields, &err));
    WorldStore store;
    store.StartCycle(fields, 3);
    WorldStore back;
    CHECK(back.FromJson(store.ToJson(), &err));
    CHECK(back.Exists());
    CHECK_EQ(back.Cycle(), 3);
    CHECK(back.Fields() == fields);
    CHECK_EQ(store.FieldsJson(), WorldFields(0x5A));
    // Every field must be there with its exact size; the error names it.
    json missing = WorldFields();
    missing.erase("owls");
    CHECK(!WorldStore::ParseFields(missing, fields, &err));
    CHECK(err.find("owls") != std::string::npos);
    json shortField = WorldFields();
    shortField["keys"] = "00";
    CHECK(!WorldStore::ParseFields(shortField, fields, &err));
    CHECK(err.find("keys") != std::string::npos);
    json notHex = WorldFields();
    notHex["codes"] = std::string(40, 'z');
    CHECK(!WorldStore::ParseFields(notHex, fields, &err));
    CHECK(!WorldStore::ParseFields(json::array(), fields, &err));
    CHECK(!back.FromJson(json{ { "cycle", 0 }, { "fields", WorldFields() } }, &err));
    CHECK(back.Cycle() == 3); // a failed load changes nothing
}

TEST_CASE(WorldStoreCycleStartSnapshot) {
    WorldStore store;
    store.StartCycle(WorldStore::EmptyFields(), 1);
    world::Ops ops;
    world::Ops relay;
    world::Ops corrections;
    int invalid = 0;
    ops.bits.push_back({ Field("sceneFlags"), 100, 0x80, 0 });
    CHECK(store.Apply(ops, relay, corrections, &invalid));
    store.RestoreCycleStart(2); // the moon
    CHECK_EQ(store.Cycle(), 2);
    CHECK_EQ(store.Fields()[Field("sceneFlags")][100], (uint8_t)0);
    WorldStore back;
    std::string err;
    CHECK(back.FromJson(store.ToJson(), &err));
    CHECK_EQ(back.Cycle(), 2);
    CHECK_EQ(back.Fields()[Field("sceneFlags")][100], (uint8_t)0);
}

TEST_CASE(WorldClockRunsOnlyWhenAsked) {
    WorldClock c;
    CHECK_EQ(c.Abs(5000), 0u);
    c.SetRunning(true, 1000);
    CHECK_EQ(c.Abs(3000), 120u); // 2 s at 60 units/s
    c.SetInverted(true, 3000);
    CHECK_EQ(c.Abs(6000), 180u); // + 3 s at 20 units/s
    c.SetRunning(false, 6000);
    CHECK_EQ(c.Abs(60000), 180u);
    c.Set(clock::kMoonAbs - 10, 60000);
    CHECK(!c.MoonReached(60000));
    c.SetRunning(true, 60000);
    CHECK(c.MoonReached(62000));
    CHECK_EQ(c.Abs(100000), clock::kMoonAbs); // never past the moon
}

TEST_CASE(PlayerStoreKeepsCycleStartCopies) {
    PlayerStore players("");
    json a1 = { { "r", 1 } };
    json a2 = { { "r", 2 } };
    json a3 = { { "r", 3 } };
    players.Upload("Alice", a1, 1);
    players.Upload("Bob", a1, 1);
    players.Upload("alice", a2, 2); // first upload of cycle 2: Alice's cycle-start copy
    players.Upload("ALICE", a3, 2);
    CHECK_EQ(players.Count(), (size_t)2);
    CHECK_EQ(players.Get("Alice")->inv, a3);
    CHECK_EQ(players.Get("Alice")->start, a2);
    CHECK(!players.IsStale("Alice", 2));
    CHECK(players.IsStale("Bob", 2)); // missed the Song of Time
    CHECK(!players.IsStale("Nobody", 2));
    players.RestoreCycleStart(2, 3); // the moon
    CHECK_EQ(players.Get("Alice")->inv, a2);
    CHECK(!players.IsStale("Alice", 3));
    CHECK_EQ(players.Get("Bob")->inv, a1); // not in that cycle: keeps its data and stays stale
    CHECK(players.IsStale("Bob", 3));
}

TEST_CASE(PlayerStorePersistsToDisk) {
    TempDir dir("coop_test_players");
    std::string err;
    std::string warn;
    {
        PlayerStore players(dir.path.string());
        players.LoadAll(&warn);
        players.Upload("Alice", json{ { "r", 1 } }, 1);
        CHECK(players.SaveAll(&err));
        players.Upload("Alice", json{ { "r", 2 } }, 1);
        CHECK(players.SaveAll(&err)); // replaces the file
    }
    CHECK(std::filesystem::exists(dir.path / "alice.json"));
    CHECK(!std::filesystem::exists(dir.path / "alice.json.tmp"));
    {
        std::ofstream broken(dir.path / "broken.json");
        broken << "{roto";
    }
    PlayerStore again(dir.path.string());
    again.LoadAll(&warn);
    CHECK(warn.find("broken") != std::string::npos);
    CHECK(again.Get("alice") != nullptr);
    CHECK_EQ(again.Get("alice")->inv, (json{ { "r", 2 } }));
    CHECK_EQ(again.Get("alice")->start, (json{ { "r", 1 } }));
    CHECK_EQ(again.Get("alice")->nick, std::string("Alice"));
    again.Clear(); // a new world: the files are set aside, never deleted
    CHECK(again.Get("alice") == nullptr);
    CHECK(!std::filesystem::exists(dir.path / "alice.json"));
    CHECK(std::filesystem::exists(dir.path / "alice.json.old"));
    json saved;
    bool missing = false;
    CHECK(!LoadJsonFile(dir.File("nada.json"), saved, &err, &missing));
    CHECK(missing);
}

TEST_CASE(SotVoteNeedsMoreThanHalf) {
    using R = SotVote::Result;
    SotVote vote;
    std::vector<uint32_t> four = { 1, 2, 3, 4 };
    vote.Start(1, 0, 30000);
    CHECK(vote.Active());
    CHECK_EQ(vote.Proposer(), 1u);
    CHECK(vote.Evaluate(four, 0) == R::Pending); // 1 of 4
    vote.Cast(2, true);
    CHECK(vote.Evaluate(four, 0) == R::Pending); // 2 of 4 is only half
    vote.Cast(3, true);
    CHECK(vote.Evaluate(four, 0) == R::Passed);
    vote.Start(1, 0, 30000);
    vote.Cast(2, false);
    vote.Cast(3, false);
    CHECK(vote.Evaluate(four, 0) == R::Failed); // 3 yes are no longer possible
    CHECK_EQ(vote.Yes(four), 1);
    CHECK_EQ(vote.No(four), 2);
    vote.Start(1, 0, 30000);
    CHECK(vote.Evaluate(four, 30000) == R::Failed); // time is up
    CHECK(vote.Evaluate({ 1 }, 0) == R::Passed);    // alone: proposing is enough
    CHECK(vote.Evaluate({ 2, 3 }, 0) == R::Pending); // the proposer left: only the others count
    CHECK(vote.Evaluate({}, 0) == R::Failed);
    CHECK_EQ(vote.SecondsLeft(10000), 20);
    vote.Cancel();
    CHECK(!vote.Active());
}

TEST_CASE(PlayerStoreHandlesReservedNames) {
    // "nul", "con", "com1"... are devices on Windows whatever the extension: such a nick needs another file name.
    TempDir dir("coop_test_players_reserved");
    std::string err;
    {
        PlayerStore players(dir.path.string());
        players.LoadAll(nullptr);
        players.Upload("Nul", json{ { "r", 1 } }, 1);
        players.Upload("Alice", json{ { "r", 2 } }, 1);
        CHECK(players.SaveAll(&err));
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir.path)) {
        CHECK(ToLower(entry.path().stem().string()) != "nul");
    }
    PlayerStore again(dir.path.string());
    again.LoadAll(nullptr);
    CHECK_EQ(again.Count(), (size_t)2);
    CHECK(again.Get("NUL") != nullptr);
    if (again.Get("nul") != nullptr) {
        CHECK_EQ(again.Get("nul")->inv, (json{ { "r", 1 } }));
    }
    CHECK_EQ(again.Get("alice")->inv, (json{ { "r", 2 } }));
}
