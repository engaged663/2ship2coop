#pragma once
// The server's 3-day clock (units in common/Clock.h). Runs only when SharedWorld says so; never passes the moon.
#include <cstdint>

namespace coop::server {

class WorldClock {
  public:
    uint32_t Abs(int64_t nowMs) const;
    bool Running() const {
        return mRunning;
    }
    bool Inverted() const {
        return mInverted;
    }
    double Scale() const {
        return mScale;
    }
    // Units of the clock per second right now: what a game needs to follow it between two broadcasts (0: stopped).
    double UnitsPerSecond() const;
    void SetRunning(bool running, int64_t nowMs);
    void SetInverted(bool inverted, int64_t nowMs);
    // How fast time passes (server.json "timeSpeed", kMinTimeSpeed..kMaxTimeSpeed): 1 is the original game.
    void SetScale(double scale, int64_t nowMs);
    void Set(uint32_t abs, int64_t nowMs); // clamped to the moon
    bool MoonReached(int64_t nowMs) const;

  private:
    void Rebase(int64_t nowMs);

    uint32_t mBaseAbs = 0;
    int64_t mBaseMs = 0;
    bool mRunning = false;
    bool mInverted = false;
    double mScale = 1.0;
};

} // namespace coop::server
