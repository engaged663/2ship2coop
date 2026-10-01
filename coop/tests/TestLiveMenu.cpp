#include "TestMain.h"

#include "common/LiveMenu.h"

using namespace coop;

namespace {

LiveMenuView Idle() {
    return {};
}

LiveMenuView Text(uint16_t id) {
    LiveMenuView v;
    v.textOpen = true;
    v.textId = id;
    return v;
}

LiveMenuView Taken() {
    LiveMenuView v;
    v.taken = true;
    return v;
}

constexpr uint16_t kItemDescription = 0x1700; // a text of the menu itself
constexpr uint16_t kNpcText = 0x0200;         // a text the game opens

} // namespace

TEST_CASE(LiveMenuStaysOpenWhileNothingNeedsLink) {
    LiveMenuWatch watch;
    CHECK(!watch.IsOpen());
    watch.Opened(Idle());
    CHECK(watch.IsOpen());
    for (int frame = 0; frame < 100; frame++) {
        CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
        watch.AfterMenu(Idle());
    }
}

TEST_CASE(LiveMenuClosesOnSceneChange) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    LiveMenuView v;
    v.transition = true;
    CHECK(watch.AfterWorld(v) == LiveMenuClose::DropText);
}

TEST_CASE(LiveMenuClosesOnGameOver) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    LiveMenuView v;
    v.gameOver = true;
    CHECK(watch.AfterWorld(v) == LiveMenuClose::DropText);
}

TEST_CASE(LiveMenuClosesWhenTheGameTakesLink) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
    CHECK(watch.AfterWorld(Taken()) == LiveMenuClose::DropText);
}

// The owls' map of the Song of Soaring opens while Link plays the ocarina: that is not a reason to close it.
TEST_CASE(LiveMenuOpenedWhileLinkWasTakenStaysOpen) {
    LiveMenuWatch watch;
    watch.Opened(Taken());
    CHECK(watch.AfterWorld(Taken()) == LiveMenuClose::No);
    CHECK(watch.AfterWorld(Taken()) == LiveMenuClose::No);
    // Link is free again, and later taken again: that one closes it
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
    CHECK(watch.AfterWorld(Taken()) == LiveMenuClose::DropText);
}

TEST_CASE(LiveMenuKeepsItsOwnText) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
    watch.AfterMenu(Text(kItemDescription)); // the player asked for an item's description
    for (int frame = 0; frame < 10; frame++) {
        CHECK(watch.AfterWorld(Text(kItemDescription)) == LiveMenuClose::No);
        watch.AfterMenu(Text(kItemDescription));
    }
    watch.AfterMenu(Idle()); // closed by the player
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
}

TEST_CASE(LiveMenuClosesForATextOfTheGameAndKeepsIt) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    watch.AfterMenu(Idle());
    CHECK(watch.AfterWorld(Text(kNpcText)) == LiveMenuClose::KeepText);
}

TEST_CASE(LiveMenuClosesWhenTheGameReplacesItsText) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    watch.AfterMenu(Text(kItemDescription));
    CHECK(watch.AfterWorld(Text(kNpcText)) == LiveMenuClose::KeepText);
}

// A text opened after the menu's update (the hot spring water cooling down, from the HUD's update) is the game's too.
TEST_CASE(LiveMenuTextOpenedAfterItsUpdateIsTheGames) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
    watch.AfterMenu(Idle());
    // ...the game opens a text here, after the menu's update...
    CHECK(watch.AfterWorld(Text(0x00FA)) == LiveMenuClose::KeepText);
}

// The owls' map opens while the song's text is still closing: that text came with the menu.
TEST_CASE(LiveMenuTextOpenWhenItOpensIsItsOwn) {
    LiveMenuWatch watch;
    watch.Opened(Text(0x1B5A));
    CHECK(watch.AfterWorld(Text(0x1B5A)) == LiveMenuClose::No);
    watch.AfterMenu(Idle());
    CHECK(watch.AfterWorld(Idle()) == LiveMenuClose::No);
}

// A talk: the game takes Link and opens a text in the same frame. The text must survive.
TEST_CASE(LiveMenuKeepingTheGamesTextWinsOverDroppingIt) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    LiveMenuView v = Text(kNpcText);
    v.taken = true;
    v.transition = true;
    CHECK(watch.AfterWorld(v) == LiveMenuClose::KeepText);
}

TEST_CASE(LiveMenuDropsItsOwnTextWhenTheSceneChanges) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    watch.AfterMenu(Text(kItemDescription));
    LiveMenuView v = Text(kItemDescription);
    v.transition = true;
    CHECK(watch.AfterWorld(v) == LiveMenuClose::DropText);
}

TEST_CASE(LiveMenuClosedForgetsEverything) {
    LiveMenuWatch watch;
    watch.Opened(Idle());
    watch.AfterMenu(Text(kItemDescription));
    watch.Closed();
    CHECK(!watch.IsOpen());
    // A new menu: the old one's text is not this one's, and what is there when it opens is no reason to close it
    watch.Opened(Taken());
    CHECK(watch.AfterWorld(Taken()) == LiveMenuClose::No);
    CHECK(watch.AfterWorld(Text(kItemDescription)) == LiveMenuClose::KeepText);
}

// The game keeps its 20 frames a second with the menu open; the menu keeps its own 30 updates a second.
TEST_CASE(LiveMenuStepsThreeTimesEveryTwoFrames) {
    int steps = 0;
    for (uint32_t frame = 0; frame < 20; frame++) {
        int n = LiveMenu_Steps(frame);
        CHECK(n == 1 || n == 2);
        CHECK(n != LiveMenu_Steps(frame + 1)); // evenly spread: 2, 1, 2, 1...
        steps += n;
    }
    CHECK_EQ(steps, 30);
}
