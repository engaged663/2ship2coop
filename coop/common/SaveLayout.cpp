#include "SaveLayout.h"

#include "WorldFields.h"

#include <cstring>

namespace coop::save {

namespace {

uint8_t* Field(FieldBytes& fields, const char* name) {
    return fields[(size_t)world::FindField(name)].data();
}

// Sets bits in a little-endian u32; true if any was missing.
bool SetBits(uint8_t* at, uint32_t bits) {
    uint32_t value = ReadU32(at);
    if ((value & bits) == bits) {
        return false;
    }
    WriteU32(at, value | bits);
    return true;
}

bool SetIfEmpty(uint8_t& slot, uint8_t item) {
    if (slot != kItemNone) {
        return false;
    }
    slot = item;
    return true;
}

} // namespace

uint8_t LocalWeekEventMask(int index) {
    uint8_t mask = 0;
    for (uint16_t flag : kLocalWeekEventFlags) {
        if ((flag >> 8) == index) {
            mask |= (uint8_t)(flag & 0xFF);
        }
    }
    return mask;
}

int FindPlayerField(const char* name) {
    for (size_t i = 0; i < kPlayerFieldCount; i++) {
        if (std::strcmp(kPlayerFields[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

FieldBytes EmptyFields() {
    FieldBytes fields;
    for (const world::FieldDef& def : world::kFields) {
        fields.emplace_back(def.size, (uint8_t)0);
    }
    return fields;
}

uint32_t ReadU32(const uint8_t* at) {
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

void WriteU32(uint8_t* at, uint32_t value) {
    at[0] = (uint8_t)(value & 0xFF);
    at[1] = (uint8_t)((value >> 8) & 0xFF);
    at[2] = (uint8_t)((value >> 16) & 0xFF);
    at[3] = (uint8_t)((value >> 24) & 0xFF);
}

uint32_t RepairUpgrades(uint32_t upgrades) {
    for (int i = 0; i < kUpgradeCount; i++) {
        uint32_t mask = ((1u << kUpgradeBits[i]) - 1) << kUpgradeShifts[i];
        if (((upgrades & mask) >> kUpgradeShifts[i]) > 3) { // past gUpgradeCapacities' four columns
            upgrades = (upgrades & ~mask) | (kNewSaveUpgrades & mask);
        }
    }
    return upgrades;
}

uint32_t RepairQuestItems(uint32_t questItems) {
    return questItems & ~((1u << kQuestSongSaria) | (1u << kQuestSongSun));
}

uint32_t ApplyCoopBaseline(FieldBytes& fields) {
    uint32_t added = 0;
    uint8_t* items = Field(fields, "items");
    uint8_t* masks = Field(fields, "masks");
    uint8_t* progress = Field(fields, "progress"); // isFirstCycle, snowheadCleared, hasTatl
    bool hadOcarina = items[kSlotOcarina] != kItemNone;
    if (SetIfEmpty(items[kSlotOcarina], kItemOcarinaOfTime)) {
        added |= BaselineOcarina;
    }
    if (SetIfEmpty(items[kSlotDekuNut], kItemDekuNut)) { // with no nuts left, as after the first cycle
        added |= BaselineDekuNut;
    }
    if (SetIfEmpty(masks[kSlotMaskDeku - kSlotMaskFirst], kItemMaskDeku)) {
        added |= BaselineDekuMask;
    }
    if (progress[2] == 0) {
        progress[2] = 1;
        added |= BaselineTatl;
    }
    if (!hadOcarina && progress[0] == 0) {
        progress[0] = 1;
        added |= BaselineFirstCycle;
    }
    uint8_t* quest = Field(fields, "quest");
    if (SetBits(quest, 1u << kQuestSongTime)) {
        added |= BaselineSongTime;
    }
    if (SetBits(quest, 1u << kQuestSongHealing)) {
        added |= BaselineSongHealing;
    }
    uint8_t* magic = Field(fields, "magicFlags"); // isMagicAcquired, isDoubleMagicAcquired
    if (magic[0] == 0) {
        magic[0] = 1;
        added |= BaselineMagic;
    }
    uint8_t* resets = Field(fields, "resets"); // u16: 0 would make the first Song of Time the original first one
    if (resets[0] == 0 && resets[1] == 0) {
        resets[0] = 1;
        added |= BaselineReset;
    }
    bool intro = false;
    uint8_t* week = Field(fields, "weekEventReg");
    for (uint16_t flag : kBaselineWeekEvents) {
        uint8_t mask = (uint8_t)(flag & 0xFF);
        if ((week[flag >> 8] & mask) != mask) {
            week[flag >> 8] |= mask;
            intro = true;
        }
    }
    uint8_t* scenes = Field(fields, "sceneFlags");
    intro = SetBits(scenes + kSceneClockTowerInterior * kCycleSceneFlagsSize + kCycleSwitch0, 1u << 0) || intro;
    intro = SetBits(scenes + kSceneOpeningDungeon * kCycleSceneFlagsSize + kCycleSwitch0, (1u << 2) | (1u << 0)) ||
            intro;
    intro = SetBits(scenes + kSceneOpeningDungeon * kCycleSceneFlagsSize + kCycleChest, 1u << 0) || intro;
    intro = SetBits(scenes + kSceneFairyFountain * kCycleSceneFlagsSize + kCycleSwitch0, 1u << 10) || intro;
    if (intro) {
        added |= BaselineIntro;
    }
    return added;
}

} // namespace coop::save
