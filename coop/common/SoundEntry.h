#pragma once
// [COOP] Sincronización total §2: one sound of a game as it travels to the others (in the tail of a pose or in an
// actor's record). The receiver plays it with AudioSfx_PlaySfx, or SoundSource_* for a fixed place of the world.
#include "Slot.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

// ByteStream.h is not included here: the game includes this header after its own U32()/U16() macros (R4300.h).
class Writer;
class Reader;

namespace sound_limits {
constexpr int kPerPose = 8;          // sounds of one Link in one pose
constexpr int kPerRecord = 8;        // sounds of one actor in one record
constexpr int kBanks = 7;            // player, item, environment, enemy, system, ocarina, voice
constexpr uint16_t kFlagBit = 0x800; // every sound id has it (SFX_FLAG); 0x400 never
constexpr float kWorldLimit = 32767.f;
} // namespace sound_limits

struct SoundEntry {
    enum Flags : uint8_t {
        kPos = 1,       // `pos`: where it plays (a translated pointer); without it, on the sender's own body
        kWorld = 2,     // `world` + `duration`: a fixed place of the world (SoundSource)
        kEachFrame = 4, // with kWorld: SoundSource_PlaySfxEachFrameAtFixedWorldPos
        kFreq = 8,      // freq != 1
        kVol = 16,      // vol != 1
        kReverb = 32,   // reverb != 0
        kToken = 64,    // token != 4
    };
    uint16_t sfx = 0;
    uint8_t flags = 0;
    Slot pos;              // kPos
    float world[3] = {};   // kWorld
    uint16_t duration = 0; // kWorld: frames the place keeps sounding
    uint16_t freq = 4096;  // ×1/4096
    uint8_t vol = 128;     // ×1/128
    int8_t reverb = 0;
    uint8_t token = 4;
};

// A sound id this protocol accepts: bank below kBanks, with the 0x800 bit and without 0x400. (The game checks the
// size of each bank again.)
bool SoundIdPlausible(uint16_t sfx);
// The engine's values; a value equal to the default is left out of the wire.
SoundEntry MakeSound(uint16_t sfx, float freq, float vol, int8_t reverb, uint8_t token);
float SoundFreq(const SoundEntry& s);
float SoundVol(const SoundEntry& s);

void WriteSound(Writer& w, const SoundEntry& s);
// False when cut short, with unknown flags, an implausible id, a place that is not a pointer, two places, a world
// place off any map, or "every frame" without a world place.
bool ReadSound(Reader& r, SoundEntry& s);
size_t SoundBytes(const SoundEntry& s);
// [n u8] + n sounds, n ≤ max (the rest is left out when writing; more is an error when reading).
void WriteSounds(Writer& w, const std::vector<SoundEntry>& sounds, size_t max);
bool ReadSounds(Reader& r, std::vector<SoundEntry>& out, size_t max);
size_t SoundsBytes(const std::vector<SoundEntry>& sounds, size_t max);

} // namespace coop
