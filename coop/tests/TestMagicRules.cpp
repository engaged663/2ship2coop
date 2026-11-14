// The magic meter's waiting states (common/MagicRules.h): a state nothing can end any more goes back to idle, or no
// magic works until the game restarts (Radical Red's report: "not being able to use magic at all").
#include "TestMain.h"

#include "common/MagicRules.h"

using namespace coop;

namespace {

MagicHolders None() {
    return {};
}

MagicHolders Item() {
    MagicHolders h;
    h.item = true;
    return h;
}

} // namespace

TEST_CASE(MagicWaitingWithNothingToEndItIsOrphaned) {
    // The border flashes while an arrow, a spin attack or a song still has to reset it
    for (int s : { kMagicMeterFlash1, kMagicMeterFlash2, kMagicMeterFlash3 }) {
        CHECK(Magic_Orphaned(s, None()));
        CHECK(!Magic_Orphaned(s, Item()));
    }
    // Goron spikes and Zora barrier: only those forms end it
    MagicHolders form;
    form.goronOrZora = true;
    for (int s : { kMagicGoronZoraSetup, kMagicGoronZora }) {
        CHECK(Magic_Orphaned(s, None()));
        CHECK(Magic_Orphaned(s, Item()));
        CHECK(!Magic_Orphaned(s, form));
    }
    MagicHolders giant;
    giant.giantsMask = true;
    CHECK(Magic_Orphaned(kMagicGiantsMask, None()));
    CHECK(!Magic_Orphaned(kMagicGiantsMask, giant));
}

TEST_CASE(MagicStatesThatEndThemselvesAreNeverOrphaned) {
    // Idle, the consumption steps, the reset, the Lens (it checks itself) and the meter filling up
    for (int s : { kMagicIdle, kMagicConsumeSetup, kMagicConsume, kMagicReset, kMagicConsumeLens, kMagicStepCapacity,
                   kMagicFill }) {
        CHECK(!Magic_Orphaned(s, None()));
    }
    CHECK(!Magic_Orphaned(-1, None()));
    CHECK(!Magic_Orphaned(99, None()));
}

TEST_CASE(MagicGuardWaitsBeforeResetting) {
    MagicGuard g;
    for (int i = 0; i < kMagicStuckFrames - 1; i++) {
        CHECK(!g.Frame(true));
    }
    CHECK(g.Frame(true));  // three seconds with nothing to end it
    CHECK(!g.Frame(true)); // counted again from the reset
}

TEST_CASE(MagicGuardStartsAgainWhenSomethingHoldsIt) {
    MagicGuard g;
    for (int i = 0; i < kMagicStuckFrames - 1; i++) {
        CHECK(!g.Frame(true));
    }
    CHECK(!g.Frame(false)); // an arrow drawn again, for a single frame: the count starts over
    for (int i = 0; i < kMagicStuckFrames - 1; i++) {
        CHECK(!g.Frame(true));
    }
    CHECK(g.Frame(true));
}
