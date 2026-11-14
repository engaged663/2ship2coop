#pragma once
// [COOP] The magic meter's waiting states (gSaveContext.magicState) and what ends each one. Magic_Consume only works
// from the idle state, so a waiting state nothing will end any more leaves Link without magic until the game restarts
// (reported: "not being able to use magic at all"). In the co-op something can take away what was going to end it (the
// arrow, the spin attack or the form that started it, while our Link is grabbed, followed, teleported or its world
// rebuilt). The game (mm/2s2h/Coop/Features/MagicGuard.cpp) asks these every frame; tested in
// coop/tests/TestMagicRules.cpp.

#include <cstdint>

namespace coop {

// The engine's MagicState values (z64save.h; static_asserted in MagicGuard.cpp).
constexpr int kMagicIdle = 0;
constexpr int kMagicConsumeSetup = 1;
constexpr int kMagicConsume = 2;
constexpr int kMagicMeterFlash1 = 3; // after consuming: until whatever consumed it resets it
constexpr int kMagicMeterFlash2 = 4; // a consumption shown in yellow (the spin attack charging)
constexpr int kMagicReset = 5;
constexpr int kMagicMeterFlash3 = 6;
constexpr int kMagicConsumeLens = 7;
constexpr int kMagicStepCapacity = 8;
constexpr int kMagicFill = 9;
constexpr int kMagicGoronZoraSetup = 10;
constexpr int kMagicGoronZora = 11;
constexpr int kMagicGiantsMask = 12;

// What can still end the state the meter is in.
struct MagicHolders {
    bool item = false;        // one of ours: an arrow (magic or Deku bubble), its Arrow_*, a spin attack, a song's effect
    bool goronOrZora = false; // Link is a Goron or a Zora (the spikes, the barrier)
    bool giantsMask = false;  // Link wears the Giant's Mask
};

// 3 seconds (20 frames a second) waiting with nothing that ends it: back to idle.
constexpr int kMagicStuckFrames = 60;

// True = the meter waits in `state` and nothing that would end it is left.
bool Magic_Orphaned(int state, const MagicHolders& holders);

// Counts the frames in a row the meter was orphaned. Frame() = true: give it back to idle now.
class MagicGuard {
  public:
    bool Frame(bool orphaned);

  private:
    int mFrames = 0;
};

} // namespace coop
