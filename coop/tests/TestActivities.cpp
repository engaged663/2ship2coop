// Activities, shared dialogues and cutscenes (spec 2026-09-30-coop-grupos-actividades §3-§9): the server checks each
// event and passes it only to the group in the scene, or to the whole scene for bosses.
#include "TestWorld.h"

using namespace coop;
using namespace coop_test;

namespace {

constexpr int16_t kScene = 0x40;
constexpr int16_t kOther = 0x41;

// Alice and Bob in a group (when groups are on); Carol in the world too. All three in kScene.
struct Party {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c;
    explicit Party(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        a->SendState(kScene, 0);
        b->SendState(kScene, 0);
        c->SendState(kScene, 0);
        s.PumpFor(80);
        if (cfg.groups) {
            a->Cmd("/invitar Bob");
            if (!b->WaitFor("invite", s).has_value()) {
                Fail(__FILE__, __LINE__, "no invite");
            }
            b->Cmd("/aceptar");
            if (!b->WaitFor("group", s).has_value()) {
                Fail(__FILE__, __LINE__, "no group");
            }
        }
        Drain(s, { a.get(), b.get(), c.get() });
    }
};

json Act(const char* state, const char* key, const char* name = "") {
    return { { "t", "act" }, { "state", state }, { "key", key }, { "name", name } };
}

} // namespace

TEST_CASE(ActStartReachesOnlyTheGroup) {
    Party p;
    p.a->Send(Act("start", "galeria_ciudad", "Galería de tiro"));
    auto got = p.b->WaitFor("act", p.s);
    CHECK(got.has_value());
    CHECK_EQ(GetString(*got, "state"), std::string("start"));
    CHECK_EQ(GetString(*got, "key"), std::string("galeria_ciudad"));
    CHECK_EQ(GetString(*got, "nick"), std::string("Alice"));
    CHECK_EQ(GetInt(*got, "from"), (int64_t)p.a->id);
    CHECK(!p.c->WaitFor("act", p.s, 150).has_value());
    p.a->Send(Act("end", "galeria_ciudad"));
    auto end = p.b->WaitFor("act", p.s);
    CHECK(end.has_value() && GetString(*end, "state") == "end");
    p.a->Send(Act("start", "Bad Key!"));
    CHECK(!p.b->WaitFor("act", p.s, 150).has_value());
}

TEST_CASE(ListAndInvitationsShowTheActivity) {
    Party p;
    p.c->Send(Act("here", "cofres", "Cofres del tesoro"));
    p.s.PumpFor(40);
    p.a->Cmd("/list");
    auto list = p.a->WaitFor("sys", p.s);
    CHECK(list.has_value() && GetString(*list, "text").find("Cofres del tesoro") != std::string::npos);
    p.c->Cmd("/invitar Alice");
    auto inv = p.a->WaitFor("invite", p.s);
    CHECK(inv.has_value());
    CHECK_EQ(GetString(*inv, "name"), std::string("Cofres del tesoro"));
    CHECK_EQ(GetString(*inv, "activity"), std::string("cofres"));
    p.c->Send(Act("none", ""));
    p.s.PumpFor(40);
    Drain(p.s, { p.a.get() });
    p.a->Cmd("/list");
    auto again = p.a->WaitFor("sys", p.s);
    CHECK(again.has_value() && GetString(*again, "text").find("Cofres") == std::string::npos);
}

TEST_CASE(HudAndRewardsGoToTheGroupInTheScene) {
    Party p;
    json hud = { { "t", "act_hud" }, { "key", "galeria_ciudad" }, { "score", 12 }, { "pos", Arr(1.0, 2.0, 3.0) },
                 { "rot", 0 }, { "timer", { { "id", 1 }, { "down", true }, { "limit", 7500 }, { "elapsed", 100 } } } };
    p.a->Send(hud);
    auto got = p.b->WaitFor("act_hud", p.s);
    CHECK(got.has_value() && GetInt(*got, "score") == 12 && GetInt(*got, "from") == p.a->id);
    CHECK(!p.c->WaitFor("act_hud", p.s, 150).has_value());
    p.b->SendState(kOther, 0);
    p.s.PumpFor(60);
    p.a->Send(hud);
    CHECK(!p.b->WaitFor("act_hud", p.s, 150).has_value()); // another scene
    p.b->SendState(kScene, 0);
    p.s.PumpFor(60);
    json badHud = hud;
    badHud["timer"]["limit"] = -5;
    p.a->Send(badHud);
    CHECK(!p.b->WaitFor("act_hud", p.s, 150).has_value());
    p.a->Send({ { "t", "act_reward" }, { "gi", 5 } });
    auto r = p.b->WaitFor("act_reward", p.s);
    CHECK(r.has_value() && GetInt(*r, "gi") == 5 && GetString(*r, "nick") == "Alice");
    p.a->Send({ { "t", "act_reward" }, { "rupees", 50 } });
    auto rr = p.b->WaitFor("act_reward", p.s);
    CHECK(rr.has_value() && GetInt(*rr, "rupees") == 50);
    p.a->Send({ { "t", "act_reward" }, { "rupees", kMaxRewardRupees + 1 } });
    p.a->Send({ { "t", "act_reward" } });
    p.a->Send({ { "t", "act_reward" }, { "gi", 5 }, { "rupees", 5 } });
    CHECK(!p.b->WaitFor("act_reward", p.s, 150).has_value());
    CHECK(!p.c->TakeEvent("act_reward").has_value());
}

TEST_CASE(FollowTalkAndTitleGoToTheGroupOrTheScene) {
    Party p;
    p.a->Send({ { "t", "follow" }, { "entrance", 0x1234 }, { "cs", 0 }, { "trans", 2 }, { "key", "carrera_goron" } });
    auto f = p.b->WaitFor("follow", p.s);
    CHECK(f.has_value() && GetInt(*f, "entrance") == 0x1234);
    CHECK(!p.c->WaitFor("follow", p.s, 150).has_value());
    p.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 0x3E8 } });
    auto t = p.b->WaitFor("talk", p.s);
    CHECK(t.has_value() && GetInt(*t, "id") == 0x3E8 && GetInt(*t, "from") == p.a->id);
    CHECK(!p.c->WaitFor("talk", p.s, 150).has_value());
    p.a->Send({ { "t", "talk" }, { "op", "shout" } });
    CHECK(!p.b->WaitFor("talk", p.s, 150).has_value());
    json title = { { "t", "title" }, { "tex", "__OTR__objects/object_boss01/gOdolwaTitleCardTex" }, { "x", 160 },
                   { "y", 180 }, { "w", 128 }, { "h", 40 }, { "scope", "scene" } };
    p.a->Send(title);
    CHECK(p.b->WaitFor("title", p.s).has_value());
    CHECK(p.c->WaitFor("title", p.s).has_value());
    title["tex"] = "C:/windows/evil";
    p.a->Send(title);
    CHECK(!p.c->WaitFor("title", p.s, 150).has_value());
}

TEST_CASE(CinemaScopes) {
    Party p;
    json cam = { { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 },
                 { "roll", 0 } };
    p.a->Send(cam); // group (the default)
    CHECK(p.b->WaitFor("cinema", p.s).has_value());
    CHECK(!p.c->WaitFor("cinema", p.s, 150).has_value());
    cam["scope"] = "scene";
    p.a->Send(cam);
    CHECK(p.b->WaitFor("cinema", p.s).has_value());
    CHECK(p.c->WaitFor("cinema", p.s).has_value());
    cam["scope"] = "world";
    p.a->Send(cam);
    CHECK(!p.b->WaitFor("cinema", p.s, 150).has_value());
}

TEST_CASE(BossCutscenesOffKeepsTheSceneScopeHome) {
    server::ServerConfig cfg;
    cfg.bossCutscenes = false;
    Party p(cfg);
    json cam = { { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 },
                 { "scope", "scene" } };
    p.a->Send(cam);
    CHECK(!p.c->WaitFor("cinema", p.s, 150).has_value());
    cam.erase("scope");
    p.a->Send(cam); // the group still sees it
    CHECK(p.b->WaitFor("cinema", p.s).has_value());
}

TEST_CASE(GroupsOffRelaysNothingForTheGroup) {
    server::ServerConfig cfg;
    cfg.groups = false;
    Party p(cfg); // no group formed
    p.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 5 } });
    CHECK(!p.b->WaitFor("talk", p.s, 150).has_value());
    p.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 5 }, { "scope", "scene" } }); // bosses: still the scene
    CHECK(p.b->WaitFor("talk", p.s).has_value());
}

TEST_CASE(ActResultAndEndScoreReachTheGroup) {
    Party p;
    p.a->Send(Act("start", "carrera_goron", "Carrera goron"));
    CHECK(p.b->WaitFor("act", p.s).has_value());
    json result = { { "t", "act" }, { "state", "result" }, { "key", "carrera_goron" }, { "won", true }, { "cs", 8345 } };
    p.a->Send(result);
    auto r = p.b->WaitFor("act", p.s);
    CHECK(r.has_value() && GetString(*r, "state") == "result");
    CHECK(r.has_value() && GetBool(*r, "won") && GetInt(*r, "cs") == 8345);
    CHECK(r.has_value() && GetString(*r, "nick") == "Alice" && GetString(*r, "key") == "carrera_goron");
    CHECK(!p.c->WaitFor("act", p.s, 150).has_value());
    json end = Act("end", "carrera_goron");
    end["score"] = 25;
    end["cs"] = 1000;
    p.a->Send(end);
    auto e = p.b->WaitFor("act", p.s);
    CHECK(e.has_value() && GetString(*e, "state") == "end");
    CHECK(e.has_value() && GetInt(*e, "score") == 25 && GetInt(*e, "cs") == 1000);
    json badCs = result;
    badCs["cs"] = -1;
    p.a->Send(badCs);
    json noWon = result;
    noWon.erase("won");
    p.a->Send(noWon);
    CHECK(!p.b->WaitFor("act", p.s, 150).has_value());
}

TEST_CASE(TalkVarsAndCinemaBlurAreChecked) {
    Party p;
    p.a->Send({ { "t", "talk" }, { "op", "open" }, { "id", 5 }, { "vars", "00ff10" } });
    auto t = p.b->WaitFor("talk", p.s);
    CHECK(t.has_value() && GetString(*t, "vars") == "00ff10");
    p.a->Send({ { "t", "talk" }, { "op", "id" }, { "id", 6 }, { "vars", "0g" } });                       // not hex
    p.a->Send({ { "t", "talk" }, { "op", "id" }, { "id", 6 }, { "vars", "abc" } });                      // odd
    p.a->Send({ { "t", "talk" }, { "op", "id" }, { "id", 6 }, { "vars", std::string(514, 'a') } });      // too long
    CHECK(!p.b->WaitFor("talk", p.s, 150).has_value());
    json cam = { { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 },
                 { "blur", 120 } };
    p.a->Send(cam);
    auto c = p.b->WaitFor("cinema", p.s);
    CHECK(c.has_value() && GetInt(*c, "blur") == 120);
    cam["blur"] = 300;
    p.a->Send(cam);
    CHECK(!p.b->WaitFor("cinema", p.s, 150).has_value());
}
