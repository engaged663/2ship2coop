// "map_pos": where each player is on their minimap goes to everyone else (a game in another scene that shows the same
// map draws them: Clock Town's five scenes share one). Server Handlers/MapHandlers.cpp.
#include "TestNet.h"

#include <cmath>

using namespace coop;
using namespace coop_test;

namespace {

json MapPos(int64_t map, double x, double y, int64_t yaw) {
    return { { "t", "map_pos" }, { "map", map }, { "x", x }, { "y", y }, { "yaw", yaw } };
}

} // namespace

TEST_CASE(MapPosGoesToEveryoneElse) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Carol");
    CHECK(a->WaitFor("join", s).has_value());
    // Bob and Carol in different scenes: the map position goes to both others anyway
    b->SendState(0x6F);
    c->SendState(0x6C);
    b->Send(MapPos(0x105, 40.5, 33.25, -1234));
    for (TestClient* other : { a.get(), c.get() }) {
        auto got = other->WaitFor("map_pos", s);
        CHECK(got.has_value());
        CHECK_EQ(GetInt(*got, "id"), (int64_t)b->id);
        CHECK_EQ(GetInt(*got, "map"), (int64_t)0x105);
        CHECK(std::abs(GetNumber(*got, "x") - 40.5) < 1e-6);
        CHECK(std::abs(GetNumber(*got, "y") - 33.25) < 1e-6);
        CHECK_EQ(GetInt(*got, "yaw"), (int64_t)-1234);
    }
    CHECK(!b->WaitFor("map_pos", s, 100).has_value()); // not back to its sender
    // on no map: only "map" -1
    b->Send(json{ { "t", "map_pos" }, { "map", -1 } });
    auto none = a->WaitFor("map_pos", s);
    CHECK(none.has_value());
    CHECK_EQ(GetInt(*none, "map", 7), (int64_t)-1);
    CHECK(none->find("x") == none->end());
}

TEST_CASE(MapPosThatIsNotOneIsDropped) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    b->Send(MapPos(0x10000, 1, 1, 0));   // not a map id
    b->Send(MapPos(-2, 1, 1, 0));
    b->Send(MapPos(0x105, 1e9, 1, 0));   // far out of any texture
    b->Send(MapPos(0x105, 1, -1e9, 0));
    b->Send(MapPos(0x105, 1, 1, 40000)); // not a yaw
    CHECK(!a->WaitFor("map_pos", s, 200).has_value());
    CHECK(!b->disconnected);
}

TEST_CASE(MapPosFloodSendsTheLatest) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    for (int i = 0; i < 60; i++) {
        b->Send(MapPos(0x105, i, 0, 0));
    }
    s.PumpFor(150);
    int received = 0;
    while (a->TakeEvent("map_pos").has_value()) {
        received++;
    }
    CHECK(received <= kMapPosBurst + 1);
    std::optional<json> last;
    CHECK(a->WaitUntil(s, 3000, [&] {
        if (auto e = a->TakeEvent("map_pos")) {
            last = e;
        }
        return last.has_value() && std::abs(GetNumber(*last, "x") - 59.0) < 1e-6;
    }));
    CHECK(!b->disconnected);
}
