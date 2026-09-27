#include "WorldClock.h"

#include "common/Clock.h"

#include <algorithm>

namespace coop::server {

uint32_t WorldClock::Abs(int64_t nowMs) const {
    if (!mRunning) {
        return mBaseAbs;
    }
    int64_t elapsed = std::max<int64_t>(0, nowMs - mBaseMs);
    int64_t abs = (int64_t)mBaseAbs + elapsed * clock::UnitsPerSecond(mInverted) / 1000;
    return (uint32_t)std::min<int64_t>(abs, clock::kMoonAbs);
}

void WorldClock::Rebase(int64_t nowMs) {
    mBaseAbs = Abs(nowMs);
    mBaseMs = nowMs;
}

void WorldClock::SetRunning(bool running, int64_t nowMs) {
    if (running == mRunning) {
        return;
    }
    Rebase(nowMs);
    mRunning = running;
}

void WorldClock::SetInverted(bool inverted, int64_t nowMs) {
    if (inverted == mInverted) {
        return;
    }
    Rebase(nowMs);
    mInverted = inverted;
}

void WorldClock::Set(uint32_t abs, int64_t nowMs) {
    mBaseAbs = std::min<uint32_t>(abs, clock::kMoonAbs);
    mBaseMs = nowMs;
}

bool WorldClock::MoonReached(int64_t nowMs) const {
    return Abs(nowMs) >= clock::kMoonAbs;
}

} // namespace coop::server
