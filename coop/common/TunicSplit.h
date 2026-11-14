#pragma once
// [COOP] Each Link's tunic in their player's color (the game: mm/2s2h/Coop/Features/CoopTunics.cpp).
//
// The tunic's color is a "SetPrimColor" inside Link's display lists (the same lists for every Link, and 2Ship's
// cosmetic editor patches that same instruction into a call to its own color), so one color for every Link. The game
// keeps a copy of each of those lists with an "end of list" in place of every color, and draws a Link's piece as a
// tiny list of its own, made for that frame:
//     call(copy) · SetPrimColor(that Link's color) · call(copy + place + 1) · ... · end
// so each Link carries its color in its own drawing and the shared lists are never touched.
//
// This is the part that needs no game: checking the places and building the copy, over raw instruction words (on
// x64 the game's Gfx is two 64-bit words, the same layout as TunicWord). Tested in coop/tests/TestTunicSplit.cpp.
#include "common/PlayerColor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace coop {

struct TunicWord {
    uint64_t w0 = 0;
    uint64_t w1 = 0;
};
static_assert(sizeof(TunicWord) == 16, "a display list instruction is two 64-bit words");

constexpr uint8_t kGfxSetPrimColor = 0xFA; // G_SETPRIMCOLOR
constexpr uint8_t kGfxCall = 0xDE;         // G_DL (2Ship's cosmetic patch calls its color list)

bool TunicIsPrimColor(const TunicWord& w);
bool TunicIsCall(const TunicWord& w);

// The list with `end` in place of every place in `sites` (indices of instructions). nullopt when there is nothing to
// do or a place is wrong: outside the list or at its last instruction (nothing would follow the color), not after the
// previous place, or neither a color nor a call (a custom model or another version of the asset).
std::optional<std::vector<TunicWord>> BuildTunicCopy(const TunicWord* list, size_t count,
                                                     const std::vector<uint32_t>& sites, const TunicWord& end);

// The SetPrimColor that paints `rgb` where `original` set the tunic's color: same minimum level, LOD fraction and alpha.
// When `original` is not a color, a plain one (level 0, LOD fraction 0, opaque).
TunicWord TunicPrimColor(const TunicWord& original, const Rgb& rgb);

} // namespace coop
