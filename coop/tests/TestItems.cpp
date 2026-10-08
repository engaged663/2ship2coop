// Shared drops (spec docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md): the item record and its keys
// (common/ItemState), the book of what lies in each stage (server/ItemBook) and the events between games and server.
#include "TestWorld.h"

#include "common/ItemState.h"
#include "server/ItemBook.h"

#include <limits>

using namespace coop;
using namespace coop_test;

namespace {

ItemState Thrown(uint32_t key) {
    ItemState s;
    s.key = key;
    s.params = 0x0103;
    s.pos[0] = 12.5f;
    s.pos[1] = -3.25f;
    s.pos[2] = 1000.125f;
    s.vy = 8.f;
    s.speed = 2.f;
    s.grav = -0.9f;
    s.scale = 0.f;
    s.yaw = -12345;
    s.phase = 777;
    s.timer = 220;
    s.act = 1;
    return s;
}

} // namespace

TEST_CASE(ItemStateRoundTrips) {
    ItemState s = Thrown(0x82000001u);
    json ev = MakeEvent(ev::kItem);
    WriteItemState(s, ev);
    ItemState back;
    CHECK(ReadItemState(json::parse(SerializeEvent(ev)), back)); // through text, as on the wire
    CHECK_EQ(back.key, s.key);
    CHECK_EQ(back.id, s.id);
    CHECK_EQ(back.params, s.params);
    for (int i = 0; i < 3; i++) {
        CHECK_EQ(back.pos[i], s.pos[i]);
    }
    CHECK_EQ(back.vy, s.vy);
    CHECK_EQ(back.speed, s.speed);
    CHECK_EQ(back.grav, s.grav); // -0.9f exactly: the same trajectory everywhere
    CHECK_EQ(back.scale, s.scale);
    CHECK_EQ(back.yaw, s.yaw);
    CHECK_EQ(back.phase, s.phase);
    CHECK_EQ(back.timer, s.timer);
    CHECK_EQ(back.act, s.act);
    CHECK_EQ(ItemLifeMs(back), (int64_t)220 * 50 + item_limits::kLifeGraceMs);
}

TEST_CASE(ItemStateFairiesCarryNoMotion) {
    ItemState s;
    s.key = 5;
    s.id = kItemActorFairy;
    s.params = 0xFE02;
    s.pos[1] = 40.f;
    json ev = json::object();
    WriteItemState(s, ev);
    CHECK(!ev.contains("vy") && !ev.contains("act"));
    ItemState back;
    CHECK(ReadItemState(ev, back));
    CHECK_EQ(back.params, 0xFE02);
    CHECK_EQ(ItemLifeMs(back), item_limits::kFairyLifeMs);
    s.id = kItemActorStrayFairy;
    CHECK_EQ(ItemLifeMs(s), (int64_t)0); // until someone takes it
    ItemState lying = Thrown(1);
    lying.timer = -1;
    CHECK_EQ(ItemLifeMs(lying), (int64_t)0);
}

TEST_CASE(ItemStateRejectsGarbage) {
    json good = json::object();
    WriteItemState(Thrown(1), good);
    ItemState out;
    CHECK(ReadItemState(good, out));
    auto bad = [&](const char* field, const json& v) {
        json e = good;
        e[field] = v;
        return !ReadItemState(e, out);
    };
    CHECK(bad("id", 0x0F)); // not an item
    CHECK(bad("params", 0x10000));
    CHECK(bad("params", -1));
    CHECK(bad("key", -1));
    CHECK(bad("key", 0x100000000ll));
    CHECK(bad("pos", Arr(1.0, 2.0)));
    CHECK(bad("pos", Arr(1.0, 2.0, 1e9)));
    CHECK(bad("vy", "fast"));
    CHECK(bad("vy", std::numeric_limits<double>::quiet_NaN()));
    CHECK(bad("speed", 1000.0));
    CHECK(bad("scale", -0.5));
    CHECK(bad("act", 3));
    CHECK(bad("yaw", 40000));
    CHECK(bad("timer", 1.5));
    json missing = good;
    missing.erase("grav");
    CHECK(!ReadItemState(missing, out));
}

TEST_CASE(ItemKeysNeverOverlap) {
    uint32_t unique = MakeUniqueItemKey(3, 0x123456);
    uint32_t list = MakeListItemKey(2, 17);
    uint32_t twin = MakeTwinItemKey(0x0E8, MakeListItemKey(1, 4), 2);
    CHECK(ItemKeyKindOf(unique) == ItemKeyKind::Unique);
    CHECK_EQ(UniqueItemKeyPlayer(unique), (uint8_t)3);
    CHECK_EQ(unique & 0xFFFFFFu, 0x123456u);
    CHECK(ItemKeyKindOf(list) == ItemKeyKind::List);
    CHECK_EQ(list, 0x40020011u);
    CHECK(ItemKeyKindOf(twin) == ItemKeyKind::Twin);
    CHECK_EQ(twin, MakeTwinItemKey(0x0E8, MakeListItemKey(1, 4), 2)); // the same in every game
    CHECK(twin != MakeTwinItemKey(0x0E8, MakeListItemKey(1, 4), 3));
    CHECK(twin != MakeTwinItemKey(0x2AE, MakeListItemKey(1, 4), 2));
}

namespace {

ItemState Drop(uint32_t key, int16_t timer = 220) {
    ItemState s;
    s.key = key;
    s.timer = timer;
    return s;
}

} // namespace

TEST_CASE(ItemBookFirstTakerWins) {
    server::ItemBook book;
    uint32_t key = MakeUniqueItemKey(1, 1);
    CHECK(book.Add(0x2D, Drop(key), 1, 1000));
    CHECK(!book.Add(0x2D, Drop(key), 1, 1000)); // announced once
    CHECK(book.Take(0x2D, key) == server::TakeResult::Yours);
    CHECK(book.Take(0x2D, key) == server::TakeResult::Gone);
    CHECK(book.Live(0x2D).empty());
    CHECK(!book.Add(0x2D, Drop(key), 1, 2000)); // taken: never again
}

TEST_CASE(ItemBookTwinsExistOnce) {
    server::ItemBook book;
    uint32_t twin = MakeTwinItemKey(0x2AE, MakeListItemKey(0, 7), 1);
    CHECK(book.Add(0x2D, Drop(twin), 1, 0));
    CHECK(!book.Add(0x2D, Drop(twin), 2, 5)); // the other game's twin
    CHECK(book.Find(0x2D, twin) != nullptr && book.Find(0x2D, twin)->from == 1);
    CHECK(book.Take(0x2D, twin) == server::TakeResult::Yours);
    CHECK(!book.Add(0x2D, Drop(twin), 2, 10)); // a game that drops it later: it was taken
    auto taken = book.Taken(0x2D);
    CHECK(taken.size() == 1 && taken[0] == twin);
}

TEST_CASE(ItemBookUnknownKeysAreYoursOnce) {
    server::ItemBook book;
    uint32_t list = MakeListItemKey(1, 9); // a rupee of the room's list: every game has it, nobody announces it
    CHECK(book.Take(0x2D, list) == server::TakeResult::Yours);
    CHECK(book.Take(0x2D, list) == server::TakeResult::Gone);
    uint32_t unique = MakeUniqueItemKey(2, 5);
    CHECK(book.Take(0x2D, unique) == server::TakeResult::Yours);
    CHECK(book.Take(0x2D, unique) == server::TakeResult::Gone);
    auto taken = book.Taken(0x2D);
    CHECK(taken.size() == 1 && taken[0] == list); // a unique key never comes back: not listed
}

TEST_CASE(ItemBookRestOnlyFromItsDropper) {
    server::ItemBook book;
    uint32_t key = MakeUniqueItemKey(1, 2);
    CHECK(book.Add(0x2D, Drop(key), 1, 0));
    float pos[3] = { 5.f, 6.f, 7.f };
    CHECK(!book.Rest(0x2D, key, 2, pos));
    CHECK(!book.Find(0x2D, key)->rested);
    CHECK(book.Rest(0x2D, key, 1, pos));
    const server::LiveItem* it = book.Find(0x2D, key);
    CHECK(it->rested && it->state.pos[0] == 5.f && it->state.pos[2] == 7.f);
    CHECK(!book.Rest(0x2D, MakeUniqueItemKey(1, 9), 1, pos)); // unknown
}

TEST_CASE(ItemBookExpiresAndForgets) {
    server::ItemBook book;
    CHECK(book.Add(0x2D, Drop(MakeUniqueItemKey(1, 1), 20), 1, 0)); // 20 frames: 1 s + the grace
    CHECK(book.Add(0x2D, Drop(MakeUniqueItemKey(1, 2), -1), 1, 0)); // never vanishes
    book.Expire(1000 + item_limits::kLifeGraceMs - 1);
    CHECK_EQ(book.Live(0x2D).size(), (size_t)2);
    book.Expire(1000 + item_limits::kLifeGraceMs);
    CHECK_EQ(book.Live(0x2D).size(), (size_t)1);
    CHECK(book.Find(0x2D, MakeUniqueItemKey(1, 2)) != nullptr);
    book.Take(0x2D, MakeListItemKey(0, 1));
    book.Forget(0x2D);
    CHECK(book.Live(0x2D).empty() && book.Taken(0x2D).empty());
    CHECK(book.Stages().empty());
}

TEST_CASE(ItemBookHasLimits) {
    server::ItemBook book;
    for (uint32_t i = 0; i < kMaxItemsPerStage; i++) {
        CHECK(book.Add(0x10, Drop(MakeUniqueItemKey(1, i)), 1, 0));
    }
    CHECK(!book.Add(0x10, Drop(MakeUniqueItemKey(1, 999999)), 1, 0)); // full
    CHECK(book.Add(0x11, Drop(MakeUniqueItemKey(1, 999999)), 1, 0));  // another stage
    for (uint32_t i = 0; i < kMaxTakenItemsPerStage + 10; i++) {
        book.Take(0x12, MakeListItemKey(0, (int16_t)i));
    }
    CHECK_EQ(book.Taken(0x12).size(), kMaxTakenItemsPerStage);
}

namespace {

constexpr int16_t kScene = 0x2D;
constexpr int16_t kOther = 0x10;

struct Scene {
    TestServer s;
    std::unique_ptr<TestClient> a, b;
    explicit Scene(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        a->SendState(kScene, 0);
        b->SendState(kScene, 1);
        s.PumpFor(60);
    }
};

json ItemEv(uint32_t key, int16_t scene = kScene) {
    json ev = { { "t", "item" }, { "scene", scene } };
    ItemState st;
    st.key = key;
    st.params = 0x0003;
    st.pos[0] = 10.f;
    st.pos[1] = 20.f;
    st.pos[2] = 30.f;
    st.vy = 8.f;
    st.speed = 2.f;
    st.grav = -0.9f;
    st.yaw = 1234;
    st.timer = 220;
    st.act = 1;
    WriteItemState(st, ev);
    return ev;
}

json TakeEv(uint32_t key, bool after = false, int16_t scene = kScene) {
    json ev = { { "t", "item_take" }, { "scene", scene }, { "key", key } };
    if (after) {
        ev["after"] = true;
    }
    return ev;
}

json RestEv(uint32_t key, double x, double y, double z) {
    return { { "t", "item_rest" }, { "scene", kScene }, { "key", key }, { "pos", Arr(x, y, z) } };
}

json AskItems(TestServer& s, TestClient& c, int16_t scene = kScene) {
    c.Send({ { "t", "items" }, { "scene", scene }, { "layer", 0 } });
    auto got = c.WaitFor("items", s);
    if (!got.has_value()) {
        Fail(__FILE__, __LINE__, "no items list");
    }
    return *got;
}

bool Answer(TestServer& s, TestClient& c, bool expected) {
    auto got = c.WaitFor("item_take", s);
    return got.has_value() && GetBool(*got, "ok", !expected) == expected;
}

std::unique_ptr<TestClient> JoinHost(TestServer& s) {
    auto host = Connect(s);
    host->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", "#host" }, { "host", true },
                 { "token", "secreto-de-prueba" } });
    auto welcome = host->WaitFor("welcome", s);
    if (!welcome.has_value()) {
        Fail(__FILE__, __LINE__, "host not welcomed");
    }
    host->id = (uint8_t)GetInt(*welcome, "id");
    return host;
}

} // namespace

TEST_CASE(DroppedItemReachesTheStageOnce) {
    Scene sc;
    CHECK(sc.a->welcome["sync"]["drops"] == true);
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    auto got = sc.b->WaitFor("item", sc.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)sc.a->id);
    ItemState st;
    CHECK(ReadItemState(*got, st));
    CHECK_EQ(st.key, key);
    CHECK_EQ(st.yaw, (int16_t)1234);
    CHECK_EQ(st.act, (uint8_t)1);
    CHECK(!sc.a->WaitFor("item", sc.s, 150).has_value()); // not back to who dropped it
    sc.a->Send(ItemEv(key));                              // the same key again: it is there already
    CHECK(!sc.b->WaitFor("item", sc.s, 150).has_value());
}

TEST_CASE(ItemOfAnotherStageIsNotSent) {
    Scene sc;
    auto c = Join(sc.s, "Carol");
    EnterWorld(sc.s, *c);
    c->SendState(kScene, 0, 0, 0, 0, 0, 1); // the same scene, another layer
    sc.s.PumpFor(60);
    sc.a->Send(ItemEv(MakeUniqueItemKey(sc.a->id, 1)));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    CHECK(!c->WaitFor("item", sc.s, 150).has_value());
    sc.a->Send(ItemEv(MakeUniqueItemKey(sc.a->id, 2), kOther)); // a scene Alice is not in
    CHECK(!sc.b->WaitFor("item", sc.s, 150).has_value());
}

TEST_CASE(FirstToTakeAnItemGetsIt) {
    Scene sc;
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(TakeEv(key));
    CHECK(Answer(sc.s, *sc.b, true));
    auto gone = sc.a->WaitFor("item_gone", sc.s);
    CHECK(gone.has_value() && GetInt(*gone, "from") == (int64_t)sc.b->id && GetInt(*gone, "key") == (int64_t)key);
    sc.a->Send(TakeEv(key)); // too late
    CHECK(Answer(sc.s, *sc.a, false));
    CHECK(!sc.b->WaitFor("item_gone", sc.s, 150).has_value());
}

TEST_CASE(SimultaneousTakesGiveOneWinner) {
    Scene sc;
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.a->Send(TakeEv(key));
    sc.b->Send(TakeEv(key));
    auto ra = sc.a->WaitFor("item_take", sc.s);
    auto rb = sc.b->WaitFor("item_take", sc.s);
    CHECK(ra.has_value() && rb.has_value());
    CHECK(GetBool(*ra, "ok") != GetBool(*rb, "ok")); // exactly one of them
}

TEST_CASE(TakingAfterwardsGetsNoAnswer) {
    Scene sc; // fairies: used at once, the server only tells the others
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(TakeEv(key, true));
    CHECK(sc.a->WaitFor("item_gone", sc.s).has_value());
    CHECK(!sc.b->WaitFor("item_take", sc.s, 150).has_value());
}

TEST_CASE(TakeOfAnotherSceneIsIgnored) {
    Scene sc;
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(TakeEv(key, false, kOther)); // its game was somewhere else by then
    CHECK(!sc.b->WaitFor("item_take", sc.s, 150).has_value());
    CHECK(!sc.a->WaitFor("item_gone", sc.s, 150).has_value());
    sc.b->Send(TakeEv(key)); // it still lies there for whoever is in the scene
    CHECK(Answer(sc.s, *sc.b, true));
}

TEST_CASE(TwinAnnouncedTwiceReachesOthersOnce) {
    Scene sc;
    uint32_t twin = MakeTwinItemKey(0x2AE, MakeListItemKey(0, 3), 1);
    sc.a->Send(ItemEv(twin));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(ItemEv(twin)); // Bob's game dropped the same prize
    CHECK(!sc.a->WaitFor("item", sc.s, 150).has_value());
    sc.b->Send(TakeEv(twin));
    CHECK(Answer(sc.s, *sc.b, true));
    sc.a->Send(ItemEv(twin)); // a game that drops it again later: taken
    CHECK(!sc.b->WaitFor("item", sc.s, 150).has_value());
}

TEST_CASE(ItemComesToRestForEveryone) {
    Scene sc;
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(RestEv(key, 1.0, 2.0, 3.0));
    CHECK(!sc.a->WaitFor("item_rest", sc.s, 150).has_value()); // only its dropper says where it lies
    sc.a->Send(RestEv(key, 15.0, 0.0, 35.0));
    auto rest = sc.b->WaitFor("item_rest", sc.s);
    CHECK(rest.has_value() && GetInt(*rest, "key") == (int64_t)key && GetInt(*rest, "from") == (int64_t)sc.a->id);
    json list = AskItems(sc.s, *sc.b);
    CHECK_EQ(list["list"].size(), (size_t)1);
    CHECK(list["list"][0]["rest"] == true);
    CHECK(list["list"][0]["pos"][0].get<double>() == 15.0);
}

TEST_CASE(LatecomersSeeWhatLiesThere) {
    Scene sc;
    uint32_t lying = MakeUniqueItemKey(sc.a->id, 1);
    uint32_t taken = MakeUniqueItemKey(sc.a->id, 2);
    uint32_t listKey = MakeListItemKey(0, 4);
    sc.a->Send(ItemEv(lying));
    sc.a->Send(ItemEv(taken));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.b->Send(TakeEv(taken));
    sc.b->Send(TakeEv(listKey));
    CHECK(Answer(sc.s, *sc.b, true));
    CHECK(Answer(sc.s, *sc.b, true));
    auto c = Join(sc.s, "Carol");
    EnterWorld(sc.s, *c);
    c->SendState(kScene, 0);
    sc.s.PumpFor(60);
    json got = AskItems(sc.s, *c);
    CHECK_EQ(got["list"].size(), (size_t)1);
    CHECK_EQ(got["list"][0]["key"].get<int64_t>(), (int64_t)lying);
    CHECK(got["list"][0]["age"].get<int64_t>() >= 0);
    CHECK(got["list"][0]["rest"] == false);
    CHECK_EQ(got["taken"].size(), (size_t)1);
    CHECK_EQ(got["taken"][0].get<int64_t>(), (int64_t)listKey);
}

TEST_CASE(ItemsAreForgottenWhenTheStageEmpties) {
    Scene sc;
    sc.a->Send(ItemEv(MakeUniqueItemKey(sc.a->id, 1)));
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.a->SendState(kOther, 0);
    sc.b->SendState(kOther, 0);
    sc.s.PumpFor(80);
    sc.a->SendState(kScene, 0);
    sc.s.PumpFor(60);
    CHECK(AskItems(sc.s, *sc.a)["list"].empty());
}

TEST_CASE(SomeoneElsesKeyIsInvalid) {
    Scene sc;
    server::RemoteClient* alice = sc.s.server->Players().ById(sc.a->id);
    uint32_t before = alice->invalidMessages;
    sc.a->Send(ItemEv(MakeUniqueItemKey(sc.b->id, 1))); // a key of Bob's
    CHECK(!sc.b->WaitFor("item", sc.s, 150).has_value());
    CHECK(alice->invalidMessages > before);
    sc.a->Send({ { "t", "item_take" }, { "scene", kScene }, { "key", "x" } });
    sc.s.PumpFor(60);
    CHECK(alice->invalidMessages > before + 1);
    sc.a->Send(ItemEv(MakeTwinItemKey(0x2AE, 3, 1))); // twins belong to nobody
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
}

TEST_CASE(DropsOffSharesNothing) {
    server::ServerConfig cfg;
    cfg.drops = false;
    Scene sc(cfg);
    CHECK(sc.a->welcome["sync"]["drops"] == false);
    uint32_t key = MakeUniqueItemKey(sc.a->id, 1);
    sc.a->Send(ItemEv(key));
    CHECK(!sc.b->WaitFor("item", sc.s, 200).has_value());
    sc.b->Send(TakeEv(key));
    CHECK(!sc.b->WaitFor("item_take", sc.s, 200).has_value());
    sc.b->Send({ { "t", "items" }, { "scene", kScene } });
    CHECK(!sc.b->WaitFor("items", sc.s, 200).has_value());
}

TEST_CASE(HostsDropButNeverTake) {
    server::ServerConfig cfg;
    cfg.hostToken = "secreto-de-prueba";
    Scene sc(cfg);
    auto host = JoinHost(sc.s);
    EnterWorld(sc.s, *host);
    host->SendState(kScene, 0);
    sc.s.PumpFor(60);
    Drain(sc.s, { sc.a.get(), sc.b.get(), host.get() });
    uint32_t key = MakeUniqueItemKey(host->id, 1);
    host->Send(ItemEv(key)); // an enemy it simulates dropped it
    CHECK(sc.a->WaitFor("item", sc.s).has_value());
    CHECK(sc.b->WaitFor("item", sc.s).has_value());
    sc.a->Send(ItemEv(MakeUniqueItemKey(sc.a->id, 1)));
    CHECK(!host->WaitFor("item", sc.s, 150).has_value()); // the server's games need no items
    host->Send(TakeEv(key));
    CHECK(!host->WaitFor("item_take", sc.s, 150).has_value()); // nor take any
    sc.b->Send(TakeEv(key));
    CHECK(host->WaitFor("item_gone", sc.s).has_value()); // but they remove theirs
    CHECK(sc.a->WaitFor("item_gone", sc.s).has_value());
}
