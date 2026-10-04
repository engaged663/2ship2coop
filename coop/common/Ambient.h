#pragma once
// [COOP] Sincronización total §3: the music orders and camera quakes an actor makes in the game that simulates it,
// for the players in its room ("ambient"). Plain data and its checks; the game decides what goes in.
#include "Events.h"

#include <cstdint>
#include <vector>

namespace coop::ambient {

enum class MusicKind : uint8_t {
    Cmd = 0,            // AudioSeq_QueueSeqCmd(value)
    StorePrevBgm = 1,   // Audio_PlayBgm_StorePrevBgm(value)
    RestorePrevBgm = 2, // Audio_RestorePrevBgm()
    PlaySubBgm = 3,     // Audio_PlaySubBgm(value)
    StopSubBgm = 4,     // Audio_StopSubBgm()
    Fanfare = 5,        // Audio_PlayFanfare(value)
};
constexpr int kMusicKinds = 6;

struct Music {
    MusicKind kind = MusicKind::Cmd;
    uint32_t value = 0;
};

struct Quake {
    int16_t type = 0; // QUAKE_TYPE_1..QUAKE_TYPE_6
    int16_t speed = 0;
    int16_t y = 0;
    int16_t x = 0;
    int16_t fov = 0;
    int16_t roll = 0;
    int16_t duration = 0; // frames
};

constexpr int kMaxMusic = 16;
constexpr int kMaxQuakes = 4;
constexpr int kQuakeTypes = 7;          // QUAKE_TYPE_NONE..QUAKE_TYPE_6
constexpr int kMaxQuakeDuration = 2000; // frames

// A raw sequence command that may travel: the main BGM (0), the fanfare (1), the sub BGM (3) or the ambience (4)
// players, operations 0x0-0xD (never the sound effects' player, a global command or resetting the audio heap).
bool SeqCmdAllowed(uint32_t cmd);
bool MusicFromJson(const json& ev, std::vector<Music>& out);  // "music" ([[kind, value]...]; missing: none)
bool QuakesFromJson(const json& ev, std::vector<Quake>& out); // "quake" ([[type, speed, y, x, fov, roll, duration]...])
json MusicToJson(const std::vector<Music>& music);
json QuakesToJson(const std::vector<Quake>& quakes);

} // namespace coop::ambient
