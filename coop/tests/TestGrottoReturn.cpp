// The way out of a grotto travels with a teleport to a player in one (common/GrottoReturn.h).
#include "TestNet.h"

#include "common/GrottoReturn.h"

using namespace coop_test;

namespace {

coop::json SampleGrotto() {
    return { { "entrance", 0xD810 }, { "room", 3 },     { "pos", { 120.5, -20.0, 840.0 } },
             { "yaw", -16384 },      { "params", 0x2FF }, { "data", -77 } };
}

} // namespace

TEST_CASE(GrottoReturnRoundTrip) {
    coop::GrottoReturn g;
    CHECK(coop::GrottoReturnFromJson(SampleGrotto(), g));
    CHECK_EQ(g.entrance, (uint16_t)0xD810);
    CHECK_EQ(g.room, (int8_t)3);
    CHECK_EQ(g.pos[2], 840.f);
    CHECK_EQ(g.yaw, (int16_t)-16384);
    CHECK_EQ(g.playerParams, (uint16_t)0x2FF);
    CHECK_EQ(g.data, (int8_t)-77);
    coop::GrottoReturn back;
    CHECK(coop::GrottoReturnFromJson(coop::GrottoReturnToJson(g), back));
    CHECK_EQ(back.entrance, g.entrance);
    CHECK_EQ(back.pos[0], g.pos[0]);
    CHECK_EQ(back.data, g.data);
}

TEST_CASE(GrottoReturnRejectsImpossibleValues) {
    coop::GrottoReturn g;
    CHECK(!coop::GrottoReturnFromJson(coop::json(), g));
    CHECK(!coop::GrottoReturnFromJson(coop::json::array(), g));
    auto bad = [](const char* key, coop::json value) {
        coop::json j = SampleGrotto();
        j[key] = value;
        coop::GrottoReturn out;
        return !coop::GrottoReturnFromJson(j, out);
    };
    CHECK(bad("entrance", 0x6E << 9)); // a scene the game lacks
    CHECK(bad("entrance", -1));
    CHECK(bad("room", 128));
    CHECK(bad("pos", { 1.0, 2.0 }));
    CHECK(bad("pos", { 1.0, 2.0, 1.0e9 }));
    CHECK(bad("yaw", 40000));
    CHECK(bad("data", 200));
    CHECK(bad("params", "x"));
    coop::json missing = SampleGrotto();
    missing.erase("data");
    CHECK(!coop::GrottoReturnFromJson(missing, g));
}

TEST_CASE(TpToPlayerInGrottoCarriesTheWayOut) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    b->SendState(0x07, 3, 0x0E00, 10.f, 20.f, 30.f); // a grotto
    b->Send({ { "t", "loc" }, { "scene", 0x07 }, { "room", 3 }, { "entrance", 0x0E00 }, { "grotto", SampleGrotto() } });
    s.PumpFor(100);
    a->Cmd("/tp Bob");
    auto t = a->WaitFor("tp", s);
    CHECK(t.has_value());
    coop::GrottoReturn g;
    CHECK(t->contains("grotto") && coop::GrottoReturnFromJson((*t)["grotto"], g));
    CHECK_EQ(g.entrance, (uint16_t)0xD810);
    CHECK_EQ(g.data, (int8_t)-77);

    // Out of the grotto (a "loc" without it): a teleport to Bob is a plain one again
    b->SendState(0x6C, 0, 0xD800, 1.f, 2.f, 3.f);
    b->Send({ { "t", "loc" }, { "scene", 0x6C }, { "room", 0 }, { "entrance", 0xD800 } });
    s.PumpFor(100);
    a->Cmd("/tp Bob");
    auto t2 = a->WaitFor("tp", s);
    CHECK(t2.has_value() && !t2->contains("grotto"));
}

TEST_CASE(BadGrottoIsNotRelayed) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    coop::json bad = SampleGrotto();
    bad["room"] = 500;
    b->SendState(0x07, 3, 0x0E00, 10.f, 20.f, 30.f);
    b->Send({ { "t", "loc" }, { "scene", 0x07 }, { "room", 3 }, { "entrance", 0x0E00 }, { "grotto", bad } });
    s.PumpFor(100);
    a->Cmd("/tp Bob");
    auto t = a->WaitFor("tp", s);
    CHECK(t.has_value() && !t->contains("grotto"));
}
