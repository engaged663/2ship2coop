// Sub-project D3: the full-memory actor stream (ActorImage) and how a slot is classified.
#include "TestMain.h"

#include "common/ActorImage.h"
#include "common/PlayerState.h"

#include <cstring>
#include <limits>

using namespace coop;

namespace {

SlotSpan Span(uint8_t region, uint16_t first, std::vector<Slot> slots) {
    SlotSpan s;
    s.region = region;
    s.first = first;
    s.slots = std::move(slots);
    return s;
}

std::vector<Slot> Raws(int n, uint64_t base = 0x1111) {
    std::vector<Slot> out;
    for (int i = 0; i < n; i++) {
        out.push_back({ SlotKind::Raw, base + (uint64_t)i });
    }
    return out;
}

ActorImageRecord Rec(uint32_t key, std::vector<SlotSpan> spans) {
    ActorImageRecord r;
    r.key = key;
    r.actorId = 0x1A2;
    r.spans = std::move(spans);
    return r;
}

ActorImagePacket Frame() {
    ActorImagePacket f;
    f.scene = 0x2D;
    f.room = 3;
    f.seq = 777;
    return f;
}

std::vector<ActorImagePacket> DecodeAll(const std::vector<std::vector<uint8_t>>& packets) {
    std::vector<ActorImagePacket> out;
    for (const auto& p : packets) {
        ActorImagePacket d;
        CHECK(DecodeActorImage(p.data(), p.size(), d));
        out.push_back(d);
    }
    return out;
}

// The slots of one key across every packet, in order, as (region, slot index) -> slot.
std::vector<std::pair<std::pair<int, int>, Slot>> SlotsOf(const std::vector<ActorImagePacket>& packets, uint32_t key) {
    std::vector<std::pair<std::pair<int, int>, Slot>> out;
    for (const auto& p : packets) {
        for (const auto& r : p.records) {
            if (r.key != key) {
                continue;
            }
            for (const auto& s : r.spans) {
                for (size_t i = 0; i < s.slots.size(); i++) {
                    out.push_back({ { s.region, s.first + (int)i }, s.slots[i] });
                }
            }
        }
    }
    return out;
}

// A fake process: exe at 0x140000000 (1 MiB), one actor (key 7) at 0x20000, a Link at 0x30000, mapped 0x50000.
class FakeResolver : public PointerResolver {
  public:
    bool ExeOffset(uint64_t a, uint64_t& off) const override {
        if (a >= 0x140000000ull && a < 0x140100000ull) {
            off = a - 0x140000000ull;
            return true;
        }
        return false;
    }
    bool ActorRef(uint64_t a, uint64_t& v) const override {
        if (a >= 0x20000 && a < 0x20400) {
            v = ActorRefValue(7, 0, (uint16_t)(a - 0x20000));
            return true;
        }
        return false;
    }
    bool LinkRef(uint64_t a, uint64_t& v) const override {
        if (a >= 0x30000 && a < 0x31000) {
            v = LinkRefValue(2, (uint32_t)(a - 0x30000));
            return true;
        }
        return false;
    }
    bool IsMapped(uint64_t a) const override {
        return a >= 0x50000 && a < 0x60000;
    }
};

} // namespace

TEST_CASE(ActorImageRoundTrip) {
    ActorImagePacket f = Frame();
    f.hasAlive = true;
    f.alive = { 1, 2, 0x80010003u };
    f.gone = { 9 };
    ActorImageRecord a = Rec(2, { Span(0, 4, { { SlotKind::Raw, 0x0123456789ABCDEFull },
                                               { SlotKind::Zero, 0 },
                                               { SlotKind::Keep, 0 },
                                               { SlotKind::Exe, 0x00ABCDEF },
                                               { SlotKind::Actor, ActorRefValue(0x80010003u, 2, 0x1F8) },
                                               { SlotKind::Link, LinkRefValue(3, 0xA88) } }),
                                   Span(1, 0, Raws(3)) });
    a.acMask = 0x05;
    a.ocMask = 0x80;
    a.sfx = { 0x3812, 0x2903 };
    ActorImageRecord b = Rec(0x80010003u, { Span(0, 0, Raws(1)) });
    b.hasSpawn = true;
    b.spawn = { 0x0B5, -7, { 10.5f, -20.f, 3000.f }, { 1, -2, 0x4000 }, 2 };
    f.records = { a, b };

    auto packets = EncodeActorImage(f);
    CHECK_EQ(packets.size(), (size_t)1);
    auto d = DecodeAll(packets)[0];
    CHECK_EQ(d.scene, (int16_t)0x2D);
    CHECK_EQ(d.room, (int8_t)3);
    CHECK_EQ(d.seq, (uint16_t)777);
    CHECK_EQ(d.part, (uint8_t)0);
    CHECK_EQ(d.parts, (uint8_t)1);
    CHECK(d.hasAlive);
    CHECK(d.alive == f.alive);
    CHECK(d.gone == f.gone);
    CHECK_EQ(d.records.size(), (size_t)2);
    const auto& ra = d.records[0];
    CHECK_EQ(ra.key, 2u);
    CHECK_EQ(ra.actorId, (uint16_t)0x1A2);
    CHECK_EQ(ra.acMask, (uint8_t)0x05);
    CHECK_EQ(ra.ocMask, (uint8_t)0x80);
    CHECK(ra.sfx == a.sfx);
    CHECK(!ra.hasSpawn);
    CHECK_EQ(ra.spans.size(), (size_t)2);
    CHECK_EQ(ra.spans[0].first, (uint16_t)4);
    CHECK(ra.spans[0].slots == a.spans[0].slots);
    CHECK(ra.spans[1].slots == a.spans[1].slots);
    const auto& rb = d.records[1];
    CHECK(rb.hasSpawn);
    CHECK_EQ(rb.spawn.actorId, (uint16_t)0x0B5);
    CHECK_EQ(rb.spawn.params, (int16_t)-7);
    CHECK_EQ(rb.spawn.pos[2], 3000.f);
    CHECK_EQ(rb.spawn.rot[2], (int16_t)0x4000);
    CHECK_EQ(rb.spawn.parentKey, 2u);
}

TEST_CASE(ActorImageSplitsBigActors) {
    ActorImagePacket f = Frame();
    // A boss-sized instance (1000 slots = 8000 bytes of changes) and a few small ones.
    std::vector<SlotSpan> big;
    for (int first = 0; first < 1000; first += 250) {
        big.push_back(Span(0, (uint16_t)first, Raws(250, 0x1000 + first)));
    }
    ActorImageRecord boss = Rec(5, big);
    boss.hasSpawn = true;
    boss.sfx = { 0x1234 };
    f.records = { Rec(1, { Span(0, 0, Raws(10)) }), boss, Rec(6, { Span(0, 2, Raws(4)) }) };
    auto packets = EncodeActorImage(f);
    CHECK(packets.size() > 6);
    for (const auto& p : packets) {
        CHECK(p.size() <= image_limits::kPacketBytes);
    }
    auto decoded = DecodeAll(packets);
    for (size_t i = 0; i < decoded.size(); i++) {
        CHECK_EQ(decoded[i].part, (uint8_t)i);
        CHECK_EQ(decoded[i].parts, (uint8_t)decoded.size());
        CHECK_EQ(decoded[i].seq, (uint16_t)777);
    }
    auto slots = SlotsOf(decoded, 5);
    CHECK_EQ(slots.size(), (size_t)1000);
    for (size_t i = 0; i < slots.size(); i++) {
        CHECK_EQ(slots[i].first.second, (int)i);
        CHECK_EQ(slots[i].second.value, 0x1000 + (uint64_t)i);
    }
    int spawns = 0;
    int sfx = 0;
    for (const auto& p : decoded) {
        for (const auto& r : p.records) {
            if (r.key == 5) {
                spawns += r.hasSpawn ? 1 : 0;
                sfx += (int)r.sfx.size();
            }
        }
    }
    CHECK_EQ(spawns, 1); // only the first piece
    CHECK_EQ(sfx, 1);
    CHECK_EQ(SlotsOf(decoded, 1).size(), (size_t)10);
    CHECK_EQ(SlotsOf(decoded, 6).size(), (size_t)4);
}

TEST_CASE(ActorImageEmptyRecordsCarryMasks) {
    ActorImagePacket f = Frame();
    ActorImageRecord r = Rec(3, {});
    r.acMask = 1;
    f.records = { r };
    auto d = DecodeAll(EncodeActorImage(f))[0];
    CHECK_EQ(d.records.size(), (size_t)1);
    CHECK_EQ(d.records[0].acMask, (uint8_t)1);
    CHECK(d.records[0].spans.empty());
}

TEST_CASE(ActorImageNeverTooManyPackets) {
    ActorImagePacket f = Frame();
    for (uint32_t k = 1; k <= 200; k++) {
        f.records.push_back(Rec(k, { Span(0, 0, Raws(200)) }));
    }
    auto packets = EncodeActorImage(f);
    CHECK(packets.size() <= 32);
    CHECK(!packets.empty());
}

TEST_CASE(ActorImageRejectsGarbage) {
    ActorImagePacket f = Frame();
    f.hasAlive = true;
    f.alive = { 1 };
    f.records = { Rec(1, { Span(0, 4, { { SlotKind::Exe, 0x10 }, { SlotKind::Raw, 5 } }) }) };
    auto good = EncodeActorImage(f)[0];
    ActorImagePacket d;
    CHECK(DecodeActorImage(good.data(), good.size(), d));
    // Every truncation fails.
    for (size_t n = 0; n < good.size(); n++) {
        CHECK(!DecodeActorImage(good.data(), n, d));
    }
    // Trailing bytes fail.
    auto longer = good;
    longer.push_back(0);
    CHECK(!DecodeActorImage(longer.data(), longer.size(), d));
    // Wrong stream type.
    auto wrongType = good;
    wrongType[0] = 1;
    CHECK(!DecodeActorImage(wrongType.data(), wrongType.size(), d));
    // Impossible room.
    auto badRoom = good;
    badRoom[4] = 100;
    CHECK(!DecodeActorImage(badRoom.data(), badRoom.size(), d));
    // Part beyond parts.
    auto badPart = good;
    badPart[7] = 3;
    CHECK(!DecodeActorImage(badPart.data(), badPart.size(), d));
    // A span past the end of any region, a bad kind, a bad region.
    ActorImagePacket g = Frame();
    g.records = { Rec(1, { Span(0, 8190, Raws(4)) }) };
    auto pastEnd = EncodeActorImage(g)[0];
    CHECK(!DecodeActorImage(pastEnd.data(), pastEnd.size(), d));
    g.records = { Rec(1, { Span(0, 0, Raws(1)) }) };
    auto one = EncodeActorImage(g)[0];
    auto badKind = one;
    badKind[one.size() - 9] = 9; // the kind byte of the only slot
    CHECK(!DecodeActorImage(badKind.data(), badKind.size(), d));
    g.records = { Rec(1, { Span(16, 0, Raws(1)) }) };
    auto badRegion = EncodeActorImage(g);
    CHECK(badRegion.empty() || !DecodeActorImage(badRegion[0].data(), badRegion[0].size(), d));
    // A spawn position that is not a number.
    ActorImagePacket h = Frame();
    ActorImageRecord s = Rec(0x80000001u, {});
    s.hasSpawn = true;
    s.spawn.pos[0] = std::numeric_limits<float>::quiet_NaN();
    h.records = { s };
    auto nan = EncodeActorImage(h)[0];
    CHECK(!DecodeActorImage(nan.data(), nan.size(), d));
}

TEST_CASE(ActorImagePeekAndStamp) {
    auto p = EncodeActorImage(Frame())[0];
    int16_t scene = 0;
    int8_t room = 0;
    CHECK(PeekActorImageHeader(p.data(), p.size(), scene, room));
    CHECK_EQ(scene, (int16_t)0x2D);
    CHECK_EQ(room, (int8_t)3);
    CHECK(StampPlayerId(p.data(), p.size(), 4));
    ActorImagePacket d;
    CHECK(DecodeActorImage(p.data(), p.size(), d));
    CHECK_EQ(d.playerId, (uint8_t)4);
    CHECK(!PeekActorImageHeader(p.data(), 3, scene, room));
}

TEST_CASE(SlotClassification) {
    FakeResolver r;
    CHECK(ClassifySlot(0, r) == (Slot{ SlotKind::Zero, 0 }));
    CHECK(ClassifySlot(0x140000123ull, r) == (Slot{ SlotKind::Exe, 0x123 }));
    CHECK(ClassifySlot(0x20010, r) == (Slot{ SlotKind::Actor, ActorRefValue(7, 0, 0x10) }));
    CHECK(ClassifySlot(0x30A88, r) == (Slot{ SlotKind::Link, LinkRefValue(2, 0xA88) }));
    CHECK(ClassifySlot(0x50008, r) == (Slot{ SlotKind::Keep, 0 }));
    // Plain data: floats, small numbers, anything unmapped.
    uint64_t twoFloats = 0;
    float fl[2] = { 1.5f, -300.f };
    std::memcpy(&twoFloats, fl, 8);
    CHECK(ClassifySlot(twoFloats, r) == (Slot{ SlotKind::Raw, twoFloats }));
    CHECK(ClassifySlot(0x0000000500000003ull, r) == (Slot{ SlotKind::Raw, 0x0000000500000003ull }));
    CHECK(ClassifySlot(0x70000, r) == (Slot{ SlotKind::Raw, 0x70000 }));
}
