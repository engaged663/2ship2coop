#pragma once
// [COOP] Sincronización total §5.1: the loaded scene's flags (play->actorCtx.sceneFlags) as 11 words, the changes the
// games send ("sflag": ops) and which words are temporary (the server keeps those per scene + layer while somebody is
// there; the others belong to the cycle and the shared world keeps them).
#include "Events.h"

#include <array>
#include <cstdint>
#include <vector>

namespace coop::scene_flags {

enum Word : uint8_t {
    kChest = 0,
    kSwitch0 = 1,
    kSwitch1 = 2,
    kSwitch2 = 3, // switch flags 0x40-0x5F: temporary
    kSwitch3 = 4, // 0x60-0x7F: temporary
    kClear = 5,
    kClearTemp = 6,
    kCollect0 = 7,
    kCollect1 = 8, // collectible flags 0x20-0x7F: temporary
    kCollect2 = 9,
    kCollect3 = 10,
    kWordCount = 11,
};

using Words = std::array<uint32_t, kWordCount>;

constexpr bool IsTemporary(uint8_t w) {
    return w == kSwitch2 || w == kSwitch3 || w == kClearTemp || w == kCollect1 || w == kCollect2 || w == kCollect3;
}

struct Op {
    uint8_t word = 0;
    uint32_t set = 0;   // bits turned on
    uint32_t clear = 0; // bits turned off (never the same as set)
};

std::vector<Op> Diff(const Words& from, const Words& to); // one op per changed word
void Apply(Words& words, const Op& op);
// "ops": [[word, set, clear]...]: 1..kWordCount ops, word < kWordCount, 32-bit values, set & clear == 0, not both 0.
bool OpsFromJson(const json& ev, std::vector<Op>& out);
json OpsToJson(const std::vector<Op>& ops);
// "words": exactly kWordCount 32-bit values.
bool WordsFromJson(const json& ev, Words& out);
json WordsToJson(const Words& words);
Words TemporaryOf(const Words& words); // the temporary words, the rest 0

} // namespace coop::scene_flags
