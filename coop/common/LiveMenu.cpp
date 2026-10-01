#include "LiveMenu.h"

namespace coop {

void LiveMenuWatch::Closed() {
    *this = LiveMenuWatch();
}

void LiveMenuWatch::Opened(const LiveMenuView& v) {
    mOpen = true;
    mTaken = v.taken;
    mOwnText = v.textOpen;
    mOwnTextId = v.textId;
}

void LiveMenuWatch::AfterMenu(const LiveMenuView& v) {
    mOwnText = v.textOpen;
    mOwnTextId = v.textId;
}

LiveMenuClose LiveMenuWatch::AfterWorld(const LiveMenuView& v) {
    bool takenNow = v.taken && !mTaken;
    mTaken = v.taken;

    // First of all: a text of the game must never be dropped (a talk also takes Link, and may change the scene)
    if (v.textOpen && !(mOwnText && v.textId == mOwnTextId)) {
        return LiveMenuClose::KeepText;
    }
    if (v.transition || v.gameOver || takenNow) {
        return LiveMenuClose::DropText;
    }
    return LiveMenuClose::No;
}

int LiveMenu_Steps(uint32_t frame) {
    return (frame % 2 == 0) ? 2 : 1;
}

} // namespace coop
