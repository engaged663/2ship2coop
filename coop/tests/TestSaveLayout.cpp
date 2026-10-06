// SaveLayout: the game's save as the co-op sees it outside the game (common/SaveLayout.h).
#include "TestMain.h"

#include "common/SaveLayout.h"
#include "common/WorldFields.h"

#include <cstring>
#include <set>
#include <string>

using namespace coop;

namespace {

uint8_t* FieldOf(save::FieldBytes& f, const char* name) {
    int i = world::FindField(name);
    CHECK(i >= 0);
    return f[i].data();
}

// A world with nothing in it: inventory slots hold ITEM_NONE, as in a new save.
save::FieldBytes EmptyInventoryWorld() {
    save::FieldBytes f = save::EmptyFields();
    std::memset(FieldOf(f, "items"), save::kItemNone, 18);
    std::memset(FieldOf(f, "masks"), save::kItemNone, 24);
    return f;
}

constexpr uint32_t kEveryPart = save::BaselineTatl | save::BaselineOcarina | save::BaselineDekuNut |
                                save::BaselineDekuMask | save::BaselineSongTime | save::BaselineSongHealing |
                                save::BaselineMagic | save::BaselineReset | save::BaselineIntro |
                                save::BaselineFirstCycle;

} // namespace

TEST_CASE(PlayerFieldsHaveUniqueNamesAndSizes) {
    std::set<std::string> names;
    size_t total = 0;
    for (const save::PlayerFieldDef& f : save::kPlayerFields) {
        CHECK(f.size > 0);
        CHECK(names.insert(f.name).second);
        total += f.size;
    }
    CHECK_EQ(save::kPlayerFieldCount, (size_t)19);
    CHECK_EQ(total, (size_t)325);
    CHECK_EQ(save::FindPlayerField("horse"), 17);
    CHECK_EQ(save::FindPlayerField("nope"), -1);
}

TEST_CASE(LocalWeekEventMaskMatchesTheFlags) {
    CHECK_EQ(save::LocalWeekEventMask(92), (uint8_t)0x87); // a scene is loaded + Gorman's race state
    CHECK_EQ(save::LocalWeekEventMask(8), (uint8_t)0x01);
    CHECK_EQ(save::LocalWeekEventMask(63), (uint8_t)0x01);
    CHECK_EQ(save::LocalWeekEventMask(82), (uint8_t)0x08);
    CHECK_EQ(save::LocalWeekEventMask(90), (uint8_t)0x20);
    CHECK_EQ(save::LocalWeekEventMask(0), (uint8_t)0x00);
    CHECK_EQ(save::LocalWeekEventMask(99), (uint8_t)0x00);
}

TEST_CASE(BaselineFillsAnEmptyWorld) {
    save::FieldBytes f = EmptyInventoryWorld();
    CHECK_EQ(save::ApplyCoopBaseline(f), kEveryPart);
    const uint8_t* items = FieldOf(f, "items");
    CHECK_EQ(items[save::kSlotOcarina], save::kItemOcarinaOfTime);
    CHECK_EQ(items[save::kSlotDekuNut], save::kItemDekuNut);
    CHECK_EQ(items[1], save::kItemNone); // nothing more than the minimum
    CHECK_EQ(FieldOf(f, "masks")[save::kSlotMaskDeku - save::kSlotMaskFirst], save::kItemMaskDeku);
    const uint8_t* progress = FieldOf(f, "progress"); // isFirstCycle, snowheadCleared, hasTatl
    CHECK_EQ(progress[0], 1);
    CHECK_EQ(progress[1], 0);
    CHECK_EQ(progress[2], 1);
    CHECK_EQ(save::ReadU32(FieldOf(f, "quest")), (1u << 12) | (1u << 13));
    CHECK_EQ(FieldOf(f, "magicFlags")[0], 1);
    CHECK_EQ(FieldOf(f, "magicFlags")[1], 0);
    CHECK_EQ(FieldOf(f, "resets")[0], 1);
    CHECK_EQ(FieldOf(f, "resets")[1], 0);
    const uint8_t* week = FieldOf(f, "weekEventReg");
    CHECK_EQ(week[59], 0x04);
    CHECK_EQ(week[31], 0x04);
    CHECK_EQ(week[2], 0x38); // entered East, West and North Clock Town
    const uint8_t* scenes = FieldOf(f, "sceneFlags");
    CHECK_EQ(save::ReadU32(scenes + 0x1A * 20 + 0), 1u);                // before Clock Town: chest
    CHECK_EQ(save::ReadU32(scenes + 0x1A * 20 + 4), (1u << 2) | 1u);    // ...and switches
    CHECK_EQ(save::ReadU32(scenes + 0x63 * 20 + 4), 1u);                // clock tower interior
    CHECK_EQ(save::ReadU32(scenes + 0x26 * 20 + 4), 1u << 10);          // fairy's fountain
}

TEST_CASE(BaselineKeepsWhatIsThere) {
    save::FieldBytes f = EmptyInventoryWorld();
    FieldOf(f, "items")[1] = 0x01; // a bow the save already had
    save::ApplyCoopBaseline(f);
    save::FieldBytes once = f;
    CHECK_EQ(save::ApplyCoopBaseline(f), 0u);
    CHECK(f == once);
    CHECK_EQ(FieldOf(f, "items")[1], 0x01);
}

TEST_CASE(BaselineFirstCycleOnlyWithoutOcarina) {
    save::FieldBytes f = EmptyInventoryWorld();
    FieldOf(f, "items")[save::kSlotOcarina] = save::kItemOcarinaOfTime; // past the first cycle
    uint32_t added = save::ApplyCoopBaseline(f);
    CHECK((added & save::BaselineOcarina) == 0);
    CHECK((added & save::BaselineFirstCycle) == 0);
    CHECK_EQ(FieldOf(f, "progress")[0], 0);
}

TEST_CASE(RepairUndoesTheFirstUnlockAll) {
    // A world the first /unlockall touched (coop_pruebas' world.json): quiver 3, bomb bag 3, wallet 3, bullet bag 4,
    // Deku sticks 7, Deku nuts 7 -> the three that index past gUpgradeCapacities go back to a new save's 0, 1, 1
    CHECK_EQ(save::RepairUpgrades(0x007f301bu), 0x0012301bu);
    // Saria's song (bit 11) and the Sun's (bit 17) go; the Song of Time (12) and three heart pieces (28-31) stay
    CHECK_EQ(save::RepairQuestItems(0x30021800u), 0x30001000u);
}

TEST_CASE(RepairKeepsWhatASaveCanHave) {
    CHECK_EQ(save::RepairUpgrades(0x00120000u), 0x00120000u); // a new save: sticks 1, nuts 1
    CHECK_EQ(save::RepairUpgrades(0x0012201bu), 0x0012201bu); // quiver 3, bomb bag 3, giant wallet
    CHECK_EQ(save::RepairUpgrades(0x00360000u), 0x00360000u); // sticks 3 and nuts 3 (a save editor's)
    CHECK_EQ(save::RepairQuestItems(0x0001f7cfu), 0x0001f7cfu); // the remains and every song of this game
}
