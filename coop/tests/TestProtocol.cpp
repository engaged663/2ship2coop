#include "TestMain.h"

#include "common/Events.h"
#include "common/PlayerState.h"
#include "common/Protocol.h"

TEST_CASE(PlayerStateRoundTrip) {
    coop::PlayerState s;
    s.seq = 7;
    s.sceneId = 0x6F;
    s.roomNum = 2;
    s.form = 3;
    s.entrance = 0xD800;
    s.pos[0] = 1.f;
    s.pos[1] = -2.5f;
    s.pos[2] = 300.f;
    s.rot = { 1, -2, 3 };
    s.mask = 9;
    s.sword = 2;
    s.stateFlags3 = 0x1000;
    s.headLimbRot = { 4, 5, 6 };
    s.joints[0] = { -1, -2, -3 };
    s.joints[21] = { 5, 6, 7 };
    s.appearance = 0x1234;
    s.unk_B86[1] = -9;
    s.unk_B10 = 0.25f;

    auto bytes = coop::EncodePlayerState(s);
    CHECK_EQ(bytes.size(), coop::PlayerStateWireSize());
    CHECK_EQ(bytes[0], coop::kStreamPlayerState);

    coop::PlayerState d;
    CHECK(coop::DecodePlayerState(bytes.data(), bytes.size(), d));
    CHECK_EQ(d.seq, (uint16_t)7);
    CHECK_EQ(d.sceneId, (int16_t)0x6F);
    CHECK_EQ(d.roomNum, (int8_t)2);
    CHECK_EQ(d.form, (uint8_t)3);
    CHECK_EQ(d.entrance, (uint16_t)0xD800);
    CHECK_EQ(d.pos[1], -2.5f);
    CHECK_EQ(d.rot.y, (int16_t)-2);
    CHECK_EQ(d.mask, (uint8_t)9);
    CHECK_EQ(d.sword, (uint8_t)2);
    CHECK_EQ(d.stateFlags3, (uint32_t)0x1000);
    CHECK_EQ(d.headLimbRot.z, (int16_t)6);
    CHECK_EQ(d.joints[0].x, (int16_t)-1);
    CHECK_EQ(d.joints[21].z, (int16_t)7);
    CHECK_EQ(d.appearance, (int16_t)0x1234);
    CHECK_EQ(d.unk_B86[1], (int16_t)-9);
    CHECK_EQ(d.unk_B10, 0.25f);
}

TEST_CASE(DecodeRejectsTruncated) {
    auto bytes = coop::EncodePlayerState(coop::PlayerState{});
    CHECK_EQ(bytes.size(), coop::PlayerStateWireSize());
    coop::PlayerState d;
    CHECK(!coop::DecodePlayerState(bytes.data(), bytes.size() - 1, d));
    CHECK(!coop::DecodePlayerState(bytes.data(), 0, d));
    bytes[0] = 99; // wrong stream type
    CHECK(!coop::DecodePlayerState(bytes.data(), bytes.size(), d));
}

TEST_CASE(DecodeRejectsTrailingBytes) {
    auto bytes = coop::EncodePlayerState(coop::PlayerState{});
    CHECK_EQ(bytes.size(), coop::PlayerStateWireSize());
    bytes.push_back(0);
    coop::PlayerState d;
    CHECK(!coop::DecodePlayerState(bytes.data(), bytes.size(), d));
}

TEST_CASE(StampPlayerIdWritesOffset1) {
    auto bytes = coop::EncodePlayerState(coop::PlayerState{});
    CHECK_EQ(bytes.size(), coop::PlayerStateWireSize());
    CHECK(coop::StampPlayerId(bytes.data(), bytes.size(), 3));
    coop::PlayerState d;
    CHECK(coop::DecodePlayerState(bytes.data(), bytes.size(), d));
    CHECK_EQ(d.playerId, (uint8_t)3);
    CHECK(!coop::StampPlayerId(bytes.data(), 1, 3));
}

TEST_CASE(PlayerStateV15RoundTrip) {
    coop::PlayerState s;
    s.sceneId = 0x10;
    s.layer = 3;
    s.meleeWeaponState = -1;
    s.meleeWeaponAnimation = 12;
    s.ocarinaInstrument = 1;
    s.ocarinaPitch = 5;
    s.ocarinaBend = 4500;
    s.ocarinaVibrato = 7;
    s.sounds.push_back(coop::MakeSound(0x6800, 1.f, 1.f, 0, 4));
    coop::SoundEntry hit = coop::MakeSound(0x1810, 1.25f, 1.f, 0, 4);
    hit.flags |= coop::SoundEntry::kPos;
    hit.pos = { coop::SlotKind::Actor, coop::ActorRefValue(0x0103, 0, 0xEC) };
    s.sounds.push_back(hit);
    auto bytes = coop::EncodePlayerState(s);
    CHECK(bytes.size() > coop::PlayerStateWireSize());
    coop::PlayerState d;
    CHECK(coop::DecodePlayerState(bytes.data(), bytes.size(), d));
    CHECK(coop::SanitizePlayerState(d));
    CHECK_EQ(d.layer, (uint8_t)3);
    CHECK_EQ(d.meleeWeaponState, (int8_t)-1);
    CHECK_EQ(d.meleeWeaponAnimation, (uint8_t)12);
    CHECK_EQ(d.ocarinaInstrument, (uint8_t)1);
    CHECK_EQ(d.ocarinaPitch, (uint8_t)5);
    CHECK_EQ(d.ocarinaBend, (uint16_t)4500);
    CHECK_EQ(d.ocarinaVibrato, (int8_t)7);
    CHECK_EQ(d.sounds.size(), (size_t)2);
    CHECK_EQ(d.sounds[1].sfx, (uint16_t)0x1810);
    CHECK(d.sounds[1].pos == hit.pos);
    bytes.push_back(0);
    CHECK(!coop::DecodePlayerState(bytes.data(), bytes.size(), d)); // trailing byte after the sounds
}

TEST_CASE(PlayerStateV15RejectsImpossibleValues) {
    auto sane = [](void (*edit)(coop::PlayerState&)) {
        coop::PlayerState s;
        edit(s);
        return coop::SanitizePlayerState(s);
    };
    CHECK(sane([](coop::PlayerState&) {}));
    CHECK(!sane([](coop::PlayerState& s) { s.layer = 17; }));
    CHECK(!sane([](coop::PlayerState& s) { s.meleeWeaponState = 2; }));
    CHECK(!sane([](coop::PlayerState& s) { s.meleeWeaponAnimation = 34; }));
    CHECK(!sane([](coop::PlayerState& s) { s.ocarinaInstrument = 17; }));
    CHECK(!sane([](coop::PlayerState& s) { s.ocarinaPitch = 16; }));
    CHECK(sane([](coop::PlayerState& s) { s.ocarinaPitch = 0xFF; }));
    coop::PlayerState many;
    many.sounds.assign(12, coop::MakeSound(0x6800, 1.f, 1.f, 0, 4));
    auto bytes = coop::EncodePlayerState(many); // written: 8 at most
    coop::PlayerState d;
    CHECK(coop::DecodePlayerState(bytes.data(), bytes.size(), d));
    CHECK_EQ(d.sounds.size(), (size_t)coop::sound_limits::kPerPose);
}

TEST_CASE(EventParseValidates) {
    coop::json ev;
    std::string err;
    auto chat = coop::MakeEvent(coop::ev::kChat);
    chat["text"] = "hola";
    std::string ok = coop::SerializeEvent(chat);
    CHECK(coop::ParseEvent((const uint8_t*)ok.data(), ok.size(), ev, &err));
    CHECK_EQ(coop::EventType(ev), std::string("chat"));
    CHECK_EQ(coop::GetString(ev, "text"), std::string("hola"));

    CHECK(!coop::ParseEvent((const uint8_t*)"nope", 4, ev, &err));
    CHECK(!err.empty());
    CHECK(!coop::ParseEvent((const uint8_t*)"{\"x\":1}", 7, ev, &err));     // no "t"
    CHECK(!coop::ParseEvent((const uint8_t*)"{\"t\":5}", 7, ev, &err));     // "t" not a string
    CHECK(!coop::ParseEvent((const uint8_t*)"[1,2]", 5, ev, &err));         // not an object
    std::string big(coop::kMaxClientEventBytes + 1, ' ');
    CHECK(!coop::ParseEvent((const uint8_t*)big.data(), big.size(), ev, &err));
}

TEST_CASE(EventGettersUseDefaultsOnWrongType) {
    coop::json ev = { { "t", "x" }, { "n", 5 }, { "s", "txt" }, { "f", 1.5 } };
    CHECK_EQ(coop::GetInt(ev, "n"), (int64_t)5);
    CHECK_EQ(coop::GetInt(ev, "s", -1), (int64_t)-1);
    CHECK_EQ(coop::GetInt(ev, "missing", 7), (int64_t)7);
    CHECK_EQ(coop::GetString(ev, "n", "def"), std::string("def"));
    CHECK_EQ(coop::GetNumber(ev, "f"), 1.5);
    CHECK_EQ(coop::GetNumber(ev, "n"), 5.0);
}

TEST_CASE(SerializeNeverThrowsOnInvalidUtf8) {
    coop::json ev = coop::MakeEvent(coop::ev::kSys);
    ev["text"] = std::string("bad\xC3");
    std::string out = coop::SerializeEvent(ev);
    CHECK(!out.empty());
}
