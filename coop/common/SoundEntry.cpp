#include "SoundEntry.h"

#include "SlotCodec.h"

#include <algorithm>
#include <cmath>

namespace coop {

namespace {

constexpr uint8_t kKnownFlags = SoundEntry::kPos | SoundEntry::kWorld | SoundEntry::kEachFrame | SoundEntry::kFreq |
                                SoundEntry::kVol | SoundEntry::kReverb | SoundEntry::kToken;

bool PointerKind(SlotKind k) {
    return k == SlotKind::Exe || k == SlotKind::Actor || k == SlotKind::Link || k == SlotKind::Scene;
}

} // namespace

bool SoundIdPlausible(uint16_t sfx) {
    return ((sfx >> 12) & 0xF) < sound_limits::kBanks && (sfx & sound_limits::kNeverBit) == 0;
}

SoundEntry MakeSound(uint16_t sfx, float freq, float vol, int8_t reverb, uint8_t token) {
    SoundEntry s;
    s.sfx = sfx;
    float f = std::isfinite(freq) ? freq : 1.f;
    float v = std::isfinite(vol) ? vol : 1.f;
    s.freq = (uint16_t)std::clamp<long>(std::lround(f * 4096.f), 0L, 65535L);
    s.vol = (uint8_t)std::clamp<long>(std::lround(v * 128.f), 0L, 255L);
    s.reverb = reverb;
    s.token = token;
    s.flags = (uint8_t)((s.freq != 4096 ? SoundEntry::kFreq : 0) | (s.vol != 128 ? SoundEntry::kVol : 0) |
                        (s.reverb != 0 ? SoundEntry::kReverb : 0) | (s.token != 4 ? SoundEntry::kToken : 0));
    return s;
}

float SoundFreq(const SoundEntry& s) {
    return s.freq / 4096.f;
}

float SoundVol(const SoundEntry& s) {
    return s.vol / 128.f;
}

void WriteSound(Writer& w, const SoundEntry& s) {
    w.U16(s.sfx);
    w.U8(s.flags);
    if (s.flags & SoundEntry::kPos) {
        WriteImageSlot(w, s.pos);
    }
    if (s.flags & SoundEntry::kWorld) {
        for (float v : s.world) {
            w.F32(v);
        }
        w.U16(s.duration);
    }
    if (s.flags & SoundEntry::kFreq) {
        w.U16(s.freq);
    }
    if (s.flags & SoundEntry::kVol) {
        w.U8(s.vol);
    }
    if (s.flags & SoundEntry::kReverb) {
        w.S8(s.reverb);
    }
    if (s.flags & SoundEntry::kToken) {
        w.U8(s.token);
    }
}

bool ReadSound(Reader& r, SoundEntry& s) {
    s = SoundEntry{};
    if (!r.U16(s.sfx) || !r.U8(s.flags) || (s.flags & ~kKnownFlags) != 0 || !SoundIdPlausible(s.sfx)) {
        return false;
    }
    bool pos = (s.flags & SoundEntry::kPos) != 0;
    bool world = (s.flags & SoundEntry::kWorld) != 0;
    if ((pos && world) || ((s.flags & SoundEntry::kEachFrame) && !world)) {
        return false;
    }
    if (pos && (!ReadImageSlot(r, s.pos) || !PointerKind(s.pos.kind))) {
        return false;
    }
    if (world) {
        for (float& v : s.world) {
            if (!r.F32(v) || !std::isfinite(v) || std::fabs(v) > sound_limits::kWorldLimit) {
                return false;
            }
        }
        if (!r.U16(s.duration)) {
            return false;
        }
    }
    return (!(s.flags & SoundEntry::kFreq) || r.U16(s.freq)) && (!(s.flags & SoundEntry::kVol) || r.U8(s.vol)) &&
           (!(s.flags & SoundEntry::kReverb) || r.S8(s.reverb)) && (!(s.flags & SoundEntry::kToken) || r.U8(s.token));
}

size_t SoundBytes(const SoundEntry& s) {
    size_t n = 3;
    n += (s.flags & SoundEntry::kPos) ? ImageSlotBytes(s.pos) : 0;
    n += (s.flags & SoundEntry::kWorld) ? 14 : 0;
    n += (s.flags & SoundEntry::kFreq) ? 2 : 0;
    n += (s.flags & SoundEntry::kVol) ? 1 : 0;
    n += (s.flags & SoundEntry::kReverb) ? 1 : 0;
    n += (s.flags & SoundEntry::kToken) ? 1 : 0;
    return n;
}

void WriteSounds(Writer& w, const std::vector<SoundEntry>& sounds, size_t max) {
    size_t n = std::min(sounds.size(), max);
    w.U8((uint8_t)n);
    for (size_t i = 0; i < n; i++) {
        WriteSound(w, sounds[i]);
    }
}

bool ReadSounds(Reader& r, std::vector<SoundEntry>& out, size_t max) {
    uint8_t n = 0;
    if (!r.U8(n) || n > max) {
        return false;
    }
    out.resize(n);
    for (SoundEntry& s : out) {
        if (!ReadSound(r, s)) {
            return false;
        }
    }
    return true;
}

size_t SoundsBytes(const std::vector<SoundEntry>& sounds, size_t max) {
    size_t n = 1;
    for (size_t i = 0; i < sounds.size() && i < max; i++) {
        n += SoundBytes(sounds[i]);
    }
    return n;
}

} // namespace coop
