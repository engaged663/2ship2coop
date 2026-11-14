// Players' colors (common/PlayerColor.h) and their trip through the server (hello, welcome, join, "color").
#include "TestNet.h"

#include "common/PlayerColor.h"

#include <cstdlib>

using namespace coop;
using namespace coop_test;

namespace {

PlayerColor Sample() {
    PlayerColor c;
    c.main = { 0x12, 0x34, 0x56 };
    c.second = { 0xAB, 0xCD, 0xEF };
    c.fx = ColorFx::Blend;
    c.speed = ColorSpeed::Fast;
    return c;
}

std::unique_ptr<TestClient> JoinWithColor(TestServer& s, const std::string& nick, int64_t color) {
    auto c = Connect(s);
    c->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", nick }, { "pass", "" }, { "color", color } });
    auto w = c->WaitFor("welcome", s);
    if (w.has_value()) {
        c->welcome = *w;
        c->id = (uint8_t)GetInt(*w, "id");
    }
    return c;
}

} // namespace

TEST_CASE(PlayerColorPacksAndUnpacks) {
    PlayerColor c = Sample();
    int64_t packed = PackPlayerColor(c);
    CHECK_EQ(packed & 0xFFFFFF, (int64_t)0x123456);
    CHECK_EQ((packed >> 24) & 0xFFFFFF, (int64_t)0xABCDEF);
    PlayerColor back;
    CHECK(UnpackPlayerColor(packed, back));
    CHECK(back == c);
    // every effect and speed survives the trip
    for (int fx = 0; fx < (int)ColorFx::Count; fx++) {
        for (int sp = 0; sp < (int)ColorSpeed::Count; sp++) {
            c.fx = (ColorFx)fx;
            c.speed = (ColorSpeed)sp;
            CHECK(UnpackPlayerColor(PackPlayerColor(c), back));
            CHECK(back == c);
        }
    }
    // fits in a double too (a Lua mod or a JSON tool reading it keeps it exact)
    CHECK(PackPlayerColor(c) < (int64_t(1) << 53));
}

TEST_CASE(PlayerColorRejectsWhatIsNotAColor) {
    PlayerColor out = Sample();
    CHECK(!UnpackPlayerColor(kNoColor, out));
    CHECK(out == Sample()); // untouched
    CHECK(!UnpackPlayerColor(-5, out));
    CHECK(!UnpackPlayerColor((int64_t)ColorFx::Count << 48, out)); // unknown effect
    CHECK(!UnpackPlayerColor((int64_t)3 << 51, out));              // unknown speed
    CHECK(!UnpackPlayerColor(int64_t(1) << 53, out));              // a bit nobody uses
    CHECK(IsValidPackedColor(kNoColor));
    CHECK(IsValidPackedColor(0));
    CHECK(IsValidPackedColor(PackPlayerColor(Sample())));
    CHECK(!IsValidPackedColor(int64_t(1) << 60));
}

TEST_CASE(PlayerColorDefaultsTellFourPlayersApart) {
    for (uint8_t a = 1; a <= kMaxPlayers + kMaxHosts; a++) {
        for (uint8_t b = a + 1; b <= kMaxPlayers + kMaxHosts; b++) {
            CHECK(DefaultPlayerColor(a).main != DefaultPlayerColor(b).main);
        }
        // never the yellow-green of our own arrow on the minimap
        CHECK(DefaultPlayerColor(a).main != (Rgb{ 200, 255, 0 }));
        CHECK(DefaultPlayerColor(a).fx == ColorFx::Solid);
    }
    CHECK(DefaultPlayerColor(1).main == PlayerColorPalette()[0]);
    CHECK(PlayerColorPalette().size() >= 16);
    // a player who never picked one shows its id's; a chosen one wins
    CHECK(ResolvePlayerColor(kNoColor, 2) == DefaultPlayerColor(2));
    CHECK(ResolvePlayerColor(int64_t(1) << 60, 3) == DefaultPlayerColor(3));
    CHECK(ResolvePlayerColor(PackPlayerColor(Sample()), 2) == Sample());
}

TEST_CASE(PlayerColorEffectsMoveWithTime) {
    PlayerColor c;
    c.main = { 255, 0, 0 };
    c.second = { 0, 0, 255 };
    // solid: always the main color
    CHECK(ShownPlayerColor(c, 0.0) == c.main);
    CHECK(ShownPlayerColor(c, 12.34) == c.main);
    // two colors: starts on the main one, halfway through the cycle on the second (Normal = 1.6 s)
    c.fx = ColorFx::Blend;
    CHECK(ShownPlayerColor(c, 0.0) == c.main);
    CHECK(ShownPlayerColor(c, 0.8) == c.second);
    Rgb mid = ShownPlayerColor(c, 0.4);
    CHECK(mid.r > 100 && mid.r < 160 && mid.b > 100 && mid.b < 160);
    // pulse: darker halfway, never another hue
    c.fx = ColorFx::Pulse;
    Rgb dark = ShownPlayerColor(c, 0.8);
    CHECK(dark.r < 120 && dark.g == 0 && dark.b == 0);
    // blink: main in the first half of the cycle, second in the other
    c.fx = ColorFx::Blink;
    CHECK(ShownPlayerColor(c, 0.1) == c.main);
    CHECK(ShownPlayerColor(c, 1.0) == c.second);
    // faster = shorter cycle
    c.speed = ColorSpeed::Fast;
    CHECK(ShownPlayerColor(c, 0.5) == c.second);
    // rainbow: starts at the main color's hue and goes through others; a gray still gets colors
    c.fx = ColorFx::Rainbow;
    c.speed = ColorSpeed::Normal;
    CHECK(ShownPlayerColor(c, 0.0) == c.main);
    CHECK(ShownPlayerColor(c, 1.0) != c.main);
    c.main = { 128, 128, 128 };
    Rgb r = ShownPlayerColor(c, 0.0);
    CHECK(!(r.r == r.g && r.g == r.b));
}

TEST_CASE(PlayerColorHsvRoundTrip) {
    for (const Rgb& c : PlayerColorPalette()) {
        float h, s, v;
        RgbToHsv(c, h, s, v);
        Rgb back = HsvToRgb(h, s, v);
        CHECK(std::abs(back.r - c.r) <= 1 && std::abs(back.g - c.g) <= 1 && std::abs(back.b - c.b) <= 1);
    }
    CHECK(RgbFromInt(0xFF8000) == (Rgb{ 255, 128, 0 }));
    CHECK_EQ(RgbToInt(Rgb{ 255, 128, 0 }), 0xFF8000u);
}

TEST_CASE(ColorTravelsInHelloWelcomeAndJoin) {
    TestServer s;
    int64_t aliceColor = PackPlayerColor(Sample());
    auto a = JoinWithColor(s, "Alice", aliceColor);
    auto b = Join(s, "Bob"); // sends no color
    auto j = a->WaitFor("join", s);
    CHECK(j.has_value());
    CHECK_EQ(GetInt(*j, "color", 7), kNoColor);
    CHECK_EQ(b->welcome["players"].size(), (size_t)1);
    CHECK_EQ(GetInt(b->welcome["players"][0], "color", 7), aliceColor);
    // a wrong color in the hello is not a reason to refuse anyone: it is just "none"
    auto c = JoinWithColor(s, "Carol", int64_t(1) << 60);
    CHECK(c->id != 0);
    auto jc = a->WaitFor("join", s);
    CHECK(jc.has_value());
    CHECK_EQ(GetInt(*jc, "color", 7), kNoColor);
}

TEST_CASE(ColorChangeGoesToTheOthers) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    int64_t color = PackPlayerColor(Sample());
    b->Send({ { "t", "color" }, { "color", color } });
    auto got = a->WaitFor("color", s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "id"), (int64_t)b->id);
    CHECK_EQ(GetInt(*got, "color"), color);
    CHECK(!b->WaitFor("color", s, 100).has_value()); // not echoed to its sender
    // whoever arrives later learns it in the welcome
    auto c = Join(s, "Carol");
    bool found = false;
    for (const auto& p : c->welcome["players"]) {
        if (GetInt(p, "id") == b->id) {
            found = GetInt(p, "color") == color;
        }
    }
    CHECK(found);
    // back to "none"
    b->Send({ { "t", "color" }, { "color", kNoColor } });
    auto none = a->WaitFor("color", s);
    CHECK(none.has_value());
    CHECK_EQ(GetInt(*none, "color", 7), kNoColor);
}

TEST_CASE(ColorFloodSendsOnlyTheLatest) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    PlayerColor c;
    for (int i = 0; i < 40; i++) {
        c.main = { (uint8_t)i, 0, 0 };
        b->Send({ { "t", "color" }, { "color", PackPlayerColor(c) } });
    }
    s.PumpFor(150);
    int received = 0;
    while (a->TakeEvent("color").has_value()) {
        received++;
    }
    CHECK(received <= kColorBurst + 1);
    // the last one still arrives once the budget refills
    std::optional<json> last;
    CHECK(a->WaitUntil(s, 3000, [&] {
        auto e = a->TakeEvent("color");
        if (e.has_value()) {
            last = e;
        }
        return last.has_value() && GetInt(*last, "color") == PackPlayerColor(c);
    }));
    CHECK(!b->disconnected);
}

TEST_CASE(ColorThatIsNotOneIsIgnored) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(a->WaitFor("join", s).has_value());
    b->Send({ { "t", "color" }, { "color", int64_t(1) << 60 } });
    b->Send({ { "t", "color" }, { "color", "rojo" } });
    CHECK(!a->WaitFor("color", s, 200).has_value());
    CHECK(!b->disconnected);
}
