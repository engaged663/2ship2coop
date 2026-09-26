#pragma once
// Binary pose stream of one player (channel kChannelStream), sent once per game frame (20 Hz).
// The "unk_*" fields mirror the Player struct fields of the same name that the engine's draw code reads.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

constexpr uint8_t kStreamPlayerState = 1;
constexpr int kPoseJoints = 22; // PLAYER_LIMB_MAX

struct Vec3s16 {
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
};

struct PlayerState {
    uint8_t playerId = 0; // stamped by the server when relaying
    uint16_t seq = 0;
    int16_t sceneId = -1;
    int8_t roomNum = -1;
    uint8_t form = 0;
    uint16_t entrance = 0;
    float pos[3] = { 0.f, 0.f, 0.f };
    Vec3s16 rot;       // actor.shape.rot
    float speed = 0.f; // actor.speed
    uint8_t mask = 0;  // currentMask
    uint8_t modelGroup = 0;
    uint8_t shield = 0; // currentShield
    uint8_t sword = 0;  // GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) of the sender
    int8_t itemAction = 0;
    int8_t heldItemAction = 0;
    uint8_t face = 0; // actor.shape.face
    int8_t invincibilityTimer = 0;
    uint16_t movementFlags = 0; // skelAnime.movementFlags
    uint32_t stateFlags1 = 0;   // whitelisted by the sender
    uint32_t stateFlags2 = 0;
    uint32_t stateFlags3 = 0;
    Vec3s16 headLimbRot;
    Vec3s16 upperLimbRot;
    int16_t upperLimbYawSecondary = 0;
    float unk_AB8 = 0.f;
    int16_t unk_AAA = 0;
    float unk_ABC = 0.f;
    int16_t unk_B86[2] = { 0, 0 };
    int16_t unk_B28 = 0;
    float unk_B10 = 0.f; // unk_B10[0]
    int16_t actionVar1 = 0; // av1.actionVar1
    int16_t unk_B8E = 0;
    int16_t unk_B62 = 0;
    Vec3s16 joints[kPoseJoints]; // PlayerAnimationFrame.frameTable
    int16_t appearance = 0;      // PlayerAnimationFrame.appearanceInfo (face + hands)
};

std::vector<uint8_t> EncodePlayerState(const PlayerState& state);
// False when the buffer is not exactly one well-formed PlayerState.
bool DecodePlayerState(const uint8_t* data, size_t size, PlayerState& out);
// Overwrites the player id inside an encoded packet (offset 1). False if the buffer is too small.
bool StampPlayerId(uint8_t* data, size_t size, uint8_t playerId);
size_t PlayerStateWireSize();

} // namespace coop
