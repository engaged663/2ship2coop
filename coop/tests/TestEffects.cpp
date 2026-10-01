// Effects echo (spec 2026-09-30-coop-grupos-limites §8): the packet format, then the server's relay to the scene.
#include "TestWorld.h"

#include "common/EffectImage.h"

using namespace coop;
using namespace coop_test;

namespace {

constexpr int16_t kScene = 0x40;
constexpr int16_t kOther = 0x41;

EffectRecord Effect(uint8_t type, std::vector<Slot> slots, uint8_t priority = 128) {
    EffectRecord r;
    r.type = type;
    r.priority = priority;
    r.slots = std::move(slots);
    return r;
}

std::vector<uint8_t> OnePacket(int16_t scene) {
    EffectPacket p;
    p.scene = scene;
    p.effects.push_back(Effect(0x00, { { SlotKind::Raw, 7 } }));
    return EncodeEffects(p)[0];
}

std::optional<std::vector<uint8_t>> WaitForEffects(TestClient& c, TestServer& s, int ms = 1000) {
    std::optional<std::vector<uint8_t>> found;
    c.WaitUntil(s, ms, [&] {
        for (size_t i = 0; i < c.rawStreams.size(); i++) {
            if (!c.rawStreams[i].empty() && c.rawStreams[i][0] == kStreamEffects) {
                found = c.rawStreams[i];
                c.rawStreams.erase(c.rawStreams.begin() + i);
                return true;
            }
        }
        return false;
    });
    return found;
}

// Alice and Bob in kScene, Carol in kOther, all in the server's world.
struct Trio {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c;
    explicit Trio(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        a->SendState(kScene, 0);
        b->SendState(kScene, 0);
        c->SendState(kOther, 0);
        s.PumpFor(80);
        Drain(s, { a.get(), b.get(), c.get() });
        b->rawStreams.clear();
        c->rawStreams.clear();
    }
};

} // namespace

TEST_CASE(EffectImageRoundTrip) {
    EffectPacket p;
    p.scene = kScene;
    p.cinema = true;
    p.effects.push_back(Effect(0x00, { { SlotKind::Raw, 0x4120000042C80000ull }, { SlotKind::Zero, 0 } }));
    p.effects.push_back(Effect(0x1D,
                               { { SlotKind::Actor, ActorRefValue(0x0103, 0, 0x10) }, { SlotKind::Exe, 0x1234 },
                                 { SlotKind::Link, LinkRefValue(2, 0x40) } },
                               64));
    auto packets = EncodeEffects(p);
    CHECK_EQ(packets.size(), (size_t)1);
    EffectPacket q;
    CHECK(DecodeEffects(packets[0].data(), packets[0].size(), q));
    CHECK_EQ(q.scene, kScene);
    CHECK(q.cinema);
    CHECK_EQ(q.effects.size(), (size_t)2);
    CHECK(q.effects[1].type == 0x1D && q.effects[1].priority == 64 && q.effects[1].slots.size() == 3);
    CHECK(q.effects[1].slots[0] == p.effects[1].slots[0]);
    CHECK(q.effects[1].slots[2] == p.effects[1].slots[2]);
    CHECK(q.effects[0].slots[0] == p.effects[0].slots[0]);
}

TEST_CASE(EffectImageSplitsAndRejectsGarbage) {
    EffectPacket big;
    big.scene = 1;
    for (int i = 0; i < 40; i++) {
        big.effects.push_back(
            Effect(0x05, std::vector<Slot>(effect_limits::kSlots, Slot{ SlotKind::Raw, (uint64_t)i + 1 })));
    }
    big.effects.push_back(Effect(0x30, {})); // not an effect of the game: dropped
    size_t total = 0;
    for (auto& pk : EncodeEffects(big)) {
        CHECK(pk.size() <= effect_limits::kPacketBytes);
        EffectPacket q;
        CHECK(DecodeEffects(pk.data(), pk.size(), q));
        total += q.effects.size();
    }
    CHECK_EQ(total, (size_t)40);
    EffectPacket one;
    one.scene = 3;
    one.effects.push_back(Effect(1, { { SlotKind::Zero, 0 } }));
    std::vector<uint8_t> good = EncodeEffects(one)[0];
    EffectPacket q;
    CHECK(DecodeEffects(good.data(), good.size(), q));
    CHECK(!DecodeEffects(good.data(), good.size() - 1, q)); // cut short
    std::vector<uint8_t> longer = good;
    longer.push_back(0);
    CHECK(!DecodeEffects(longer.data(), longer.size(), q)); // trailing bytes
    std::vector<uint8_t> wrongStream = good;
    wrongStream[0] = kStreamActors;
    CHECK(!DecodeEffects(wrongStream.data(), wrongStream.size(), q));
    std::vector<uint8_t> badType = good;
    badType[6] = 0x40; // the first effect's type
    CHECK(!DecodeEffects(badType.data(), badType.size(), q));
}

TEST_CASE(EffectsGoToTheSceneStamped) {
    Trio t;
    t.a->SendStream(OnePacket(kScene));
    auto got = WaitForEffects(*t.b, t.s);
    CHECK(got.has_value() && (*got)[1] == t.a->id);
    CHECK(!WaitForEffects(*t.c, t.s, 150).has_value());
    t.a->SendStream(OnePacket(kOther)); // not its scene
    t.a->SendStream({ kStreamEffects, 0, 0x40, 0 }); // garbage
    CHECK(!WaitForEffects(*t.b, t.s, 150).has_value());
}

TEST_CASE(EffectsOffRelaysNothing) {
    server::ServerConfig cfg;
    cfg.effects = false;
    Trio t(cfg);
    t.a->SendStream(OnePacket(kScene));
    CHECK(!WaitForEffects(*t.b, t.s, 150).has_value());
}

TEST_CASE(EffectsAreRateLimited) {
    Trio t;
    for (int i = 0; i < 200; i++) {
        t.a->SendStream(OnePacket(kScene));
    }
    t.s.PumpFor(300);
    int got = 0;
    while (WaitForEffects(*t.b, t.s, 20).has_value()) {
        got++;
    }
    CHECK(got > 0);
    CHECK(got < 120); // the burst (60) plus what refills meanwhile, never all of them
}
