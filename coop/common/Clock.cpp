#include "Clock.h"

#include "Text.h"

#include <algorithm>
#include <cstdio>

namespace coop::clock {

namespace {

constexpr int32_t kAheadToleranceUnits = 60; // the game may run up to 1 s ahead before it waits
constexpr int32_t kBehindToleranceUnits = 6; // more than 2 frames behind: jump forward
constexpr int32_t kMaxAheadUnits = 600;      // more than 10 s ahead (a lost jump, a restarted server): go back

// Same rounding as the game's CLOCK_TIME(hr, min).
uint16_t TimeOf(int hours, int minutes) {
    return (uint16_t)(((hours * 60 + minutes) * 0x10000) / (24 * 60));
}

} // namespace

uint16_t TimeOfAbs(uint32_t abs) {
    return (uint16_t)((kDawnTime + abs) & 0xFFFF);
}

int DayOfAbs(uint32_t abs) {
    return 1 + (int)(abs / kDayUnits);
}

uint32_t AbsOf(int day, uint16_t time) {
    day = std::clamp(day, 1, 3);
    return (uint32_t)(day - 1) * kDayUnits + (uint16_t)(time - kDawnTime);
}

bool IsNight(uint16_t time) {
    return time >= TimeOf(18, 0) || time < kDawnTime;
}

uint32_t NextHalfDay(uint32_t abs) {
    return (abs / kHalfDayUnits + 1) * kHalfDayUnits;
}

int UnitsPerSecond(bool inverted) {
    return inverted ? kUnitsPerSecondInverted : kUnitsPerSecond;
}

std::string Format(uint32_t abs) {
    int minutes = (int)((TimeOfAbs(abs) * 1440u + 0x8000u) / 0x10000u) % 1440;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Día %d, %02d:%02d", DayOfAbs(abs), minutes / 60, minutes % 60);
    return buf;
}

bool Parse(int day, const std::string& hhmm, uint32_t& abs) {
    size_t colon = hhmm.find(':');
    int hours = 0;
    int minutes = 0;
    if (day < 1 || day > 3 || colon == std::string::npos || !ParseInt(hhmm.substr(0, colon), hours) ||
        !ParseInt(hhmm.substr(colon + 1), minutes) || hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
        return false;
    }
    abs = AbsOf(day, TimeOf(hours, minutes));
    return true;
}

Follow FollowServer(uint32_t gameAbs, uint32_t serverAbs, bool serverStopped) {
    const uint32_t limit = kMoonAbs - kClientMoonMargin;
    Follow f;
    if (gameAbs >= limit) {
        f.freeze = true;
        f.write = gameAbs > limit;
        f.backwards = f.write;
        f.writeAbs = limit;
        return f;
    }
    uint32_t target = std::min(serverAbs, limit);
    int32_t drift = (int32_t)gameAbs - (int32_t)target;
    if (serverStopped) {
        f.freeze = true;
        f.backwards = drift > kMaxAheadUnits;
        f.write = drift < 0 || f.backwards;
        f.writeAbs = target;
        return f;
    }
    if (drift > kMaxAheadUnits) {
        f.write = true;
        f.backwards = true;
        f.writeAbs = target;
    } else if (drift > kAheadToleranceUnits) {
        f.freeze = true;
    } else if (drift < -kBehindToleranceUnits) {
        f.write = true;
        f.writeAbs = target;
    }
    return f;
}

} // namespace coop::clock
