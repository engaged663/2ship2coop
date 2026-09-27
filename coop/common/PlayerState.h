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

// Limits of the engine values a puppet uses as table indexes (checked against the game headers by
// static_asserts in PoseCapture.cpp). Anything outside them would draw garbage or crash.
namespace pose_limits {
constexpr uint8_t kFormCount = 5;          // PLAYER_FORM_MAX
constexpr uint8_t kMaskCount = 0x19;       // PLAYER_MASK_MAX
constexpr uint8_t kShieldCount = 3;        // PLAYER_SHIELD_MAX
constexpr uint8_t kModelGroupCount = 15;   // PLAYER_MODELGROUP_MAX
constexpr uint8_t kFaceCount = 16;         // PLAYER_FACE_MAX
constexpr uint8_t kSwordMaxDrawn = 3;      // EQUIP_VALUE_SWORD_GILDED: the human sword models end there
constexpr int8_t kItemActionMin = -1;      // PLAYER_IA_MINUS1
constexpr int8_t kItemActionCount = 0x53;  // PLAYER_IA_MAX
constexpr uint16_t kEntranceScenes = 0x6E; // ENTR_SCENE_MAX (entrance >> 9)
constexpr float kWorldLimit = 60000.f;     // no map is this big
constexpr float kValueLimit = 1.0e6f;      // speed and the raw float fields
// State bits a puppet may use (everything else could make the draw code touch actors it lacks).
constexpr uint32_t kStateFlags1 = (1u << 27) | (1u << 22) | (1u << 25); // swimming, shield up, Zora boomerang
constexpr uint32_t kStateFlags2 = 1u << 29;                             // not drawn
constexpr uint32_t kStateFlags3 = (1u << 12) | (1u << 15);              // Goron ball, Zora fast swim
} // namespace pose_limits

// False when a value is impossible (enum out of range, non-finite or absurd float): drop the packet.
// Otherwise clears the state bits a puppet must not use and turns an undrawable sword into none.
bool SanitizePlayerState(PlayerState& state);

std::vector<uint8_t> EncodePlayerState(const PlayerState& state);
// False when the buffer is not exactly one well-formed PlayerState.
bool DecodePlayerState(const uint8_t* data, size_t size, PlayerState& out);
// Overwrites the player id inside an encoded packet (offset 1). False if the buffer is too small.
bool StampPlayerId(uint8_t* data, size_t size, uint8_t playerId);
size_t PlayerStateWireSize();

} // namespace coop
