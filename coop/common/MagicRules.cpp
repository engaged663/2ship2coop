#include "MagicRules.h"

namespace coop {

bool Magic_Orphaned(int state, const MagicHolders& holders) {
    switch (state) {
        case kMagicMeterFlash1:
        case kMagicMeterFlash2:
        case kMagicMeterFlash3:
            return !holders.item;
        case kMagicGoronZoraSetup:
        case kMagicGoronZora:
            return !holders.goronOrZora;
        case kMagicGiantsMask:
            return !holders.giantsMask;
        default:
            return false; // idle, or a state the meter ends by itself
    }
}

bool MagicGuard::Frame(bool orphaned) {
    if (!orphaned) {
        mFrames = 0;
        return false;
    }
    if (++mFrames < kMagicStuckFrames) {
        return false;
    }
    mFrames = 0;
    return true;
}

} // namespace coop
