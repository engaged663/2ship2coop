// Server-side stores of the shared world: world bytes and ops, the clock, saved players, the vote.
#include "TestWorld.h"

#include "common/Clock.h"
#include "common/Text.h"
#include "server/World/JsonFile.h"
#include "server/World/PlayerStore.h"
#include "server/World/SotVote.h"
#include "server/World/WorldClock.h"
#include "server/World/WorldImage.h"
#include "server/World/WorldStore.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>

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

// A world.json from another version of the server still loads: what is missing starts empty, what changed size is
// cut or padded, what this version does not know is dropped (each case is a warning). Only a non-hex value fails.
TEST_CASE(WorldStoreLoadsOlderSchemas) {
    json fields = WorldFields(0x11);
    fields.erase("sceneExtra");
    fields["owls"] = "ab"; // one byte instead of two
    fields["zzz"] = "00";
    json saved = { { "version", 1 }, { "cycle", 3 }, { "fields", fields } };
    WorldStore store;
    std::string err;
    std::vector<std::string> warnings;
    CHECK(store.FromJson(saved, &err, &warnings));
    CHECK_EQ(warnings.size(), (size_t)3);
    for (const char* name : { "sceneExtra", "owls", "zzz" }) {
        CHECK(std::any_of(warnings.begin(), warnings.end(),
                          [name](const std::string& w) { return w.find(name) != std::string::npos; }));
    }
    CHECK_EQ(store.Cycle(), 3);
    CHECK(store.Fields()[Field("sceneExtra")] == std::vector<uint8_t>(480, 0));
    CHECK(store.Fields()[Field("owls")] == std::vector<uint8_t>({ 0xAB, 0x00 }));
    CHECK_EQ(store.Fields()[Field("items")][0], (uint8_t)0x11);
    CHECK(store.Start() == store.Fields()); // no copy of the cycle's start in the file: the current world
    fields["items"] = "xyz";
    saved["fields"] = fields;
    WorldStore bad;
    CHECK(!bad.FromJson(saved, &err));
    CHECK(err.find("items") != std::string::npos);
    CHECK_EQ(store.ToJson()["version"].get<int>(), 2);
}

TEST_CASE(WorldStoreReplaceSetsBoth) {
    FieldSet now = WorldStore::EmptyFields();
    FieldSet start = WorldStore::EmptyFields();
    now[Field("owls")][0] = 1;
    start[Field("owls")][0] = 2;
    WorldStore store;
    store.Replace(now, start, 7);
    CHECK(store.Exists());
    CHECK_EQ(store.Cycle(), 7);
    CHECK(store.Fields() == now);
    CHECK(store.Start() == start);
    store.RestoreCycleStart(8);
    CHECK(store.Fields() == start);
}

TEST_CASE(SetAsideNeverOverwrites) {
    TempDir dir("coop_test_set_aside");
    std::string path = dir.File("world.json");
    std::string first;
    std::string second;
    std::ofstream(path) << "uno";
    CHECK(SetAside(path, &first));
    std::ofstream(path) << "dos";
    CHECK(SetAside(path, &second));
    CHECK(first != second);
    CHECK(first.find("world.json.bad-") != std::string::npos);
    CHECK(std::filesystem::exists(first));
    CHECK(std::filesystem::exists(second));
    CHECK(!std::filesystem::exists(path));
    CHECK(!SetAside(dir.File("nada.json"), &first)); // nothing to set aside
    CHECK_EQ(FileStamp().size(), (size_t)19);       // 2026-10-05_14-30-00
}

TEST_CASE(WorldImageRoundTrip) {
    TempDir dir("coop_test_world_image");
    WorldImage image;
    image.fields = WorldStore::EmptyFields();
    image.start = WorldStore::EmptyFields();
    image.fields[Field("owls")][1] = 0x40;
    image.start[Field("items")][3] = 0x07;
    image.cycle = 4;
    image.clockAbs = 1234;
    image.inverted = true;
    image.frozen = true;
    PlayerRecord ana;
    ana.nick = "Ana";
    ana.inv = { { "v", 1 }, { "fields", { { "rupees", "0a00" } } } };
    ana.cycle = 4;
    ana.start = ana.inv;
    ana.startCycle = 4;
    PlayerRecord bob = ana;
    bob.nick = "Bob";
    bob.cycle = 3; // missed the last Song of Time
    image.players = { ana, bob };
    std::string err;
    std::string world = dir.File("world.json");
    std::string players = dir.File("players");
    std::ofstream(dir.File("old.json")) << "{}";
    CHECK(SaveWorldImage(image, world, players, &err));
    WorldImage back;
    std::vector<std::string> warnings;
    CHECK(LoadWorldImage(world, players, back, &warnings, &err));
    CHECK(warnings.empty());
    CHECK(back.fields == image.fields);
    CHECK(back.start == image.start);
    CHECK_EQ(back.cycle, 4);
    CHECK_EQ(back.clockAbs, 1234u);
    CHECK(back.inverted);
    CHECK(back.frozen);
    std::map<std::string, PlayerRecord> byNick;
    for (const PlayerRecord& r : back.players) {
        byNick[r.nick] = r;
    }
    CHECK_EQ(byNick.size(), (size_t)2);
    CHECK_EQ(byNick["Ana"].inv, ana.inv);
    CHECK_EQ(byNick["Bob"].cycle, 3);
    CHECK_EQ(byNick["Bob"].startCycle, 4);
    // Saving another image puts the previous players aside (.old), never deletes them
    image.players = { bob };
    CHECK(SaveWorldImage(image, world, players, &err));
    CHECK(std::filesystem::exists(std::filesystem::path(players) / "ana.json.old"));
    CHECK(!std::filesystem::exists(std::filesystem::path(players) / "ana.json"));
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

// A damaged player file is set aside (never overwritten by the next upload): it can still be fixed by hand.
TEST_CASE(PlayerStoreSetsAsideBadFiles) {
    TempDir dir("coop_test_players_aside");
    std::ofstream(dir.path / "ana.json") << "{roto";
    PlayerStore players(dir.path.string());
    std::string warn;
    players.LoadAll(&warn);
    CHECK(warn.find("ana.json") != std::string::npos);
    CHECK(players.Get("ana") == nullptr);
    CHECK(!std::filesystem::exists(dir.path / "ana.json"));
    auto setAside = [&dir]() {
        std::vector<std::filesystem::path> found;
        for (const auto& entry : std::filesystem::directory_iterator(dir.path)) {
            if (entry.path().filename().string().rfind("ana.json.bad-", 0) == 0) {
                found.push_back(entry.path());
            }
        }
        return found;
    };
    CHECK_EQ(setAside().size(), (size_t)1);
    players.Upload("Ana", json{ { "r", 1 } }, 1);
    std::string err;
    CHECK(players.SaveAll(&err));
    CHECK(std::filesystem::exists(dir.path / "ana.json"));
    std::vector<std::filesystem::path> aside = setAside();
    CHECK_EQ(aside.size(), (size_t)1);
    std::ifstream in(aside[0]);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(text, std::string("{roto"));
}

TEST_CASE(PlayerStoreReplaceAll) {
    TempDir dir("coop_test_players_replace");
    PlayerStore players(dir.path.string());
    players.Upload("Ana", json{ { "r", 1 } }, 1);
    std::string err;
    CHECK(players.SaveAll(&err));
    PlayerRecord bob;
    bob.nick = "Bob";
    bob.inv = { { "r", 2 } };
    bob.cycle = 3;
    bob.start = bob.inv;
    bob.startCycle = 3;
    players.ReplaceAll({ bob });
    CHECK(players.Get("Ana") == nullptr);
    CHECK_EQ(players.Get("bob")->cycle, 3);
    CHECK(players.SaveAll(&err));
    CHECK(std::filesystem::exists(dir.path / "bob.json"));
    CHECK(std::filesystem::exists(dir.path / "ana.json.old"));
    CHECK(!std::filesystem::exists(dir.path / "ana.json"));
}

// One player's file written into a world that already has others (the converter's --player-only).
TEST_CASE(PlayerStoreSetsOneRecord) {
    TempDir dir("coop_test_players_set");
    PlayerStore players(dir.path.string());
    players.Upload("Ana", json{ { "r", 1 } }, 2);
    std::string err;
    CHECK(players.SaveAll(&err));
    PlayerRecord bob;
    bob.nick = "Bob";
    bob.inv = { { "r", 5 } };
    bob.cycle = 2;
    bob.start = { { "r", 0 } };
    bob.startCycle = 2;
    players.Set(bob);
    CHECK(players.SaveAll(&err));
    CHECK(std::filesystem::exists(dir.path / "ana.json")); // the others stay as they were
    CHECK(!std::filesystem::exists(dir.path / "ana.json.old"));
    PlayerStore again(dir.path.string());
    again.LoadAll(nullptr);
    CHECK(again.Get("ana") != nullptr);
    CHECK_EQ(again.Get("bob")->inv, bob.inv);
    CHECK_EQ(again.Get("bob")->start, bob.start);
    CHECK(!again.IsStale("Bob", 2));
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
