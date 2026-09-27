#pragma once
// Rules of the original game that the shared world has to apply outside of it: the game's own code only sees one
// player. The values mirror the game's headers and are checked against them in mm/2s2h/Coop/World/FieldTable.cpp.
#include <cstdint>

namespace coop::world {

constexpr uint8_t kItemSwordGreatFairy = 0x10; // ITEM_SWORD_GREAT_FAIRY
constexpr uint8_t kItemSwordGilded = 0x4F;     // ITEM_SWORD_GILDED
constexpr int kSlotSwordGreatFairy = 16;       // SLOT_SWORD_GREAT_FAIRY (inside the "items" field)
constexpr uint8_t kEquipSwordRazor = 2;        // EQUIP_VALUE_SWORD_RAZOR (the sword is the low nibble of equipment)
constexpr uint8_t kEquipSwordGilded = 3;       // EQUIP_VALUE_SWORD_GILDED

// Takkuri (z_en_thiefbird.c) can steal the Gilded Sword or the Great Fairy's Sword, both shared by the world. The
// Song of Time gives them back only in the robbed player's save (Sram_SaveEndOfCycle reads its stolenItems), but
// the new world can come from another game. Puts them back into the world fields "equipment" (2 bytes) and
// "items" (18 bytes) as those rules would. True if something changed.
bool ReturnStolenSwords(uint32_t stolenItems, uint8_t* equipment, uint8_t* items);

} // namespace coop::world
