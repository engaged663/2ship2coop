#include "PlayerState.h"

#include "ByteStream.h"

namespace coop {

namespace {

// Adapters so one field list (VisitFields) drives both encoding and decoding.
struct WriteIO {
    Writer w;
    bool U8(uint8_t& v) { w.U8(v); return true; }
    bool S8(int8_t& v) { w.S8(v); return true; }
    bool U16(uint16_t& v) { w.U16(v); return true; }
    bool S16(int16_t& v) { w.S16(v); return true; }
    bool U32(uint32_t& v) { w.U32(v); return true; }
    bool F32(float& v) { w.F32(v); return true; }
};

struct ReadIO {
    Reader r;
    bool U8(uint8_t& v) { return r.U8(v); }
    bool S8(int8_t& v) { return r.S8(v); }
    bool U16(uint16_t& v) { return r.U16(v); }
    bool S16(int16_t& v) { return r.S16(v); }
    bool U32(uint32_t& v) { return r.U32(v); }
    bool F32(float& v) { return r.F32(v); }
};

template <class IO> bool Vec(IO& io, Vec3s16& v) {
    return io.S16(v.x) && io.S16(v.y) && io.S16(v.z);
}

// Wire layout after [type u8][playerId u8]. Append new fields at the end and bump kProtocolVersion.
template <class IO> bool VisitFields(IO& io, PlayerState& s) {
    bool ok = io.U16(s.seq) && io.S16(s.sceneId) && io.S8(s.roomNum) && io.U8(s.form) && io.U16(s.entrance) &&
              io.F32(s.pos[0]) && io.F32(s.pos[1]) && io.F32(s.pos[2]) && Vec(io, s.rot) && io.F32(s.speed) &&
              io.U8(s.mask) && io.U8(s.modelGroup) && io.U8(s.shield) && io.U8(s.sword) && io.S8(s.itemAction) &&
              io.S8(s.heldItemAction) && io.U8(s.face) && io.S8(s.invincibilityTimer) && io.U16(s.movementFlags) &&
              io.U32(s.stateFlags1) && io.U32(s.stateFlags2) && io.U32(s.stateFlags3) && Vec(io, s.headLimbRot) &&
              Vec(io, s.upperLimbRot) && io.S16(s.upperLimbYawSecondary) && io.F32(s.unk_AB8) &&
              io.S16(s.unk_AAA) && io.F32(s.unk_ABC) && io.S16(s.unk_B86[0]) && io.S16(s.unk_B86[1]) &&
              io.S16(s.unk_B28) && io.F32(s.unk_B10) && io.S16(s.actionVar1) && io.S16(s.unk_B8E) &&
              io.S16(s.unk_B62);
    for (int i = 0; ok && i < kPoseJoints; i++) {
        ok = Vec(io, s.joints[i]);
    }
    return ok && io.S16(s.appearance);
}

} // namespace

std::vector<uint8_t> EncodePlayerState(const PlayerState& state) {
    WriteIO io;
    io.w.U8(kStreamPlayerState);
    io.w.U8(state.playerId);
    PlayerState copy = state;
    VisitFields(io, copy);
    return io.w.Take();
}

size_t PlayerStateWireSize() {
    static const size_t size = EncodePlayerState(PlayerState{}).size();
    return size;
}

bool DecodePlayerState(const uint8_t* data, size_t size, PlayerState& out) {
    if (data == nullptr || size != PlayerStateWireSize() || data[0] != kStreamPlayerState) {
        return false;
    }
    ReadIO io{ Reader(data, size) };
    uint8_t type = 0;
    PlayerState decoded;
    if (!io.U8(type) || !io.U8(decoded.playerId) || !VisitFields(io, decoded)) {
        return false;
    }
    out = decoded;
    return true;
}

bool StampPlayerId(uint8_t* data, size_t size, uint8_t playerId) {
    if (data == nullptr || size < 2) {
        return false;
    }
    data[1] = playerId;
    return true;
}

} // namespace coop
