// Shared props (pots, grass, items lying around) and the ending: relay, memory per scene, latecomers.
#include "TestWorld.h"

using namespace coop;
using namespace coop_test;

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

json Prop(int64_t key, int child, int16_t scene = kScene) {
    return { { "t", "prop" }, { "scene", scene }, { "key", key }, { "child", child } };
}

json AskProps(TestServer& s, TestClient& c, int16_t scene = kScene) {
    c.Send({ { "t", "props" }, { "scene", scene } });
    auto got = c.WaitFor("props", s);
    if (!got.has_value()) {
        Fail(__FILE__, __LINE__, "no props list");
    }
    return *got;
}

} // namespace

TEST_CASE(PropGoneReachesTheSceneAndLatecomers) {
    Scene sc;
    sc.a->Send(Prop(0x10005, -1));
    auto got = sc.b->WaitFor("prop", sc.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)sc.a->id);
    CHECK_EQ(GetInt(*got, "key"), (int64_t)0x10005);
    CHECK(!sc.a->WaitFor("prop", sc.s, 150).has_value()); // not back to the sender
    sc.a->Send(Prop(0x10005, -1)); // broken twice (both at once): said once
    CHECK(!sc.b->WaitFor("prop", sc.s, 150).has_value());

    auto c = Join(sc.s, "Carol");
    EnterWorld(sc.s, *c);
    c->SendState(kScene, 0);
    sc.s.PumpFor(60);
    json list = AskProps(sc.s, *c);
    CHECK_EQ(list["list"].size(), (size_t)1);
    CHECK_EQ(list["list"][0][0].get<int64_t>(), (int64_t)0x10005);
    CHECK_EQ(list["list"][0][1].get<int>(), -1);
}

TEST_CASE(PropOfAnotherSceneIsIgnored) {
    Scene sc;
    sc.a->Send(Prop(7, -1, kOther));
    CHECK(!sc.b->WaitFor("prop", sc.s, 200).has_value());
    CHECK(AskProps(sc.s, *sc.b, kOther)["list"].empty());
}

TEST_CASE(PropsGrowBackWhenTheSceneEmpties) {
    Scene sc;
    sc.a->Send(Prop(3, 2));
    CHECK(sc.b->WaitFor("prop", sc.s).has_value());
    sc.a->SendState(kOther, 0);
    sc.b->SendState(kOther, 0);
    sc.s.PumpFor(80);
    sc.a->SendState(kScene, 0);
    sc.s.PumpFor(60);
    CHECK(AskProps(sc.s, *sc.a)["list"].empty());
}

TEST_CASE(DroppedItemShownToTheScene) {
    Scene sc;
    json item = { { "t", "item" }, { "scene", kScene }, { "key", 0x81000001ll }, { "id", 0xE },
                  { "params", 0x1 },  { "pos", Arr(1.0, 2.0, 3.0) } };
    sc.a->Send(item);
    auto got = sc.b->WaitFor("item", sc.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)sc.a->id);
    sc.a->Send(Prop(0x81000001ll, -3)); // picked up: gone for everyone, never remembered
    CHECK(sc.b->WaitFor("prop", sc.s).has_value());
    CHECK(AskProps(sc.s, *sc.b)["list"].empty());
}

TEST_CASE(RegrowingGrassCutIsRelayedEveryTime) {
    Scene sc;
    sc.a->Send(Prop(0x20003, -4));
    CHECK(sc.b->WaitFor("prop", sc.s).has_value());
    sc.a->Send(Prop(0x20003, -4)); // it grew back and was cut again
    CHECK(sc.b->WaitFor("prop", sc.s).has_value());
    CHECK(AskProps(sc.s, *sc.b)["list"].empty());
}

TEST_CASE(SharedPropsOffRelaysNothing) {
    server::ServerConfig cfg;
    cfg.sharedProps = false;
    Scene sc(cfg);
    sc.a->Send(Prop(1, -1));
    CHECK(!sc.b->WaitFor("prop", sc.s, 200).has_value());
}
