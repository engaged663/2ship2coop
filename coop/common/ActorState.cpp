#include "ActorState.h"

#include <cmath>

#include "ByteStream.h"

namespace coop {

namespace {

using namespace actor_limits;

// Header: [type u8][playerId u8][scene s16][room s8][seq u16][part u8][parts u8][total u16][goneCount u8][gone u16...]
// [count u8], then the records (WriteRecord: every field of ActorRecord in order, lists as [count u8][items]).
constexpr size_t kHeaderBytes = 1 + 1 + 2 + 1 + 2 + 1 + 1 + 2 + 1 + 1;

bool Finite(float v, float limit) {
    return std::isfinite(v) && std::fabs(v) <= limit;
}

void WriteVec(Writer& w, const Vec3s16& v) {
    w.S16(v.x);
    w.S16(v.y);
    w.S16(v.z);
}

bool ReadVec(Reader& r, Vec3s16& v) {
    return r.S16(v.x) && r.S16(v.y) && r.S16(v.z);
}

void WriteRecord(Writer& w, const ActorRecord& a) {
    w.U16(a.key);
    w.U16(a.actorId);
    w.S16(a.params);
    w.U8(a.visible ? 1 : 0);
    for (float v : a.pos) {
        w.F32(v);
    }
    for (float v : a.focus) {
        w.F32(v);
    }
    WriteVec(w, a.rot);
    w.S16(a.worldRotY);
    for (float v : a.scale) {
        w.F32(v);
    }
    w.U8(a.health);
    w.U16(a.colorFilterParams);
    w.U8(a.colorFilterTimer);
    w.U8(a.shadowAlpha);
    w.U8((uint8_t)a.joints.size());
    for (const Vec3s16& j : a.joints) {
        WriteVec(w, j);
    }
    w.U8((uint8_t)a.colliders.size());
    for (const ActorCollider& c : a.colliders) {
        w.U8(c.flags);
        WriteVec(w, c.dimPos);
    }
    w.U8((uint8_t)a.sfx.size());
    for (uint16_t s : a.sfx) {
        w.U16(s);
    }
    w.U16(a.loopSfx);
    w.U8(a.loopSfxFlags);
    w.U8((uint8_t)a.extras.size());
    for (uint8_t e : a.extras) {
        w.U8(e);
    }
}

bool ReadRecord(Reader& r, ActorRecord& a) {
    uint8_t flags = 0;
    if (!r.U16(a.key) || !r.U16(a.actorId) || !r.S16(a.params) || !r.U8(flags)) {
        return false;
    }
    a.visible = (flags & 1) != 0;
    for (float& v : a.pos) {
        if (!r.F32(v) || !Finite(v, pose_limits::kWorldLimit)) {
            return false;
        }
    }
    for (float& v : a.focus) {
        if (!r.F32(v) || !Finite(v, pose_limits::kWorldLimit)) {
            return false;
        }
    }
    if (!ReadVec(r, a.rot) || !r.S16(a.worldRotY)) {
        return false;
    }
    for (float& v : a.scale) {
        if (!r.F32(v) || !Finite(v, kScaleLimit)) {
            return false;
        }
    }
    uint8_t n = 0;
    if (!r.U8(a.health) || !r.U16(a.colorFilterParams) || !r.U8(a.colorFilterTimer) || !r.U8(a.shadowAlpha) ||
        !r.U8(n) || n > kJoints) {
        return false;
    }
    a.joints.resize(n);
    for (Vec3s16& j : a.joints) {
        if (!ReadVec(r, j)) {
            return false;
        }
    }
    if (!r.U8(n) || n > kColliders) {
        return false;
    }
    a.colliders.resize(n);
    for (ActorCollider& c : a.colliders) {
        if (!r.U8(c.flags) || !ReadVec(r, c.dimPos)) {
            return false;
        }
    }
    if (!r.U8(n) || n > kSfx) {
        return false;
    }
    a.sfx.resize(n);
    for (uint16_t& s : a.sfx) {
        if (!r.U16(s)) {
            return false;
        }
    }
    if (!r.U16(a.loopSfx) || !r.U8(a.loopSfxFlags) || !r.U8(n) || n > kExtras) {
        return false;
    }
    a.extras.resize(n);
    for (uint8_t& e : a.extras) {
        if (!r.U8(e)) {
            return false;
        }
    }
    return true;
}

// A record as the encoder will write it (limits applied: longer lists are cut).
ActorRecord Clamped(const ActorRecord& a) {
    ActorRecord c = a;
    if (c.joints.size() > (size_t)kJoints) {
        c.joints.resize(kJoints);
    }
    if (c.colliders.size() > (size_t)kColliders) {
        c.colliders.resize(kColliders);
    }
    if (c.sfx.size() > (size_t)kSfx) {
        c.sfx.resize(kSfx);
    }
    if (c.extras.size() > (size_t)kExtras) {
        c.extras.resize(kExtras);
    }
    return c;
}

std::vector<uint8_t> EncodePart(const ActorPacket& frame, uint8_t part, uint8_t parts, uint16_t total,
                                const std::vector<uint16_t>& gone, const std::vector<std::vector<uint8_t>>& records) {
    Writer w;
    w.U8(kStreamActors);
    w.U8(frame.playerId);
    w.S16(frame.scene);
    w.S8(frame.room);
    w.U16(frame.seq);
    w.U8(part);
    w.U8(parts);
    w.U16(total);
    w.U8((uint8_t)gone.size());
    for (uint16_t key : gone) {
        w.U16(key);
    }
    w.U8((uint8_t)records.size());
    std::vector<uint8_t> out = w.Take();
    for (const auto& rec : records) {
        out.insert(out.end(), rec.begin(), rec.end());
    }
    return out;
}

} // namespace

std::vector<std::vector<uint8_t>> EncodeActorPackets(const ActorPacket& frame) {
    std::vector<uint16_t> gone = frame.gone;
    if (gone.size() > (size_t)kGone) {
        gone.resize(kGone);
    }
    // Group the encoded records into parts that fit.
    std::vector<std::vector<std::vector<uint8_t>>> groups(1);
    size_t used = kHeaderBytes + gone.size() * 2;
    size_t count = 0;
    for (const ActorRecord& a : frame.actors) {
        if (count++ >= (size_t)kActorsPerFrame) {
            break;
        }
        Writer w;
        WriteRecord(w, Clamped(a));
        std::vector<uint8_t> rec = w.Take();
        if (used + rec.size() > kPacketBytes && !groups.back().empty()) {
            if (groups.size() >= (size_t)kParts) {
                break;
            }
            groups.emplace_back();
            used = kHeaderBytes;
        }
        used += rec.size();
        groups.back().push_back(std::move(rec));
    }
    uint16_t total = 0;
    for (const auto& g : groups) {
        total = (uint16_t)(total + g.size());
    }
    std::vector<std::vector<uint8_t>> packets;
    for (size_t i = 0; i < groups.size(); i++) {
        packets.push_back(EncodePart(frame, (uint8_t)i, (uint8_t)groups.size(), total,
                                     i == 0 ? gone : std::vector<uint16_t>{}, groups[i]));
    }
    return packets;
}

bool PeekActorHeader(const uint8_t* data, size_t size, int16_t& scene, int8_t& room) {
    if (data == nullptr || size < kHeaderBytes || data[0] != kStreamActors) {
        return false;
    }
    Reader r(data + 2, size - 2);
    int16_t s = 0;
    int8_t rm = 0;
    if (!r.S16(s) || !r.S8(rm) || s < 0 || rm < 0 || rm > kRoomMax) {
        return false;
    }
    scene = s;
    room = rm;
    return true;
}

bool DecodeActorPacket(const uint8_t* data, size_t size, ActorPacket& out) {
    if (data == nullptr || size > kPacketBytes || size < kHeaderBytes || data[0] != kStreamActors) {
        return false;
    }
    Reader r(data, size);
    ActorPacket p;
    uint8_t type = 0;
    uint8_t n = 0;
    if (!r.U8(type) || !r.U8(p.playerId) || !r.S16(p.scene) || !r.S8(p.room) || !r.U16(p.seq) || !r.U8(p.part) ||
        !r.U8(p.parts) || !r.U16(p.total) || !r.U8(n)) {
        return false;
    }
    if (p.scene < 0 || p.room < 0 || p.room > kRoomMax || p.parts == 0 || p.parts > kParts || p.part >= p.parts ||
        n > kGone || (n > 0 && p.part != 0) || p.total > kActorsPerFrame) {
        return false;
    }
    p.gone.resize(n);
    for (uint16_t& key : p.gone) {
        if (!r.U16(key)) {
            return false;
        }
    }
    if (!r.U8(n) || n > kActorsPerFrame) {
        return false;
    }
    p.actors.resize(n);
    for (ActorRecord& a : p.actors) {
        if (!ReadRecord(r, a)) {
            return false;
        }
    }
    if (r.Remaining() != 0) {
        return false;
    }
    out = std::move(p);
    return true;
}

} // namespace coop
