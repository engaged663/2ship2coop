#pragma once
// The 3-day clock in "absolute" game units: 0 = Day 1 6:00, 0x10000 = Day 2 6:00, 0x30000 = the moon falls.
// The game keeps time as a u16 (0x10000 per day, 0x4000 = 6:00) plus a day number; these convert both ways.
#include <cstdint>
#include <string>

namespace coop::clock {

constexpr uint32_t kDayUnits = 0x10000;
constexpr uint32_t kHalfDayUnits = 0x8000;
constexpr uint32_t kMoonAbs = 3 * kDayUnits;  // Day 4, 6:00
constexpr uint16_t kDawnTime = 0x4000;        // 6:00 in the game's u16 time
constexpr int kUnitsPerSecond = 60;           // 3 per frame at 20 fps, as in the original
constexpr int kUnitsPerSecondInverted = 20;   // Inverted Song of Time
constexpr uint32_t kClientMoonMargin = 120;   // a game stops 2 s before the moon: the server makes it fall
constexpr int kMaxExtrapolationMs = 2000;     // a game keeps the server's clock running this long without news

uint16_t TimeOfAbs(uint32_t abs);        // save.time
int DayOfAbs(uint32_t abs);              // save.day (1..3; 4 at the moon)
uint32_t AbsOf(int day, uint16_t time);  // inverse; days outside 1..3 are clamped
bool IsNight(uint16_t time);             // 18:00 to 5:59
uint32_t NextHalfDay(uint32_t abs);      // next 6:00 or 18:00 strictly after abs
int UnitsPerSecond(bool inverted);
std::string Format(uint32_t abs);        // "Día 2, 14:35"
bool Parse(int day, const std::string& hhmm, uint32_t& abs); // day 1..3, "hh:mm"

// What a game does at the start of each frame to follow the server's clock (see mm/2s2h/Coop/World/ClockSync.cpp).
struct Follow {
    bool freeze = false;    // stop the game's own clock this frame (it is ahead, or the server's is stopped)
    bool write = false;     // set the game's time to writeAbs
    bool backwards = false; // that write moves the time back (the game must resync its day/night actors)
    uint32_t writeAbs = 0;
};
Follow FollowServer(uint32_t gameAbs, uint32_t serverAbs, bool serverStopped);

} // namespace coop::clock
