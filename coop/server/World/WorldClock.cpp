#include "WorldClock.h"

#include "common/Clock.h"
#include "common/Protocol.h"

#include <algorithm>

namespace coop::server {

uint32_t WorldClock::Abs(int64_t nowMs) const {
    if (!mRunning) {
        return mBaseAbs;
    }
    int64_t elapsed = std::max<int64_t>(0, nowMs - mBaseMs);
    int64_t abs = (int64_t)mBaseAbs + (int64_t)((double)elapsed * clock::UnitsPerSecond(mInverted) * mScale / 1000.0);
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

double WorldClock::UnitsPerSecond() const {
    return mRunning ? clock::UnitsPerSecond(mInverted) * mScale : 0.0;
}

void WorldClock::SetScale(double scale, int64_t nowMs) {
    scale = std::clamp(scale, kMinTimeSpeed, kMaxTimeSpeed);
    if (scale == mScale) {
        return;
    }
    Rebase(nowMs); // what already passed stays as it is
    mScale = scale;
}

void WorldClock::Set(uint32_t abs, int64_t nowMs) {
    mBaseAbs = std::min<uint32_t>(abs, clock::kMoonAbs);
    mBaseMs = nowMs;
}

bool WorldClock::MoonReached(int64_t nowMs) const {
    return Abs(nowMs) >= clock::kMoonAbs;
}

} // namespace coop::server
