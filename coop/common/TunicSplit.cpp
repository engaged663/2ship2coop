// [COOP] The copy of a tunic display list and the per-Link color (see TunicSplit.h).
#include "common/TunicSplit.h"

namespace coop {

namespace {

uint8_t Opcode(const TunicWord& w) {
    return (uint8_t)((w.w0 >> 24) & 0xFF);
}

} // namespace

bool TunicIsPrimColor(const TunicWord& w) {
    return Opcode(w) == kGfxSetPrimColor;
}

bool TunicIsCall(const TunicWord& w) {
    return Opcode(w) == kGfxCall;
}

std::optional<std::vector<TunicWord>> BuildTunicCopy(const TunicWord* list, size_t count,
                                                     const std::vector<uint32_t>& sites, const TunicWord& end) {
    if (list == nullptr || count == 0 || sites.empty()) {
        return std::nullopt;
    }
    size_t next = 0; // the first index the next place may take
    for (uint32_t site : sites) {
        if (site < next || (size_t)site + 1 >= count) {
            return std::nullopt;
        }
        if (!TunicIsPrimColor(list[site]) && !TunicIsCall(list[site])) {
            return std::nullopt;
        }
        next = (size_t)site + 1;
    }
    std::vector<TunicWord> copy(list, list + count);
    for (uint32_t site : sites) {
        copy[site] = end;
    }
    return copy;
}

TunicWord TunicPrimColor(const TunicWord& original, const Rgb& rgb) {
    TunicWord out;
    uint64_t alpha = 0xFF;
    if (TunicIsPrimColor(original)) {
        out.w0 = original.w0 & 0xFFFFFFFFull; // command, minimum level, LOD fraction
        alpha = original.w1 & 0xFF;
    } else {
        out.w0 = (uint64_t)kGfxSetPrimColor << 24;
    }
    out.w1 = ((uint64_t)rgb.r << 24) | ((uint64_t)rgb.g << 16) | ((uint64_t)rgb.b << 8) | alpha;
    return out;
}

} // namespace coop
