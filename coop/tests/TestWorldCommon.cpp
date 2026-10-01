// Shared-world building blocks (sub-project B): hex, the schema, change ops and the clock math.
#include "TestMain.h"

#include "common/Clock.h"
#include "common/Events.h"
#include "common/Hex.h"
#include "common/Protocol.h"
#include "common/WorldFields.h"
#include "common/WorldOps.h"
#include "common/WorldRules.h"

#include <string>
#include <vector>

using namespace coop;

TEST_CASE(HexRoundTrip) {
    std::vector<uint8_t> bytes = { 0x00, 0x0f, 0xa5, 0xff };
    CHECK_EQ(ToHex(bytes), std::string("000fa5ff"));
    std::vector<uint8_t> back;
    CHECK(FromHex("000FA5ff", back));
    CHECK(back == bytes);
    CHECK(!FromHex("abc", back)); // odd length
    CHECK(!FromHex("zz", back));  // not hex
    CHECK(back == bytes);         // untouched on failure
}

TEST_CASE(SchemaNamesAreUniqueAndFound) {
    for (size_t i = 0; i < world::kFieldCount; i++) {
        CHECK_EQ(world::FindField(world::kFields[i].name), (int)i);
        CHECK(world::kFields[i].size > 0);
    }
    CHECK_EQ(world::FindField("nope"), -1);
}

TEST_CASE(DiffAndApplyBitsKeepOtherPlayersBits) {
    using namespace world;
    uint16_t f = (uint16_t)FindField("weekEventReg");
    std::vector<uint8_t> before(kFields[f].size, 0);
    std::vector<uint8_t> after = before;
    before[3] = 0b1010;
    after[3] = 0b0110; // one bit set, one cleared
    after[99] = 0x80;
    Ops ops;
    Diff(f, before.data(), after.data(), ops);
    CHECK_EQ(ops.bits.size(), (size_t)2);
    // Applied where another player set another bit meanwhile: that bit survives.
    std::vector<uint8_t> other = before;
    other[3] |= 0b0001;
    bool changed = false;
    for (const BitsOp& op : ops.bits) {
        CHECK(ApplyBits(other.data(), op, &changed));
    }
    CHECK_EQ(other[3], (uint8_t)0b0111);
    CHECK_EQ(other[99], (uint8_t)0x80);
}

TEST_CASE(DiffAndApplyBytesAndCounters) {
    using namespace world;
    uint16_t items = (uint16_t)FindField("items");
    std::vector<uint8_t> a(kFields[items].size, 0xFF);
    std::vector<uint8_t> b = a;
    b[5] = 0x2C;
    Ops ops;
    Diff(items, a.data(), b.data(), ops);
    CHECK_EQ(ops.bytes.size(), (size_t)1);
    bool changed = false;
    CHECK(ApplyByte(a.data(), ops.bytes[0], &changed));
    CHECK(changed);
    CHECK_EQ(a[5], (uint8_t)0x2C);

    uint16_t skulls = (uint16_t)FindField("skulls");
    std::vector<uint8_t> s0(4, 0);
    std::vector<uint8_t> s1(4, 0);
    s1[2] = 3; // second counter (bytes 2-3) +3
    Ops adds;
    Diff(skulls, s0.data(), s1.data(), adds);
    CHECK_EQ(adds.adds.size(), (size_t)1);
    CHECK_EQ(adds.adds[0].offset, (uint16_t)2);
    CHECK_EQ(adds.adds[0].delta, 3);
}

TEST_CASE(CountersClampToTheirType) {
    using namespace world;
    int32_t applied = 0;
    uint16_t keys = (uint16_t)FindField("keys"); // s8, -1 = no keys yet
    std::vector<uint8_t> k(kFields[keys].size, 0xFF);
    CHECK(ApplyAdd(k.data(), { keys, 0, 2 }, &applied));
    CHECK_EQ((int8_t)k[0], (int8_t)1);
    CHECK(ApplyAdd(k.data(), { keys, 0, 500 }, &applied));
    CHECK_EQ((int8_t)k[0], (int8_t)127);
    CHECK_EQ(applied, 126);
    uint16_t bottles = (uint16_t)FindField("bottles"); // u8
    std::vector<uint8_t> b(1, 1);
    CHECK(ApplyAdd(b.data(), { bottles, 0, -5 }, &applied));
    CHECK_EQ(b[0], (uint8_t)0);
    CHECK_EQ(applied, -1);
    uint16_t hearts = (uint16_t)FindField("heartQuarters"); // u16
    std::vector<uint8_t> h = { 0xFF, 0xFF };
    CHECK(ApplyAdd(h.data(), { hearts, 0, 1 }, &applied));
    CHECK_EQ(applied, 0);
}

TEST_CASE(OpsOutsideTheSchemaAreRejected) {
    using namespace world;
    std::vector<uint8_t> big(4096, 0);
    bool changed = false;
    int32_t applied = 0;
    uint16_t week = (uint16_t)FindField("weekEventReg");
    uint16_t items = (uint16_t)FindField("items");
    uint16_t skulls = (uint16_t)FindField("skulls");
    CHECK(!ApplyBits(big.data(), { (uint16_t)kFieldCount, 0, 1, 0 }, &changed)); // unknown field
    CHECK(!ApplyBits(big.data(), { week, 100, 1, 0 }, &changed));                // past the end
    CHECK(!ApplyBits(big.data(), { items, 0, 1, 0 }, &changed));                 // wrong kind
    CHECK(!ApplyByte(big.data(), { week, 0, 1 }, &changed));                      // wrong kind
    CHECK(!ApplyAdd(big.data(), { week, 0, 1 }, &applied));                       // not a counter
    CHECK(!ApplyAdd(big.data(), { skulls, 1, 1 }, &applied));                     // misaligned u16
    CHECK(!ApplyAdd(big.data(), { skulls, 4, 1 }, &applied));                     // past the end
    CHECK(!ApplyAdd(big.data(), { skulls, 0, 70000 }, &applied));                 // absurd delta
}

TEST_CASE(OpsJsonRoundTripAndValidation) {
    using namespace world;
    Ops ops;
    ops.bits.push_back({ 0, 3, 0x10, 0x01 });
    ops.bytes.push_back({ 12, 5, 0x2C });
    ops.adds.push_back({ 24, 2, -3 });
    json j = ToJson(ops);
    Ops back;
    CHECK(FromJson(j, back, nullptr));
    CHECK_EQ(back.Count(), (size_t)3);
    CHECK_EQ(back.bits[0].set, (uint8_t)0x10);
    CHECK_EQ(back.bytes[0].value, (uint8_t)0x2C);
    CHECK_EQ(back.adds[0].delta, -3);
    std::string err;
    json notByte = { { "bits", json::array({ json::array({ 0, 3, 256, 0 }) }) } };
    CHECK(!FromJson(notByte, back, &err));
    json tooShort = { { "bytes", json::array({ json::array({ 0, 3 }) }) } };
    CHECK(!FromJson(tooShort, back, &err));
    json notList = { { "adds", "x" } };
    CHECK(!FromJson(notList, back, &err));
    CHECK(!err.empty());
    CHECK(ToJson(Ops{}).empty());
}

TEST_CASE(ClockConversions) {
    using namespace clock;
    CHECK_EQ(TimeOfAbs(0), (uint16_t)0x4000); // Day 1, 6:00
    CHECK_EQ(DayOfAbs(0), 1);
    CHECK_EQ(DayOfAbs(kDayUnits), 2);
    CHECK_EQ(DayOfAbs(kMoonAbs), 4);
    CHECK_EQ(AbsOf(1, 0x4000), 0u);
    CHECK_EQ(AbsOf(1, 0x3FFF), 0xFFFFu); // 5:59 at the end of the first night
    CHECK_EQ(AbsOf(2, 0x4000), kDayUnits);
    for (uint32_t abs : { 0u, 12345u, 0x2ABCDu, kMoonAbs - 1 }) {
        CHECK_EQ(AbsOf(DayOfAbs(abs), TimeOfAbs(abs)), abs);
    }
    CHECK(IsNight(0xC000));
    CHECK(!IsNight(0xBFFF));
    CHECK(IsNight(0x3FFF));
    CHECK(!IsNight(0x4000));
    CHECK_EQ(NextHalfDay(0), kHalfDayUnits);
    CHECK_EQ(NextHalfDay(kHalfDayUnits - 1), kHalfDayUnits);
    CHECK_EQ(NextHalfDay(kHalfDayUnits), kDayUnits);
    CHECK_EQ(Format(0), std::string("Día 1, 06:00"));
    uint32_t abs = 0;
    CHECK(Parse(2, "14:35", abs));
    CHECK_EQ(Format(abs), std::string("Día 2, 14:35"));
    CHECK(Parse(3, "05:59", abs));
    CHECK(abs < kMoonAbs && abs > kMoonAbs - 60);
    CHECK(Parse(1, "03:00", abs)); // after midnight still belongs to the first day
    CHECK_EQ(DayOfAbs(abs), 1);
    CHECK(!Parse(4, "10:00", abs));
    CHECK(!Parse(1, "24:00", abs));
    CHECK(!Parse(1, "10-00", abs));
    CHECK(!Parse(1, "", abs));
}

TEST_CASE(ClockFollowTheServer) {
    using namespace clock;
    Follow f = FollowServer(1000, 1000, false);
    CHECK(!f.freeze && !f.write);
    f = FollowServer(1000, 1003, false); // within 2 frames: the game's own clock is fine
    CHECK(!f.write);
    f = FollowServer(1000, 1300, false); // behind (a pause, slow frames): jump forward
    CHECK(f.write && f.writeAbs == 1300 && !f.freeze && !f.backwards);
    f = FollowServer(1300, 1000, false); // ahead by 5 s: wait for the server
    CHECK(f.freeze && !f.write);
    f = FollowServer(1030, 1000, false); // slightly ahead: keep running
    CHECK(!f.freeze && !f.write);
    f = FollowServer(5000, 1000, false); // far ahead (a lost jump, a restarted server): go back
    CHECK(f.write && f.backwards && f.writeAbs == 1000 && !f.freeze);
    f = FollowServer(1000, 1500, true);  // server stopped and ahead: catch up, then stop
    CHECK(f.freeze && f.write && f.writeAbs == 1500 && !f.backwards);
    f = FollowServer(2000, 1500, true);
    CHECK(f.freeze && !f.write);
    f = FollowServer(3000, 1500, true);
    CHECK(f.freeze && f.write && f.backwards && f.writeAbs == 1500);
}

TEST_CASE(ClockFollowStopsBeforeTheMoon) {
    using namespace clock;
    uint32_t limit = kMoonAbs - kClientMoonMargin;
    Follow f = FollowServer(limit - 10, kMoonAbs, false);
    CHECK(f.write && f.writeAbs == limit);
    f = FollowServer(limit, kMoonAbs, false);
    CHECK(f.freeze && !f.write);
    f = FollowServer(limit + 3, kMoonAbs, false);
    CHECK(f.freeze && f.write && f.backwards && f.writeAbs == limit);
}

TEST_CASE(EventSizeLimitsDependOnDirection) {
    json ev = { { "t", "x" }, { "pad", std::string(20000, 'a') } };
    std::string text = SerializeEvent(ev);
    json out;
    CHECK(!ParseEvent((const uint8_t*)text.data(), text.size(), out, nullptr));                     // server side
    CHECK(ParseEvent((const uint8_t*)text.data(), text.size(), out, nullptr, kMaxServerEventBytes)); // game side
    CHECK(GetBool(json{ { "b", true } }, "b"));
    CHECK(!GetBool(json{ { "b", 1 } }, "b"));
    CHECK(GetBool(json{ { "b", 1 } }, "c", true));
    CHECK_EQ(kProtocolVersion, 12u); // v12: effects, results, talk values; v11: groups and activities
}

TEST_CASE(StolenSwordsComeBackToTheWorld) {
    // The world computed by another game after Takkuri stole them: Kokiri Sword + Mirror Shield, no Great
    // Fairy's Sword. The robbed player had the Gilded Sword and the Great Fairy's Sword in stolenItems.
    uint8_t equipment[2] = { 0x21, 0x00 };
    std::vector<uint8_t> items(18, 0xFF);
    CHECK(world::ReturnStolenSwords(0x4F100000u, equipment, items.data()));
    CHECK_EQ(equipment[0], (uint8_t)0x23); // Gilded Sword, the shield kept
    CHECK_EQ(items[world::kSlotSwordGreatFairy], world::kItemSwordGreatFairy);
    CHECK(!world::ReturnStolenSwords(0x4F100000u, equipment, items.data())); // already back
    // The Kokiri or Razor Sword and bottles come back with the rules of any game: nothing to do here.
    uint8_t kokiri[2] = { 0x11, 0x00 };
    std::vector<uint8_t> none(18, 0xFF);
    CHECK(!world::ReturnStolenSwords(0x4E120000u, kokiri, none.data()));
    CHECK_EQ(kokiri[0], (uint8_t)0x11);
    CHECK(!world::ReturnStolenSwords(0, kokiri, none.data()));
    CHECK_EQ(none[world::kSlotSwordGreatFairy], (uint8_t)0xFF);
    // A sword better than a Razor Sword stays as it is (the rules only replace a Razor or Kokiri Sword, or none).
    uint8_t gilded[2] = { 0x13, 0x00 };
    CHECK(!world::ReturnStolenSwords(0x004F0000u, gilded, none.data()));
    // The second stolen item counts too.
    uint8_t second[2] = { 0x10, 0x00 };
    CHECK(world::ReturnStolenSwords(0x124F0000u, second, none.data()));
    CHECK_EQ(second[0], (uint8_t)0x13);
}
