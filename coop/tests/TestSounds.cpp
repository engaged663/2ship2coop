// Sincronización total §2: one sound on the wire (SoundEntry), alone and in lists.
#include "TestMain.h"

#include "common/ByteStream.h"
#include "common/SoundEntry.h"

using namespace coop;

namespace {

std::vector<uint8_t> Bytes(const SoundEntry& s) {
    Writer w;
    WriteSound(w, s);
    return w.Take();
}

bool ReadBack(const std::vector<uint8_t>& b, SoundEntry& out) {
    Reader r(b.data(), b.size());
    return ReadSound(r, out) && r.Remaining() == 0;
}

} // namespace

TEST_CASE(SoundEntryDefaultsTravelInThreeBytes) {
    SoundEntry s = MakeSound(0x2803, 1.f, 1.f, 0, 4);
    CHECK_EQ(s.flags, (uint8_t)0);
    auto b = Bytes(s);
    CHECK_EQ(b.size(), (size_t)3);
    CHECK_EQ(b.size(), SoundBytes(s));
    SoundEntry d;
    CHECK(ReadBack(b, d));
    CHECK_EQ(d.sfx, (uint16_t)0x2803);
    CHECK_EQ(SoundFreq(d), 1.f);
    CHECK_EQ(SoundVol(d), 1.f);
    CHECK_EQ(d.reverb, (int8_t)0);
    CHECK_EQ(d.token, (uint8_t)4);
}

TEST_CASE(SoundEntryRoundTrip) {
    SoundEntry s = MakeSound(0x3A12, 1.5f, 0.5f, -20, 2);
    CHECK_EQ(s.flags, (uint8_t)(SoundEntry::kFreq | SoundEntry::kVol | SoundEntry::kReverb | SoundEntry::kToken));
    s.flags |= SoundEntry::kPos;
    s.pos = { SlotKind::Actor, ActorRefValue(0x80010003u, 0, 0xEC) };
    auto b = Bytes(s);
    CHECK_EQ(b.size(), SoundBytes(s));
    SoundEntry d;
    CHECK(ReadBack(b, d));
    CHECK_EQ(d.flags, s.flags);
    CHECK(d.pos == s.pos);
    CHECK_EQ(SoundFreq(d), 1.5f);
    CHECK_EQ(SoundVol(d), 0.5f);
    CHECK_EQ(d.reverb, (int8_t)-20);
    CHECK_EQ(d.token, (uint8_t)2);

    SoundEntry w = MakeSound(0x2810, 1.f, 1.f, 0, 4);
    w.flags |= SoundEntry::kWorld | SoundEntry::kEachFrame;
    w.world[0] = 100.5f;
    w.world[1] = -3.f;
    w.world[2] = 32000.f;
    w.duration = 60;
    auto wb = Bytes(w);
    CHECK_EQ(wb.size(), SoundBytes(w));
    CHECK(ReadBack(wb, d));
    CHECK_EQ(d.world[0], 100.5f);
    CHECK_EQ(d.world[2], 32000.f);
    CHECK_EQ(d.duration, (uint16_t)60);
    CHECK((d.flags & SoundEntry::kEachFrame) != 0);
}

TEST_CASE(SoundEntryRejectsGarbage) {
    SoundEntry d;
    CHECK(!ReadBack(Bytes(MakeSound(0x7803, 1.f, 1.f, 0, 4)), d)); // bank 7: none
    CHECK(!ReadBack(Bytes(MakeSound(0x2403, 1.f, 1.f, 0, 4)), d)); // the 0x400 bit: no sound has it
    SoundEntry bad = MakeSound(0x2803, 1.f, 1.f, 0, 4);
    bad.flags = 0x80;
    CHECK(!ReadBack(Bytes(bad), d)); // unknown flag
    bad.flags = SoundEntry::kEachFrame;
    CHECK(!ReadBack(Bytes(bad), d)); // every frame, but no place of the world
    bad.flags = SoundEntry::kPos;
    bad.pos = { SlotKind::Raw, 5 };
    CHECK(!ReadBack(Bytes(bad), d)); // a place must be a pointer
    bad.flags = SoundEntry::kPos | SoundEntry::kWorld;
    bad.pos = { SlotKind::Exe, 5 };
    CHECK(!ReadBack(Bytes(bad), d)); // two places
    bad.flags = SoundEntry::kWorld;
    bad.world[0] = 1e9f;
    CHECK(!ReadBack(Bytes(bad), d)); // off any map
    auto cut = Bytes(MakeSound(0x2803, 2.f, 1.f, 0, 4));
    cut.pop_back();
    CHECK(!ReadBack(cut, d));
}

// A sound that must keep going while it is asked for every frame (instead of starting again) is played without its
// 0x800 bit ("NA_SE_... - SFX_FLAG"): the spin attack's charge, a bomb's fuse, the Goron's rolling dust.
TEST_CASE(SoundEntryCarriesContinuousSounds) {
    CHECK(SoundIdPlausible(0x1022)); // NA_SE_IT_SWORD_CHARGE - SFX_FLAG
    CHECK(SoundIdPlausible(0x00EB)); // NA_SE_PL_GORON_BALL_CHARGE - SFX_FLAG
    CHECK(SoundIdPlausible(0x1822)); // the same sound, restarted
    CHECK(!SoundIdPlausible(0x1422) && !SoundIdPlausible(0x1C22)); // 0x400: never
    SoundEntry d;
    CHECK(ReadBack(Bytes(MakeSound(0x1022, 1.25f, 1.f, 0, 4)), d));
    CHECK_EQ(d.sfx, (uint16_t)0x1022);
    CHECK_EQ(SoundFreq(d), 1.25f);
}

TEST_CASE(SoundListsAreBounded) {
    std::vector<SoundEntry> list(10, MakeSound(0x2803, 1.f, 1.f, 0, 4));
    Writer w;
    WriteSounds(w, list, 8);
    auto b = w.Take();
    CHECK_EQ(b[0], (uint8_t)8);
    CHECK_EQ(b.size(), SoundsBytes(list, 8));
    Reader r(b.data(), b.size());
    std::vector<SoundEntry> out;
    CHECK(ReadSounds(r, out, 8));
    CHECK_EQ(out.size(), (size_t)8);
    CHECK_EQ(r.Remaining(), (size_t)0);
    Reader r2(b.data(), b.size());
    CHECK(!ReadSounds(r2, out, 4)); // more than this list allows
}
