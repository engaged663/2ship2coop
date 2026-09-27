#include "WorldRules.h"

namespace coop::world {

bool ReturnStolenSwords(uint32_t stolenItems, uint8_t* equipment, uint8_t* items) {
    const uint8_t stolen[2] = { (uint8_t)(stolenItems >> 24), (uint8_t)(stolenItems >> 16) }; // STOLEN_ITEM_1, _2
    bool changed = false;
    for (uint8_t item : stolen) {
        // As Sram_SaveEndOfCycle: a Razor Sword or less becomes the Gilded Sword if that one was stolen
        if (item >= kItemSwordGilded && (equipment[0] & 0x0F) <= kEquipSwordRazor) {
            equipment[0] = (uint8_t)((equipment[0] & 0xF0) | kEquipSwordGilded);
            changed = true;
        }
        if (item == kItemSwordGreatFairy && items[kSlotSwordGreatFairy] != kItemSwordGreatFairy) {
            items[kSlotSwordGreatFairy] = kItemSwordGreatFairy;
            changed = true;
        }
    }
    return changed;
}

} // namespace coop::world
