// Sincronización total: the common pieces (scene flags, ambient) and the server's relays (scene + layer, "sflag",
// "sflags", "ambient", the players' objects, the switches of server.json).
#include "TestWorld.h"

#include "common/ActorImage.h"
#include "common/Ambient.h"
#include "common/SceneFlags.h"

using namespace coop;
using namespace coop_test;

TEST_CASE(SceneFlagsDiffAndApply) {
    using namespace scene_flags;
    Words a{};
    Words b{};
    b[kSwitch2] = 0x5;
    b[kChest] = 0x80000000u;
    a[kCollect1] = 0x3;
    auto ops = Diff(a, b);
    CHECK_EQ(ops.size(), (size_t)3);
    Words c = a;
    for (const Op& op : ops) {
        Apply(c, op);
    }
    CHECK(c == b);
    CHECK(IsTemporary(kSwitch2) && IsTemporary(kSwitch3) && IsTemporary(kClearTemp) && IsTemporary(kCollect3));
    CHECK(!IsTemporary(kSwitch0) && !IsTemporary(kChest) && !IsTemporary(kClear) && !IsTemporary(kCollect0));
    Words t = TemporaryOf(b);
    CHECK_EQ(t[kSwitch2], 0x5u);
    CHECK_EQ(t[kChest], 0u);
}

TEST_CASE(SceneFlagOpsAreChecked) {
    using namespace scene_flags;
    std::vector<Op> ops;
    CHECK(OpsFromJson({ { "ops", { { 3, 1, 0 }, { 10, 0, 4294967295ll } } } }, ops));
    CHECK_EQ(ops.size(), (size_t)2);
    CHECK_EQ(ops[1].clear, 0xFFFFFFFFu);
    CHECK(OpsToJson(ops) == json({ { 3, 1, 0 }, { 10, 0, 4294967295ll } }));
    CHECK(!OpsFromJson({ { "ops", json::array() } }, ops));              // nothing
    CHECK(!OpsFromJson({ { "ops", { { 11, 1, 0 } } } }, ops));           // no such word
    CHECK(!OpsFromJson({ { "ops", { { 1, 1, 1 } } } }, ops));            // on and off at once
    CHECK(!OpsFromJson({ { "ops", { { 1, 0, 0 } } } }, ops));            // changes nothing
    CHECK(!OpsFromJson({ { "ops", { { 1, -1, 0 } } } }, ops));           // negative
    CHECK(!OpsFromJson({ { "ops", { { 1, 4294967296ll, 0 } } } }, ops)); // over 32 bits
    CHECK(!OpsFromJson({ { "ops", { { 1, 1.5, 0 } } } }, ops));          // not an integer
    CHECK(!OpsFromJson({ { "ops", { { 1, 1 } } } }, ops));               // short
    json twelve = json::array();
    for (int i = 0; i < 12; i++) {
        twelve.push_back({ i % 11, 1, 0 });
    }
    CHECK(!OpsFromJson({ { "ops", twelve } }, ops)); // more ops than words
    Words w{};
    CHECK(!WordsFromJson({ { "words", { 1, 2, 3 } } }, w));
    json eleven = json::array();
    for (int i = 0; i < 11; i++) {
        eleven.push_back(i);
    }
    CHECK(WordsFromJson({ { "words", eleven } }, w));
    CHECK_EQ(w[10], 10u);
    CHECK(WordsToJson(w) == eleven);
}

TEST_CASE(AmbientMusicIsChecked) {
    using namespace ambient;
    CHECK(SeqCmdAllowed(0x00000025u));  // play a sequence on the main BGM
    CHECK(SeqCmdAllowed(0x13000000u));  // stop the fanfare
    CHECK(SeqCmdAllowed(0x03000040u));  // the sub BGM
    CHECK(!SeqCmdAllowed(0x02000010u)); // the sound effects' player: never
    CHECK(!SeqCmdAllowed(0xE0000000u)); // a global command
    CHECK(!SeqCmdAllowed(0xF0000000u)); // resetting the audio heap
    std::vector<Music> music;
    CHECK(MusicFromJson(json::object(), music) && music.empty()); // missing: none
    CHECK(MusicFromJson({ { "music", { { 0, 0x25 }, { 1, 0x38 }, { 2, 0 }, { 3, 0x29 }, { 4, 0 }, { 5, 0x22 } } } },
                        music));
    CHECK_EQ(music.size(), (size_t)6);
    CHECK(music[1].kind == MusicKind::StorePrevBgm && music[1].value == 0x38);
    CHECK(MusicToJson(music) == json({ { 0, 0x25 }, { 1, 0x38 }, { 2, 0 }, { 3, 0x29 }, { 4, 0 }, { 5, 0x22 } }));
    CHECK(!MusicFromJson({ { "music", { { 6, 0 } } } }, music));          // no such kind
    CHECK(!MusicFromJson({ { "music", { { 0, 0x02000010 } } } }, music)); // a command that may not travel
    CHECK(!MusicFromJson({ { "music", { { 1, 0x10000 } } } }, music));    // not a sequence
    CHECK(!MusicFromJson({ { "music", { { 2, 5 } } } }, music));          // restoring takes no value
    json many = json::array();
    for (int i = 0; i < 17; i++) {
        many.push_back({ 4, 0 });
    }
    CHECK(!MusicFromJson({ { "music", many } }, music));
}

TEST_CASE(AmbientQuakesAreChecked) {
    using namespace ambient;
    std::vector<Quake> quakes;
    CHECK(QuakesFromJson(json::object(), quakes) && quakes.empty());
    CHECK(QuakesFromJson({ { "quake", { { 3, 20000, 4, 0, 0, 0, 12 } } } }, quakes));
    CHECK_EQ(quakes.size(), (size_t)1);
    CHECK_EQ(quakes[0].speed, (int16_t)20000);
    CHECK(QuakesToJson(quakes) == json({ { 3, 20000, 4, 0, 0, 0, 12 } }));
    CHECK(!QuakesFromJson({ { "quake", { { 0, 1, 1, 1, 1, 1, 10 } } } }, quakes));     // no type
    CHECK(!QuakesFromJson({ { "quake", { { 7, 1, 1, 1, 1, 1, 10 } } } }, quakes));     // no such type
    CHECK(!QuakesFromJson({ { "quake", { { 1, 1, 1, 1, 1, 1, 0 } } } }, quakes));      // lasts nothing
    CHECK(!QuakesFromJson({ { "quake", { { 1, 1, 1, 1, 1, 1, 5000 } } } }, quakes));   // too long
    CHECK(!QuakesFromJson({ { "quake", { { 1, 40000, 1, 1, 1, 1, 10 } } } }, quakes)); // not an s16
    CHECK(!QuakesFromJson({ { "quake", { { 1, 1, 1 } } } }, quakes));
}

// ---- Server ----

namespace {

constexpr int16_t kScene = 0x2D;
constexpr int16_t kOther = 0x2E;

// The latest "auth" a client got (older ones dropped).
std::optional<json> TakeLatestAuth(TestClient& c) {
    std::optional<json> last;
    while (auto ev = c.TakeEvent("auth")) {
        last = ev;
    }
    return last;
}

// Alice and Bob in kScene layer 0 (Alice arrived first), Carol in kScene layer 1, Dave in kOther. All in the world.
// authA/authC: the last "auth" Alice and Carol got while arriving.
struct Stages {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c, d;
    std::optional<json> authA, authC;
    explicit Stages(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        d = Join(s, "Dave");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        EnterWorld(s, *d);
        a->SendState(kScene, 0);
        s.PumpFor(30);
        b->SendState(kScene, 0);
        c->SendState(kScene, 0, 0, 0, 0, 0, 1);
        d->SendState(kOther, 0);
        s.PumpFor(300);
        authA = TakeLatestAuth(*a);
        authC = TakeLatestAuth(*c);
        Drain(s, { a.get(), b.get(), c.get(), d.get() });
        for (TestClient* x : { a.get(), b.get(), c.get(), d.get() }) {
            x->rawStreams.clear();
            x->streams.clear();
        }
    }
};

std::vector<uint8_t> ActorFrame(int16_t scene, int8_t room, uint32_t key = 0x0105) {
    ActorImagePacket p;
    p.scene = scene;
    p.room = room;
    p.seq = 1;
    ActorImageRecord r;
    r.key = key;
    r.actorId = 0x33;
    r.spans.push_back({ 0, 0, { { SlotKind::Raw, 1 } } });
    p.records.push_back(r);
    return EncodeActorImage(p)[0];
}

// A stream of that type arrived (the rest of what arrived is dropped). fromId: who the server stamped.
bool GotStream(TestServer& s, TestClient& c, uint8_t type, int ms = 300, uint8_t* fromId = nullptr) {
    bool got = c.WaitUntil(s, ms, [&] {
        for (const auto& r : c.rawStreams) {
            if (r.size() > 1 && r[0] == type) {
                if (fromId != nullptr) {
                    *fromId = r[1];
                }
                return true;
            }
        }
        return false;
    });
    c.rawStreams.clear();
    return got;
}

// The owner an "auth" names for a room (0: none).
uint8_t OwnerIn(const json& auth, int room) {
    for (const json& pair : auth["rooms"]) {
        if (pair[0].get<int>() == room) {
            return (uint8_t)pair[1].get<int>();
        }
    }
    return 0;
}

json SceneFlag(int16_t scene, json ops) {
    return { { "t", "sflag" }, { "scene", scene }, { "ops", std::move(ops) } };
}

json Arrival(int16_t scene, const scene_flags::Words& words) {
    return { { "t", "sflags" }, { "scene", scene }, { "words", scene_flags::WordsToJson(words) } };
}

json AmbientEv(int16_t scene, json music, json quake = json::array()) {
    return { { "t", "ambient" }, { "scene", scene }, { "room", 0 }, { "music", std::move(music) },
             { "quake", std::move(quake) } };
}

} // namespace

TEST_CASE(WelcomeCarriesTheSyncSwitches) {
    server::ServerConfig cfg;
    cfg.sounds = false;
    cfg.ocarina = false;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    const json& sync = a->welcome["sync"];
    CHECK(sync.is_object());
    CHECK(sync["sounds"] == false && sync["ocarina"] == false);
    CHECK(sync["ambient"] == true && sync["playerObjects"] == true && sync["sceneFlags"] == true &&
          sync["sceneObjects"] == true);
}

TEST_CASE(StagesAreSeparated) {
    Stages t;
    // Authority per scene + layer: Alice owns room 0 of layer 0, Carol (alone there) room 0 of layer 1
    CHECK(t.authA.has_value() && t.authC.has_value());
    CHECK_EQ(OwnerIn(*t.authA, 0), t.a->id);
    CHECK_EQ(OwnerIn(*t.authC, 0), t.c->id);
    CHECK_EQ(GetInt(*t.authC, "scene"), (int64_t)kScene); // the game compares it with its own scene id
    // Actors: Alice's frame reaches Bob, not Carol; Carol's reaches nobody of layer 0
    t.a->SendStream(ActorFrame(kScene, 0));
    CHECK(GotStream(t.s, *t.b, kStreamActors));
    CHECK(!GotStream(t.s, *t.c, kStreamActors, 150));
    t.c->SendStream(ActorFrame(kScene, 0));
    CHECK(!GotStream(t.s, *t.a, kStreamActors, 150));
    CHECK(!GotStream(t.s, *t.b, kStreamActors, 150));
    // Props: gone for layer 0 only
    t.a->Send({ { "t", "prop" }, { "scene", kScene }, { "key", 0x0103 }, { "child", -1 } });
    CHECK(t.b->WaitFor("prop", t.s).has_value());
    CHECK(!t.c->WaitFor("prop", t.s, 150).has_value());
    t.c->Send({ { "t", "props" }, { "scene", kScene }, { "layer", 1 } });
    auto listC = t.c->WaitFor("props", t.s);
    CHECK(listC.has_value() && (*listC)["list"].empty());
    t.b->Send({ { "t", "props" }, { "scene", kScene }, { "layer", 0 } });
    auto listB = t.b->WaitFor("props", t.s);
    CHECK(listB.has_value() && (*listB)["list"].size() == 1);
}

TEST_CASE(LocMovesAPlayerToAnotherLayer) {
    Stages t;
    t.c->Send({ { "t", "loc" }, { "scene", kScene }, { "room", 0 }, { "layer", 0 } });
    t.c->SendState(kScene, 0, 0, 0, 0, 0, 0); // and its poses say the same
    t.s.PumpFor(100);
    t.a->SendStream(ActorFrame(kScene, 0));
    CHECK(GotStream(t.s, *t.c, kStreamActors));
}

TEST_CASE(PlayerObjectsGoToTheStageFromAnyone) {
    Stages t;
    t.b->SendStream(ActorFrame(kScene, image_limits::kPlayerRoom, 0x82000001u)); // Bob owns no room
    uint8_t from = 0;
    CHECK(GotStream(t.s, *t.a, kStreamActors, 300, &from));
    CHECK_EQ(from, t.b->id);
    CHECK(!GotStream(t.s, *t.c, kStreamActors, 150)); // another layer
    CHECK(!GotStream(t.s, *t.d, kStreamActors, 150)); // another scene
    t.b->SendStream(ActorFrame(kOther, image_limits::kPlayerRoom)); // not its scene
    CHECK(!GotStream(t.s, *t.a, kStreamActors, 150));
}

TEST_CASE(PlayerObjectsOffRelaysNothing) {
    server::ServerConfig cfg;
    cfg.playerObjects = false;
    Stages t(cfg);
    t.b->SendStream(ActorFrame(kScene, image_limits::kPlayerRoom, 0x82000001u));
    CHECK(!GotStream(t.s, *t.a, kStreamActors, 150));
}

TEST_CASE(SceneFlagGoesToTheStageAndBackToItsSender) {
    Stages t;
    t.a->Send(SceneFlag(kScene, { { 3, 0x10, 0 } }));
    auto toA = t.a->WaitFor("sflag", t.s);
    auto toB = t.b->WaitFor("sflag", t.s);
    CHECK(toA.has_value() && toB.has_value());
    CHECK_EQ(GetInt(*toB, "from"), (int64_t)t.a->id);
    CHECK_EQ(GetInt(*toB, "scene"), (int64_t)kScene);
    CHECK((*toB)["ops"] == json({ { 3, 0x10, 0 } }));
    CHECK(!t.c->WaitFor("sflag", t.s, 150).has_value()); // another layer
    CHECK(!t.d->WaitFor("sflag", t.s, 150).has_value()); // another scene
    t.a->Send(SceneFlag(kOther, { { 3, 1, 0 } }));       // not its scene
    CHECK(!t.b->WaitFor("sflag", t.s, 150).has_value());
}

TEST_CASE(SceneFlagOrderIsTheServers) {
    Stages t;
    t.a->Send(SceneFlag(kScene, { { 1, 1, 0 } }));
    t.b->Send(SceneFlag(kScene, { { 1, 0, 1 } }));
    std::vector<int64_t> seenByA;
    std::vector<int64_t> seenByB;
    for (int i = 0; i < 2; i++) {
        auto x = t.a->WaitFor("sflag", t.s);
        auto y = t.b->WaitFor("sflag", t.s);
        CHECK(x.has_value() && y.has_value());
        seenByA.push_back(GetInt(*x, "from"));
        seenByB.push_back(GetInt(*y, "from"));
    }
    CHECK(seenByA == seenByB); // the same order for everyone: everyone ends with the same bit
}

TEST_CASE(SceneFlagsTemporaryWordsAreKeptForWhoArrives) {
    Stages t;
    scene_flags::Words found{};
    found[scene_flags::kSwitch2] = 0x4;  // Alice's scene as she found it
    found[scene_flags::kSwitch0] = 0xFF; // a cycle flag: never kept here
    t.a->Send(Arrival(kScene, found));
    CHECK(!t.a->WaitFor("sflags", t.s, 150).has_value()); // the first one there: its flags are the scene's
    t.a->Send(SceneFlag(kScene, { { scene_flags::kSwitch3, 0x8, 0 }, { scene_flags::kSwitch0, 0x100, 0 } }));
    t.s.PumpFor(50);
    t.b->Send(Arrival(kScene, scene_flags::Words{}));
    auto got = t.b->WaitFor("sflags", t.s);
    CHECK(got.has_value());
    scene_flags::Words kept{};
    CHECK(scene_flags::WordsFromJson(*got, kept));
    CHECK_EQ(kept[scene_flags::kSwitch2], 0x4u);
    CHECK_EQ(kept[scene_flags::kSwitch3], 0x8u);
    CHECK_EQ(kept[scene_flags::kSwitch0], 0u);
    t.c->Send(Arrival(kScene, scene_flags::Words{})); // layer 1 has its own (none yet)
    CHECK(!t.c->WaitFor("sflags", t.s, 150).has_value());
}

TEST_CASE(SceneFlagsAreForgottenWhenTheStageEmpties) {
    Stages t;
    scene_flags::Words found{};
    found[scene_flags::kSwitch2] = 0x4;
    t.a->Send(Arrival(kScene, found));
    t.s.PumpFor(50);
    t.a->SendState(kOther, 0);
    t.b->SendState(kOther, 0);
    t.s.PumpFor(100);
    t.b->SendState(kScene, 0);
    t.s.PumpFor(50);
    t.b->Send(Arrival(kScene, scene_flags::Words{}));
    CHECK(!t.b->WaitFor("sflags", t.s, 150).has_value()); // nothing kept: Bob's are the scene's now
}

TEST_CASE(SceneFlagsInvalidOrOff) {
    {
        Stages t;
        t.a->Send(SceneFlag(kScene, { { 1, 1, 1 } }));
        t.a->Send({ { "t", "sflags" }, { "scene", kScene }, { "words", { 1, 2 } } });
        CHECK(!t.b->WaitFor("sflag", t.s, 150).has_value());
    }
    {
        server::ServerConfig cfg;
        cfg.sceneFlags = false;
        Stages t(cfg);
        t.a->Send(SceneFlag(kScene, { { 3, 1, 0 } }));
        CHECK(!t.b->WaitFor("sflag", t.s, 150).has_value());
    }
}

TEST_CASE(AmbientGoesToTheStage) {
    Stages t;
    t.a->Send(AmbientEv(kScene, { { 0, 0x25 } }, { { 1, 0, 3, 0, 0, 0, 10 } }));
    auto got = t.b->WaitFor("ambient", t.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)t.a->id);
    CHECK_EQ(GetInt(*got, "room"), (int64_t)0);
    CHECK((*got)["music"] == json({ { 0, 0x25 } }));
    CHECK((*got)["quake"] == json({ { 1, 0, 3, 0, 0, 0, 10 } }));
    CHECK(!t.a->WaitFor("ambient", t.s, 100).has_value()); // not back to who made it
    CHECK(!t.c->WaitFor("ambient", t.s, 100).has_value()); // another layer
    CHECK(!t.d->WaitFor("ambient", t.s, 100).has_value()); // another scene
    t.a->Send(AmbientEv(kScene, { { 0, 0xF0000000u } }));  // never: resetting the audio heap
    t.a->Send(AmbientEv(kScene, json::array()));           // nothing in it
    CHECK(!t.b->WaitFor("ambient", t.s, 150).has_value());
}

TEST_CASE(AmbientOffRelaysNothing) {
    server::ServerConfig cfg;
    cfg.ambient = false;
    Stages t(cfg);
    t.a->Send(AmbientEv(kScene, { { 4, 0 } }));
    CHECK(!t.b->WaitFor("ambient", t.s, 150).has_value());
}

// The bug: rolling as a Goron or charging a spin attack for a while got the player kicked ("too many invalid
// packets") and its puppet stuttered: every pose of those frames carried a continuous sound (without its 0x800 bit)
// and was thrown away. 55 poses are more than the 50 invalid ones that kick.
TEST_CASE(PosesWithContinuousSoundsAreRelayed) {
    Stages t;
    int got = 0;
    for (int i = 0; i < 55; i++) {
        PlayerState st;
        st.sceneId = kScene;
        st.seq = (uint16_t)(1000 + i);
        st.sounds.push_back(MakeSound(0x08C1 - 0x800, 1.f, 1.f, 0, 4)); // NA_SE_PL_ROLL_DUST - SFX_FLAG
        st.sounds.push_back(MakeSound(0x1822 - 0x800, 1.5f, 1.f, 0, 4)); // NA_SE_IT_SWORD_CHARGE - SFX_FLAG
        t.a->SendStream(EncodePlayerState(st));
        t.s.PumpFor(40); // 25 a second: under the server's pose budget
        while (auto pose = (t.b->streams.empty() ? std::optional<PlayerState>() : t.b->streams.front())) {
            t.b->streams.pop_front();
            got += (pose->sounds.size() == 2 && pose->sounds[1].sfx == 0x1022) ? 1 : 0;
        }
    }
    t.s.PumpFor(100);
    got += (int)t.b->streams.size();
    CHECK(got >= 50);           // Bob saw (nearly) every one of them
    CHECK(!t.a->disconnected);  // and Alice is still here
    CHECK(!t.a->TakeEvent("kicked").has_value());
}

// Every game sends its Eponas' state every frame (an empty list too: it removes the others' copies of ours) besides
// its pose. Both shared one budget of 30 a second, so a quarter of the poses were dropped: every puppet stuttered.
TEST_CASE(EponaStreamNeverCostsPoses) {
    Stages t;
    EponaPacket none;
    none.sceneId = kScene;
    for (int i = 0; i < 45; i++) { // a moment with lots of horse packets
        t.a->SendEpona(none);
    }
    t.s.PumpFor(20);
    t.b->streams.clear();
    for (int i = 0; i < 40; i++) { // then 20 frames a second, each with its horse packet and its pose
        PlayerState st;
        st.sceneId = kScene;
        st.seq = (uint16_t)(2000 + i);
        t.a->SendEpona(none);
        t.a->SendStream(EncodePlayerState(st));
        t.s.PumpFor(50);
    }
    t.s.PumpFor(100);
    CHECK(t.b->streams.size() >= 38); // (nearly) every pose
}

// A horse packet of the scene a game is entering can arrive before the pose that tells the server about it: that is
// changing scene, never an invalid packet (they added up to a kick).
TEST_CASE(EponaFromAnotherSceneIsNotInvalid) {
    Stages t;
    EponaPacket elsewhere;
    elsewhere.sceneId = kOther;
    for (int i = 0; i < 60; i++) {
        t.a->SendEpona(elsewhere);
        t.s.PumpFor(20);
    }
    t.s.PumpFor(100);
    CHECK(!t.a->disconnected);
    CHECK(!t.a->TakeEvent("kicked").has_value());
}

TEST_CASE(PoseSoundsAndOcarinaFollowTheServer) {
    auto relayed = [](bool allow) {
        server::ServerConfig cfg;
        cfg.sounds = allow;
        cfg.ocarina = allow;
        Stages t(cfg);
        PlayerState st;
        st.sceneId = kScene;
        st.ocarinaInstrument = 1;
        st.ocarinaPitch = 2;
        st.sounds.push_back(MakeSound(0x6800, 1.f, 1.f, 0, 4));
        t.a->SendStream(EncodePlayerState(st));
        auto got = t.b->WaitForStream(t.s);
        CHECK(got.has_value());
        return got.value_or(PlayerState{});
    };
    PlayerState on = relayed(true);
    CHECK_EQ(on.sounds.size(), (size_t)1);
    CHECK_EQ(on.ocarinaInstrument, (uint8_t)1);
    PlayerState off = relayed(false);
    CHECK(off.sounds.empty());
    CHECK_EQ(off.ocarinaInstrument, (uint8_t)0);
    CHECK_EQ(off.ocarinaPitch, (uint8_t)0xFF);
}

// ---- A song played right ("song") ----

namespace {

json Song(int16_t scene, int song, int form = 4) {
    return { { "t", "song" }, { "scene", scene }, { "song", song }, { "form", form } };
}

} // namespace

// Everyone else in that scene hears it, whatever its layer (it is music, not actors).
TEST_CASE(SongGoesToTheScene) {
    Stages t;
    t.a->Send(Song(kScene, 8, 1)); // Epona's Song, as a Goron
    auto toB = t.b->WaitFor("song", t.s);
    auto toC = t.c->WaitFor("song", t.s);
    CHECK(toB.has_value() && toC.has_value());
    CHECK_EQ(GetInt(*toB, "from"), (int64_t)t.a->id);
    CHECK_EQ(GetInt(*toB, "scene"), (int64_t)kScene);
    CHECK_EQ(GetInt(*toB, "song"), (int64_t)8);
    CHECK_EQ(GetInt(*toB, "form"), (int64_t)1);
    CHECK(!t.a->WaitFor("song", t.s, 100).has_value()); // not back to who played it
    CHECK(!t.d->WaitFor("song", t.s, 100).has_value()); // another scene
    t.a->Send(Song(kOther, 6));                         // not its scene
    CHECK(!t.b->WaitFor("song", t.s, 150).has_value());
}

TEST_CASE(SongInvalidOrOff) {
    {
        Stages t;
        t.a->Send(Song(kScene, kMaxSong + 1));               // no music of its own (the Ballad of the Wind Fish...)
        t.a->Send(Song(kScene, 6, pose_limits::kFormCount)); // no such form
        t.a->Send({ { "t", "song" }, { "scene", kScene } }); // nothing to play
        CHECK(!t.b->WaitFor("song", t.s, 150).has_value());
    }
    {
        server::ServerConfig cfg;
        cfg.ocarina = false;
        Stages t(cfg);
        t.a->Send(Song(kScene, 6));
        CHECK(!t.b->WaitFor("song", t.s, 150).has_value());
    }
}

TEST_CASE(SongsAreRateLimited) {
    Stages t;
    for (int i = 0; i < 10; i++) {
        t.a->Send(Song(kScene, 6));
    }
    t.s.PumpFor(200);
    int n = 0;
    while (t.b->TakeEvent("song").has_value()) {
        n++;
    }
    CHECK(n >= 1 && n <= kSongBurst);
}
